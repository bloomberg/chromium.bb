// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/last_download_finder.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/history/chrome_history_client.h"
#include "chrome/browser/history/chrome_history_client_factory.h"
#include "chrome/browser/history/download_row.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/web_history_service_factory.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A BrowserContextKeyedServiceFactory::TestingFactoryFunction that creates a
// HistoryService for a TestingProfile.
KeyedService* BuildHistoryService(content::BrowserContext* context) {
  TestingProfile* profile = static_cast<TestingProfile*>(context);

  // Delete the file before creating the service.
  base::FilePath history_path(
      profile->GetPath().Append(chrome::kHistoryFilename));
  if (!base::DeleteFile(history_path, false) ||
      base::PathExists(history_path)) {
    ADD_FAILURE() << "failed to delete history db file "
                  << history_path.value();
    return NULL;
  }

  HistoryService* history_service = new HistoryService(
      ChromeHistoryClientFactory::GetForProfile(profile), profile);
  if (history_service->Init(profile->GetPath()))
    return history_service;

  ADD_FAILURE() << "failed to initialize history service";
  delete history_service;
  return NULL;
}

}  // namespace

class LastDownloadFinderTest : public testing::Test {
 public:
  void NeverCalled(scoped_ptr<
      safe_browsing::ClientIncidentReport_DownloadDetails> download) {
    FAIL();
  }

  // Creates a new profile that participates in safe browsing and adds a
  // download to its history.
  void CreateProfileWithDownload() {
    TestingProfile* profile = CreateProfile(SAFE_BROWSING_OPT_IN);
    HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS);
    history_service->CreateDownload(
        CreateTestDownloadRow(),
        base::Bind(&LastDownloadFinderTest::OnDownloadCreated,
                   base::Unretained(this)));
  }

  // safe_browsing::LastDownloadFinder::LastDownloadCallback implementation that
  // passes the found download to |result| and then runs a closure.
  void OnLastDownload(
      scoped_ptr<safe_browsing::ClientIncidentReport_DownloadDetails>* result,
      const base::Closure& quit_closure,
      scoped_ptr<safe_browsing::ClientIncidentReport_DownloadDetails>
          download) {
    *result = download.Pass();
    quit_closure.Run();
  }

 protected:
  // A type for specifying whether or not a profile created by CreateProfile
  // participates in safe browsing.
  enum SafeBrowsingDisposition {
    SAFE_BROWSING_OPT_OUT,
    SAFE_BROWSING_OPT_IN,
  };

  LastDownloadFinderTest() : profile_number_() {}

  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
  }

  virtual void TearDown() OVERRIDE {
    // Shut down the history service on all profiles.
    std::vector<Profile*> profiles(
        profile_manager_->profile_manager()->GetLoadedProfiles());
    for (size_t i = 0; i < profiles.size(); ++i) {
      profiles[0]->AsTestingProfile()->DestroyHistoryService();
    }
    profile_manager_.reset();
    TestingBrowserProcess::DeleteInstance();
    testing::Test::TearDown();
  }

  TestingProfile* CreateProfile(SafeBrowsingDisposition safe_browsing_opt_in) {
    std::string profile_name("profile");
    profile_name.append(base::IntToString(++profile_number_));

    // Set up keyed service factories.
    TestingProfile::TestingFactories factories;
    // Build up a custom history service.
    factories.push_back(std::make_pair(HistoryServiceFactory::GetInstance(),
                                       &BuildHistoryService));
    // Suppress WebHistoryService since it makes network requests.
    factories.push_back(std::make_pair(
        WebHistoryServiceFactory::GetInstance(),
        static_cast<BrowserContextKeyedServiceFactory::TestingFactoryFunction>(
            NULL)));

    // Create prefs for the profile with safe browsing enabled or not.
    scoped_ptr<TestingPrefServiceSyncable> prefs(
        new TestingPrefServiceSyncable);
    chrome::RegisterUserProfilePrefs(prefs->registry());
    prefs->SetBoolean(prefs::kSafeBrowsingEnabled,
                      safe_browsing_opt_in == SAFE_BROWSING_OPT_IN);

    TestingProfile* profile = profile_manager_->CreateTestingProfile(
        profile_name,
        prefs.PassAs<PrefServiceSyncable>(),
        base::UTF8ToUTF16(profile_name),  // user_name
        0,                                // avatar_id
        std::string(),                    // supervised_user_id
        factories);

    return profile;
  }

  void AddDownload(Profile* profile, const history::DownloadRow& download) {
    base::RunLoop run_loop;

    HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS);
    history_service->CreateDownload(
        download,
        base::Bind(&LastDownloadFinderTest::ContinueOnDownloadCreated,
                   base::Unretained(this),
                   run_loop.QuitClosure()));
    run_loop.Run();
  }

  // Wait for the history backend thread to process any outstanding tasks.
  // This is needed because HistoryService::QueryDownloads uses PostTaskAndReply
  // to do work on the backend thread and then invoke the caller's callback on
  // the originating thread. The PostTaskAndReplyRelay holds a reference to the
  // backend until its RunReplyAndSelfDestruct is called on the originating
  // thread. This reference MUST be released (on the originating thread,
  // remember) _before_ calling DestroyHistoryService in TearDown(). See the
  // giant comment in HistoryService::Cleanup explaining where the backend's
  // dtor must be run.
  void FlushHistoryBackend(Profile* profile) {
    base::RunLoop run_loop;
    HistoryServiceFactory::GetForProfile(profile, Profile::EXPLICIT_ACCESS)
        ->FlushForTest(run_loop.QuitClosure());
    run_loop.Run();
    // Then make sure anything bounced back to the main thread has been handled.
    base::RunLoop().RunUntilIdle();
  }

  // Runs the last download finder on all loaded profiles, returning the found
  // download or an empty pointer if none was found.
  scoped_ptr<safe_browsing::ClientIncidentReport_DownloadDetails>
  RunLastDownloadFinder() {
    base::RunLoop run_loop;

    scoped_ptr<safe_browsing::ClientIncidentReport_DownloadDetails>
        last_download;

    scoped_ptr<safe_browsing::LastDownloadFinder> finder(
        safe_browsing::LastDownloadFinder::Create(
            base::Bind(&LastDownloadFinderTest::OnLastDownload,
                       base::Unretained(this),
                       &last_download,
                       run_loop.QuitClosure())));

    if (finder)
      run_loop.Run();

    return last_download.Pass();
  }

  history::DownloadRow CreateTestDownloadRow() {
    base::Time now(base::Time::Now());
    return history::DownloadRow(
        base::FilePath(FILE_PATH_LITERAL("spam.exe")),
        base::FilePath(FILE_PATH_LITERAL("spam.exe")),
        std::vector<GURL>(1, GURL("http://www.google.com")),  // url_chain
        GURL(),                                               // referrer
        "application/octet-stream",                           // mime_type
        "application/octet-stream",                   // original_mime_type
        now - base::TimeDelta::FromMinutes(10),       // start
        now - base::TimeDelta::FromMinutes(9),        // end
        std::string(),                                // etag
        std::string(),                                // last_modified
        47LL,                                         // received
        47LL,                                         // total
        content::DownloadItem::COMPLETE,              // download_state
        content::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,  // danger_type
        content::DOWNLOAD_INTERRUPT_REASON_NONE,      // interrupt_reason,
        1,                                            // id
        false,                                        // download_opened
        std::string(),                                // ext_id
        std::string());                               // ext_name
  }

  void ExpectNoDownloadFound(scoped_ptr<
      safe_browsing::ClientIncidentReport_DownloadDetails> download) {
    EXPECT_FALSE(download);
  }

  void ExpectFoundTestDownload(scoped_ptr<
      safe_browsing::ClientIncidentReport_DownloadDetails> download) {
    ASSERT_TRUE(download);
  }

  content::TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<TestingProfileManager> profile_manager_;

 private:
  // A HistoryService::DownloadCreateCallback that asserts that the download was
  // created and runs |closure|.
  void ContinueOnDownloadCreated(const base::Closure& closure, bool created) {
    ASSERT_TRUE(created);
    closure.Run();
  }

  // A HistoryService::DownloadCreateCallback that asserts that the download was
  // created.
  void OnDownloadCreated(bool created) { ASSERT_TRUE(created); }

  int profile_number_;
};

// Tests that nothing happens if there are no profiles at all.
TEST_F(LastDownloadFinderTest, NoProfiles) {
  ExpectNoDownloadFound(RunLastDownloadFinder());
}

// Tests that nothing happens other than the callback being invoked if there are
// no profiles participating in safe browsing.
TEST_F(LastDownloadFinderTest, NoParticipatingProfiles) {
  // Create a profile with a history service that is opted-out
  TestingProfile* profile = CreateProfile(SAFE_BROWSING_OPT_OUT);

  // Add a download.
  AddDownload(profile, CreateTestDownloadRow());

  ExpectNoDownloadFound(RunLastDownloadFinder());
}

// Tests that a download is found from a single profile.
TEST_F(LastDownloadFinderTest, SimpleEndToEnd) {
  // Create a profile with a history service that is opted-in.
  TestingProfile* profile = CreateProfile(SAFE_BROWSING_OPT_IN);

  // Add a download.
  AddDownload(profile, CreateTestDownloadRow());

  ExpectFoundTestDownload(RunLastDownloadFinder());
}

// Tests that there is no crash if the finder is deleted before results arrive.
TEST_F(LastDownloadFinderTest, DeleteBeforeResults) {
  // Create a profile with a history service that is opted-in.
  TestingProfile* profile = CreateProfile(SAFE_BROWSING_OPT_IN);

  // Add a download.
  AddDownload(profile, CreateTestDownloadRow());

  // Start a finder and kill it before the search completes.
  safe_browsing::LastDownloadFinder::Create(
      base::Bind(&LastDownloadFinderTest::NeverCalled, base::Unretained(this)))
      .reset();

  // Flush tasks on the history backend thread.
  FlushHistoryBackend(profile);
}

// Tests that a download in profile added after the search is begun is found.
TEST_F(LastDownloadFinderTest, AddProfileAfterStarting) {
  // Create a profile with a history service that is opted-in.
  CreateProfile(SAFE_BROWSING_OPT_IN);

  scoped_ptr<safe_browsing::ClientIncidentReport_DownloadDetails> last_download;
  base::RunLoop run_loop;

  // Post a task that will create a second profile once the main loop is run.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&LastDownloadFinderTest::CreateProfileWithDownload,
                 base::Unretained(this)));

  // Create a finder that we expect will find a download in the second profile.
  scoped_ptr<safe_browsing::LastDownloadFinder> finder(
      safe_browsing::LastDownloadFinder::Create(
          base::Bind(&LastDownloadFinderTest::OnLastDownload,
                     base::Unretained(this),
                     &last_download,
                     run_loop.QuitClosure())));

  run_loop.Run();

  ExpectFoundTestDownload(last_download.Pass());
}
