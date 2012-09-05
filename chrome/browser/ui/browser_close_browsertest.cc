// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_service.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/download/download_test_file_chooser_observer.h"
#include "chrome/browser/net/url_request_mock_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/download_item.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/test/net/url_request_slow_download_job.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;

class BrowserCloseTest : public InProcessBrowserTest {
 public:
  // Structure defining test cases for DownloadsCloseCheck.
  struct DownloadsCloseCheckCase {
    std::string DebugString() const;

    // Input
    struct {
      struct {
        int windows;
        int downloads;
      } regular;
      struct {
        int windows;
        int downloads;
      } incognito;
    } profile_a;

    struct {
      struct {
        int windows;
        int downloads;
      } regular;
      struct {
        int windows;
        int downloads;
      } incognito;
    } profile_b;

    // We always probe a window in profile A.
    enum { REGULAR = 0, INCOGNITO = 1 } window_to_probe;

    // Output
    Browser::DownloadClosePreventionType type;

    // Unchecked if type == DOWNLOAD_CLOSE_OK.
    int num_blocking;
  };

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&chrome_browser_net::SetUrlRequestMocksEnabled, true));
  }

  // Create a second profile to work within multi-profile.
  Profile* CreateSecondProfile() {
    FilePath user_data_dir;
    PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    if (!second_profile_data_dir_.CreateUniqueTempDirUnderPath(user_data_dir))
      return NULL;

    Profile* second_profile =
        g_browser_process->profile_manager()->GetProfile(
            second_profile_data_dir_.path());
    if (!second_profile)
      return NULL;

    bool result = second_profile_downloads_dir_.CreateUniqueTempDir();
    if (!result)
      return NULL;
    second_profile->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        second_profile_downloads_dir_.path());

    return second_profile;
  }

  // Create |num_downloads| number of downloads that are stalled
  // (will quickly get to a place where the server won't
  // provide any more data) so that we can test closing the
  // browser with active downloads.
  void CreateStalledDownloads(Browser* browser, int num_downloads) {
    GURL url(URLRequestSlowDownloadJob::kKnownSizeUrl);

    if (num_downloads == 0)
      return;

    // Setup an observer waiting for the given number of downloads
    // to get to IN_PROGRESS.
    DownloadManager* download_manager =
        BrowserContext::GetDownloadManager(browser->profile());
    scoped_ptr<content::DownloadTestObserver> observer(
        new content::DownloadTestObserverInProgress(download_manager,
                                                    num_downloads));

    // Set of that number of downloads.
    size_t count_downloads = num_downloads;
    while (num_downloads--)
      ui_test_utils::NavigateToURLWithDisposition(
          browser, url, NEW_BACKGROUND_TAB,
          ui_test_utils::BROWSER_TEST_NONE);

    // Wait for them.
    observer->WaitForFinished();
    EXPECT_EQ(count_downloads,
              observer->NumDownloadsSeenInState(DownloadItem::IN_PROGRESS));
  }

  // All all downloads created in CreateStalledDownloads() to
  // complete, and block in this routine until they do complete.
  void CompleteAllDownloads(Browser* browser) {
    GURL finish_url(URLRequestSlowDownloadJob::kFinishDownloadUrl);
    ui_test_utils::NavigateToURL(browser, finish_url);

    // Go through and, for every single profile, wait until there are
    // no active downloads on that download manager.
    std::vector<Profile*> profiles(
        g_browser_process->profile_manager()->GetLoadedProfiles());
    for (std::vector<Profile*>::const_iterator pit = profiles.begin();
         pit != profiles.end(); ++pit) {
      DownloadService* download_service =
          DownloadServiceFactory::GetForProfile(*pit);
      if (download_service->HasCreatedDownloadManager()) {
        DownloadManager *mgr = BrowserContext::GetDownloadManager(*pit);
        scoped_refptr<content::DownloadTestFlushObserver> observer(
            new content::DownloadTestFlushObserver(mgr));
        observer->WaitForFlush();
      }
      if ((*pit)->HasOffTheRecordProfile()) {
        DownloadService* incognito_download_service =
          DownloadServiceFactory::GetForProfile(
              (*pit)->GetOffTheRecordProfile());
        if (incognito_download_service->HasCreatedDownloadManager()) {
          DownloadManager *mgr = BrowserContext::GetDownloadManager(
              (*pit)->GetOffTheRecordProfile());
          scoped_refptr<content::DownloadTestFlushObserver> observer(
              new content::DownloadTestFlushObserver(mgr));
          observer->WaitForFlush();
        }
      }
    }
  }

  // Create a Browser (with associated window) on the specified profile.
  Browser* CreateBrowserOnProfile(Profile* profile) {
    Browser* new_browser = new Browser(Browser::CreateParams(profile));
    chrome::AddSelectedTabWithURL(new_browser, GURL(chrome::kAboutBlankURL),
                                  content::PAGE_TRANSITION_AUTO_TOPLEVEL);
    content::WaitForLoadStop(chrome::GetActiveWebContents(new_browser));
    new_browser->window()->Show();
    return new_browser;
  }

  // Adjust the number of browsers and associated windows up or down
  // to |num_windows|.  This routine assumes that there is only a single
  // browser associated with the profile on entry.  |*base_browser| contains
  // this browser, and the profile is derived from that browser.  On output,
  // if |*base_browser| was destroyed (because |num_windows == 0|), NULL
  // is assigned to that memory location.
  bool AdjustBrowsersOnProfile(Browser** base_browser, int num_windows) {
    int num_downloads_blocking;
    if (num_windows == 0) {
      if (Browser::DOWNLOAD_CLOSE_OK !=
          (*base_browser)->OkToCloseWithInProgressDownloads(
              &num_downloads_blocking))
        return false;
      (*base_browser)->window()->Close();
      *base_browser = 0;
      return true;
    }

    // num_windows > 0
    Profile* profile((*base_browser)->profile());
    for (int w = 1; w < num_windows; ++w) {
      CreateBrowserOnProfile(profile);
    }
    return true;
  }

  int TotalUnclosedBrowsers() {
    int count = 0;
    for (BrowserList::const_iterator iter = BrowserList::begin();
         iter != BrowserList::end(); ++iter)
      if (!(*iter)->IsAttemptingToCloseBrowser()) {
        count++;
      }
    return count;
  }

  // Note that this is invalid to call if TotalUnclosedBrowsers() == 0.
  Browser* FirstUnclosedBrowser() {
    for (BrowserList::const_iterator iter = BrowserList::begin();
         iter != BrowserList::end(); ++iter)
      if (!(*iter)->IsAttemptingToCloseBrowser())
        return (*iter);
    return NULL;
  }

  bool SetupForDownloadCloseCheck() {
    first_profile_ = browser()->profile();

    bool result = first_profile_downloads_dir_.CreateUniqueTempDir();
    EXPECT_TRUE(result);
    if (!result) return false;
    first_profile_->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        first_profile_downloads_dir_.path());

    second_profile_ = CreateSecondProfile();
    EXPECT_TRUE(second_profile_);
    if (!second_profile_) return false;

    DownloadTestFileChooserObserver(first_profile_) .EnableFileChooser(false);
    DownloadTestFileChooserObserver(second_profile_).EnableFileChooser(false);
    return true;
  }

  // Test a specific DownloadsCloseCheckCase.  Returns false if
  // an assertion has failed and the test should be aborted.
  bool ExecuteDownloadCloseCheckCase(size_t i) {
    const DownloadsCloseCheckCase& check_case(download_close_check_cases[i]);

    // Test invariant: so that we don't actually try and close the browser,
    // we always enter the function with a single browser window open on the
    // main profile.  That means we need to exit the function the same way.
    // So we setup everything except for the |first_profile_| regular, and then
    // flip the bit on the main window.
    // Note that this means that browser() is unreliable in the context
    // of this function or its callers; we'll be killing that main window
    // and recreating it fairly frequently.
    int unclosed_browsers = TotalUnclosedBrowsers();
    EXPECT_EQ(1, unclosed_browsers);
    if (1 != unclosed_browsers)
      return false;

    Browser* entry_browser = FirstUnclosedBrowser();
    EXPECT_EQ(first_profile_, entry_browser->profile())
        << "Case" << i
        << ": " << check_case.DebugString();
    if (first_profile_ != entry_browser->profile())
      return false;
    int total_download_count = DownloadService::DownloadCountAllProfiles();
    EXPECT_EQ(0, total_download_count)
        << "Case " << i
        << ": " << check_case.DebugString();
    if (0 != total_download_count)
      return false;

    Profile* first_profile_incognito = first_profile_->GetOffTheRecordProfile();
    Profile* second_profile_incognito =
        second_profile_->GetOffTheRecordProfile();
    DownloadTestFileChooserObserver(first_profile_incognito)
        .EnableFileChooser(false);
    DownloadTestFileChooserObserver(second_profile_incognito)
        .EnableFileChooser(false);

    // For simplicty of coding, we create a window on each profile so that
    // we can easily create downloads, then we destroy or create windows
    // as necessary.
    Browser* browser_a_regular(CreateBrowserOnProfile(first_profile_));
    Browser* browser_a_incognito(
        CreateBrowserOnProfile(first_profile_incognito));
    Browser* browser_b_regular(CreateBrowserOnProfile(second_profile_));
    Browser* browser_b_incognito(
        CreateBrowserOnProfile(second_profile_incognito));

    // Kill our entry browser.
    entry_browser->window()->Close();
    entry_browser = NULL;

    // Create all downloads needed.
    CreateStalledDownloads(
        browser_a_regular, check_case.profile_a.regular.downloads);
    CreateStalledDownloads(
        browser_a_incognito, check_case.profile_a.incognito.downloads);
    CreateStalledDownloads(
        browser_b_regular, check_case.profile_b.regular.downloads);
    CreateStalledDownloads(
        browser_b_incognito, check_case.profile_b.incognito.downloads);

    // Adjust the windows
    Browser** browsers[] = {
      &browser_a_regular, &browser_a_incognito,
      &browser_b_regular, &browser_b_incognito
    };
    int window_counts[] = {
      check_case.profile_a.regular.windows,
      check_case.profile_a.incognito.windows,
      check_case.profile_b.regular.windows,
      check_case.profile_b.incognito.windows,
    };
    for (size_t j = 0; j < arraysize(browsers); ++j) {
      bool result = AdjustBrowsersOnProfile(browsers[j], window_counts[j]);
      EXPECT_TRUE(result);
      if (!result)
        return false;
    }
    content::RunAllPendingInMessageLoop();

    // All that work, for this one little test.
    EXPECT_TRUE((check_case.window_to_probe ==
                 DownloadsCloseCheckCase::REGULAR) ||
                (check_case.window_to_probe ==
                 DownloadsCloseCheckCase::INCOGNITO));
    if (!((check_case.window_to_probe ==
           DownloadsCloseCheckCase::REGULAR) ||
          (check_case.window_to_probe ==
           DownloadsCloseCheckCase::INCOGNITO)))
      return false;

    int num_downloads_blocking;
    Browser* browser_to_probe =
        (check_case.window_to_probe == DownloadsCloseCheckCase::REGULAR ?
         browser_a_regular :
         browser_a_incognito);
    Browser::DownloadClosePreventionType type =
        browser_to_probe->OkToCloseWithInProgressDownloads(
            &num_downloads_blocking);
    EXPECT_EQ(check_case.type, type) << "Case " << i
                                     << ": " << check_case.DebugString();
    if (type != Browser::DOWNLOAD_CLOSE_OK)
      EXPECT_EQ(check_case.num_blocking, num_downloads_blocking)
          << "Case " << i
          << ": " << check_case.DebugString();

    // Release all the downloads.
    CompleteAllDownloads(browser_to_probe);

    // Create a new main window and kill everything else.
    entry_browser = CreateBrowserOnProfile(first_profile_);
    for (BrowserList::const_iterator bit = BrowserList::begin();
         bit != BrowserList::end(); ++bit) {
      if ((*bit) != entry_browser) {
        EXPECT_TRUE((*bit)->window());
        if (!(*bit)->window())
          return false;
        (*bit)->window()->Close();
      }
    }
    content::RunAllPendingInMessageLoop();

    return true;
  }

  static const DownloadsCloseCheckCase download_close_check_cases[];

  // DownloadCloseCheck variables.
  Profile* first_profile_;
  Profile* second_profile_;

  ScopedTempDir first_profile_downloads_dir_;
  ScopedTempDir second_profile_data_dir_;
  ScopedTempDir second_profile_downloads_dir_;
};

const BrowserCloseTest::DownloadsCloseCheckCase
BrowserCloseTest::download_close_check_cases[] = {
  // Top level nesting is {profile_a, profile_b}
  // Second level nesting is {regular, incognito
  // Third level (inner) nesting is {windows, downloads}

  // Last window (incognito) triggers browser close warning.
  {{{0, 0}, {1, 1}}, {{0, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::INCOGNITO,
   Browser::DOWNLOAD_CLOSE_BROWSER_SHUTDOWN, 1},

  // Last incognito window triggers incognito close warning.
  {{{1, 0}, {1, 1}}, {{0, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::INCOGNITO,
   Browser::DOWNLOAD_CLOSE_LAST_WINDOW_IN_INCOGNITO_PROFILE, 1},

  // Last incognito window with no downloads triggers no warning.
  {{{0, 0}, {1, 0}}, {{0, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::INCOGNITO,
   Browser::DOWNLOAD_CLOSE_OK},

  // Last incognito window with window+download on another incognito profile
  // triggers no warning.
  {{{0, 0}, {1, 0}}, {{0, 0}, {1, 1}},
   BrowserCloseTest::DownloadsCloseCheckCase::INCOGNITO,
   Browser::DOWNLOAD_CLOSE_OK},

  // Non-last incognito window triggers no warning.
  {{{0, 0}, {2, 1}}, {{0, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::INCOGNITO,
   Browser::DOWNLOAD_CLOSE_OK},

  // Non-last regular window triggers no warning.
  {{{2, 1}, {0, 0}}, {{0, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::REGULAR,
   Browser::DOWNLOAD_CLOSE_OK},

  // Last regular window triggers browser close.
  {{{1, 1}, {0, 0}}, {{0, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::REGULAR,
   Browser::DOWNLOAD_CLOSE_BROWSER_SHUTDOWN, 1},

  // Last regular window triggers browser close for download on different
  // profile.
  {{{1, 0}, {0, 0}}, {{0, 1}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::REGULAR,
   Browser::DOWNLOAD_CLOSE_BROWSER_SHUTDOWN, 1},

  // Last regular window triggers no warning if incognito
  // active (http://crbug.com/61257).
  {{{1, 0}, {1, 1}}, {{0, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::REGULAR,
   Browser::DOWNLOAD_CLOSE_OK},

  // Last regular window triggers no warning if other profile window active.
  {{{1, 1}, {0, 0}}, {{1, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::REGULAR,
   Browser::DOWNLOAD_CLOSE_OK},

  // Last regular window triggers no warning if other incognito window
  // active.
  {{{1, 0}, {0, 0}}, {{0, 0}, {1, 1}},
   BrowserCloseTest::DownloadsCloseCheckCase::REGULAR,
   Browser::DOWNLOAD_CLOSE_OK},

  // Last regular window triggers no warning if incognito active.
  {{{1, 1}, {1, 0}}, {{0, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::REGULAR,
   Browser::DOWNLOAD_CLOSE_OK},

  // Test plural for regular.
  {{{1, 2}, {0, 0}}, {{0, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::REGULAR,
   Browser::DOWNLOAD_CLOSE_BROWSER_SHUTDOWN, 2},

  // Test plural for incognito.
  {{{1, 0}, {1, 2}}, {{0, 0}, {0, 0}},
   BrowserCloseTest::DownloadsCloseCheckCase::INCOGNITO,
   Browser::DOWNLOAD_CLOSE_LAST_WINDOW_IN_INCOGNITO_PROFILE, 2},
};

std::string BrowserCloseTest::DownloadsCloseCheckCase::DebugString() const {
  std::string result;
  result += "{";
  if (profile_a.regular.windows || profile_a.regular.downloads)
    result += base::StringPrintf("Regular profile A: (%d w, %d d), ",
                                 profile_a.regular.windows,
                                 profile_a.regular.downloads);
  if (profile_a.incognito.windows || profile_a.incognito.downloads)
    result += base::StringPrintf("Incognito profile A: (%d w, %d d), ",
                                 profile_a.incognito.windows,
                                 profile_a.incognito.downloads);
  if (profile_b.regular.windows || profile_b.regular.downloads)
    result += base::StringPrintf("Regular profile B: (%d w, %d d), ",
                                 profile_b.regular.windows,
                                 profile_b.regular.downloads);
  if (profile_b.incognito.windows || profile_b.incognito.downloads)
    result += base::StringPrintf("Incognito profile B: (%d w, %d d), ",
                                 profile_b.incognito.windows,
                                 profile_b.incognito.downloads);
  result += (window_to_probe == REGULAR ? "Probe regular" :
             window_to_probe == INCOGNITO ? "Probe incognito" :
             "Probe unknown");
  result += "} -> ";
  if (type == Browser::DOWNLOAD_CLOSE_OK) {
    result += "No warning";
  } else {
    result += base::StringPrintf(
        "%s (%d downloads) warning",
        (type == Browser::DOWNLOAD_CLOSE_BROWSER_SHUTDOWN ? "Browser shutdown" :
         type == Browser::DOWNLOAD_CLOSE_LAST_WINDOW_IN_INCOGNITO_PROFILE ?
         "Incognito close" : "Unknown"),
        num_blocking);
  }
  return result;
}

// The following test is split into six chunks to reduce the chance
// of hitting the 25s timeout.

// This test is timing out very often under AddressSanitizer.
// http://crbug.com/111914 and http://crbug.com/103371.
#if defined(ADDRESS_SANITIZER)

#define MAYBE_DownloadsCloseCheck_0 DISABLED_DownloadsCloseCheck_0
#define MAYBE_DownloadsCloseCheck_1 DISABLED_DownloadsCloseCheck_1
#define MAYBE_DownloadsCloseCheck_2 DISABLED_DownloadsCloseCheck_2
#define MAYBE_DownloadsCloseCheck_3 DISABLED_DownloadsCloseCheck_3
#define MAYBE_DownloadsCloseCheck_4 DISABLED_DownloadsCloseCheck_4
#define MAYBE_DownloadsCloseCheck_5 DISABLED_DownloadsCloseCheck_5

#else

// Crashing on Linux. http://crbug.com/100566
#if defined(OS_LINUX)
#define MAYBE_DownloadsCloseCheck_0 DISABLED_DownloadsCloseCheck_0
#define MAYBE_DownloadsCloseCheck_1 DISABLED_DownloadsCloseCheck_1
#else
#define MAYBE_DownloadsCloseCheck_0 DownloadsCloseCheck_0
#define MAYBE_DownloadsCloseCheck_1 DownloadsCloseCheck_1
#endif

#define MAYBE_DownloadsCloseCheck_3 DownloadsCloseCheck_3
#define MAYBE_DownloadsCloseCheck_4 DownloadsCloseCheck_4
// Timing out on XP debug. http://crbug.com/111914
#if defined(OS_WIN)
# define MAYBE_DownloadsCloseCheck_2 DISABLED_DownloadsCloseCheck_2
# define MAYBE_DownloadsCloseCheck_5 DISABLED_DownloadsCloseCheck_5
#else
# define MAYBE_DownloadsCloseCheck_2 DownloadsCloseCheck_2
# define MAYBE_DownloadsCloseCheck_5 DownloadsCloseCheck_5
#endif  // defined(OS_WIN)

#endif  // defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_F(BrowserCloseTest, MAYBE_DownloadsCloseCheck_0) {
  ASSERT_TRUE(SetupForDownloadCloseCheck());
  for (size_t i = 0; i < arraysize(download_close_check_cases) / 6; ++i) {
    ExecuteDownloadCloseCheckCase(i);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserCloseTest, MAYBE_DownloadsCloseCheck_1) {
  ASSERT_TRUE(SetupForDownloadCloseCheck());
  for (size_t i = arraysize(download_close_check_cases) / 6;
       i < 2 * arraysize(download_close_check_cases) / 6; ++i) {
    ExecuteDownloadCloseCheckCase(i);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserCloseTest, MAYBE_DownloadsCloseCheck_2) {
  ASSERT_TRUE(SetupForDownloadCloseCheck());
  for (size_t i = 2 * arraysize(download_close_check_cases) / 6;
       i < 3 * arraysize(download_close_check_cases) / 6; ++i) {
    ExecuteDownloadCloseCheckCase(i);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserCloseTest, MAYBE_DownloadsCloseCheck_3) {
  ASSERT_TRUE(SetupForDownloadCloseCheck());
  for (size_t i = 3 * arraysize(download_close_check_cases) / 6;
       i < 4 * arraysize(download_close_check_cases) / 6; ++i) {
    ExecuteDownloadCloseCheckCase(i);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserCloseTest, MAYBE_DownloadsCloseCheck_4) {
  ASSERT_TRUE(SetupForDownloadCloseCheck());
  for (size_t i = 4 * arraysize(download_close_check_cases) / 6;
       i < 5 * arraysize(download_close_check_cases) / 6; ++i) {
    ExecuteDownloadCloseCheckCase(i);
  }
}

IN_PROC_BROWSER_TEST_F(BrowserCloseTest, MAYBE_DownloadsCloseCheck_5) {
  ASSERT_TRUE(SetupForDownloadCloseCheck());
  for (size_t i = 5 * arraysize(download_close_check_cases) / 6;
       i < 6 * arraysize(download_close_check_cases) / 6; ++i) {
    ExecuteDownloadCloseCheckCase(i);
  }
}
