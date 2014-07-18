// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ui_test_utils.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/autocomplete/autocomplete_controller.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog_queue.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_iterator.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/find_in_page_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/geoposition.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/test/python_utils.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"
#include "ui/snapshot/test/snapshot_desktop.h"

#if defined(USE_AURA)
#include "ash/shell.h"
#include "ui/aura/window_event_dispatcher.h"
#endif

using content::DomOperationNotificationDetails;
using content::NativeWebKeyboardEvent;
using content::NavigationController;
using content::NavigationEntry;
using content::OpenURLParams;
using content::RenderViewHost;
using content::RenderWidgetHost;
using content::Referrer;
using content::WebContents;

namespace ui_test_utils {

namespace {

#if defined(OS_WIN)
const char kSnapshotBaseName[] = "ChromiumSnapshot";
const char kSnapshotExtension[] = ".png";

base::FilePath GetSnapshotFileName(const base::FilePath& snapshot_directory) {
  base::Time::Exploded the_time;

  base::Time::Now().LocalExplode(&the_time);
  std::string filename(base::StringPrintf("%s%04d%02d%02d%02d%02d%02d%s",
      kSnapshotBaseName, the_time.year, the_time.month, the_time.day_of_month,
      the_time.hour, the_time.minute, the_time.second, kSnapshotExtension));

  base::FilePath snapshot_file = snapshot_directory.AppendASCII(filename);
  if (base::PathExists(snapshot_file)) {
    int index = 0;
    std::string suffix;
    base::FilePath trial_file;
    do {
      suffix = base::StringPrintf(" (%d)", ++index);
      trial_file = snapshot_file.InsertBeforeExtensionASCII(suffix);
    } while (base::PathExists(trial_file));
    snapshot_file = trial_file;
  }
  return snapshot_file;
}
#endif  // defined(OS_WIN)

Browser* WaitForBrowserNotInSet(std::set<Browser*> excluded_browsers) {
  Browser* new_browser = GetBrowserNotInSet(excluded_browsers);
  if (new_browser == NULL) {
    BrowserAddedObserver observer;
    new_browser = observer.WaitForSingleNewBrowser();
    // The new browser should never be in |excluded_browsers|.
    DCHECK(!ContainsKey(excluded_browsers, new_browser));
  }
  return new_browser;
}

}  // namespace

bool GetCurrentTabTitle(const Browser* browser, base::string16* title) {
  WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  if (!web_contents)
    return false;
  NavigationEntry* last_entry = web_contents->GetController().GetActiveEntry();
  if (!last_entry)
    return false;
  title->assign(last_entry->GetTitleForDisplay(std::string()));
  return true;
}

Browser* OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  chrome::HostDesktopType active_desktop = chrome::GetActiveDesktop();
  chrome::OpenURLOffTheRecord(profile, url, active_desktop);
  Browser* browser = chrome::FindTabbedBrowser(
      profile->GetOffTheRecordProfile(), false, active_desktop);
  content::TestNavigationObserver observer(
      browser->tab_strip_model()->GetActiveWebContents());
  observer.Wait();
  return browser;
}

void NavigateToURL(chrome::NavigateParams* params) {
  chrome::Navigate(params);
  content::WaitForLoadStop(params->target_contents);
}


void NavigateToURLWithPost(Browser* browser, const GURL& url) {
  chrome::NavigateParams params(browser, url,
                                content::PAGE_TRANSITION_FORM_SUBMIT);
  params.uses_post = true;
  NavigateToURL(&params);
}

void NavigateToURL(Browser* browser, const GURL& url) {
  NavigateToURLWithDisposition(browser, url, CURRENT_TAB,
                               BROWSER_TEST_WAIT_FOR_NAVIGATION);
}

// Navigates the specified tab (via |disposition|) of |browser| to |url|,
// blocking until the |number_of_navigations| specified complete.
// |disposition| indicates what tab the download occurs in, and
// |browser_test_flags| controls what to wait for before continuing.
static void NavigateToURLWithDispositionBlockUntilNavigationsComplete(
    Browser* browser,
    const GURL& url,
    int number_of_navigations,
    WindowOpenDisposition disposition,
    int browser_test_flags) {
  TabStripModel* tab_strip = browser->tab_strip_model();
  if (disposition == CURRENT_TAB && tab_strip->GetActiveWebContents())
      content::WaitForLoadStop(tab_strip->GetActiveWebContents());
  content::TestNavigationObserver same_tab_observer(
      tab_strip->GetActiveWebContents(),
      number_of_navigations);

  std::set<Browser*> initial_browsers;
  for (chrome::BrowserIterator it; !it.done(); it.Next())
    initial_browsers.insert(*it);

  content::WindowedNotificationObserver tab_added_observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

  browser->OpenURL(OpenURLParams(
      url, Referrer(), disposition, content::PAGE_TRANSITION_TYPED, false));
  if (browser_test_flags & BROWSER_TEST_WAIT_FOR_BROWSER)
    browser = WaitForBrowserNotInSet(initial_browsers);
  if (browser_test_flags & BROWSER_TEST_WAIT_FOR_TAB)
    tab_added_observer.Wait();
  if (!(browser_test_flags & BROWSER_TEST_WAIT_FOR_NAVIGATION)) {
    // Some other flag caused the wait prior to this.
    return;
  }
  WebContents* web_contents = NULL;
  if (disposition == NEW_BACKGROUND_TAB) {
    // We've opened up a new tab, but not selected it.
    TabStripModel* tab_strip = browser->tab_strip_model();
    web_contents = tab_strip->GetWebContentsAt(tab_strip->active_index() + 1);
    EXPECT_TRUE(web_contents != NULL)
        << " Unable to wait for navigation to \"" << url.spec()
        << "\" because the new tab is not available yet";
    if (!web_contents)
      return;
  } else if ((disposition == CURRENT_TAB) ||
      (disposition == NEW_FOREGROUND_TAB) ||
      (disposition == SINGLETON_TAB)) {
    // The currently selected tab is the right one.
    web_contents = browser->tab_strip_model()->GetActiveWebContents();
  }
  if (disposition == CURRENT_TAB) {
    same_tab_observer.Wait();
    return;
  } else if (web_contents) {
    content::TestNavigationObserver observer(web_contents,
                                             number_of_navigations);
    observer.Wait();
    return;
  }
  EXPECT_TRUE(NULL != web_contents) << " Unable to wait for navigation to \""
                                    << url.spec() << "\""
                                    << " because we can't get the tab contents";
}

void NavigateToURLWithDisposition(Browser* browser,
                                  const GURL& url,
                                  WindowOpenDisposition disposition,
                                  int browser_test_flags) {
  NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser,
      url,
      1,
      disposition,
      browser_test_flags);
}

void NavigateToURLBlockUntilNavigationsComplete(Browser* browser,
                                                const GURL& url,
                                                int number_of_navigations) {
  NavigateToURLWithDispositionBlockUntilNavigationsComplete(
      browser,
      url,
      number_of_navigations,
      CURRENT_TAB,
      BROWSER_TEST_WAIT_FOR_NAVIGATION);
}

base::FilePath GetTestFilePath(const base::FilePath& dir,
                               const base::FilePath& file) {
  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.Append(dir).Append(file);
}

GURL GetTestUrl(const base::FilePath& dir, const base::FilePath& file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}

bool GetRelativeBuildDirectory(base::FilePath* build_dir) {
  // This function is used to find the build directory so TestServer can serve
  // built files (nexes, etc).  TestServer expects a path relative to the source
  // root.
  base::FilePath exe_dir =
      CommandLine::ForCurrentProcess()->GetProgram().DirName();
  base::FilePath src_dir;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &src_dir))
    return false;

  // We must first generate absolute paths to SRC and EXE and from there
  // generate a relative path.
  if (!exe_dir.IsAbsolute())
    exe_dir = base::MakeAbsoluteFilePath(exe_dir);
  if (!src_dir.IsAbsolute())
    src_dir = base::MakeAbsoluteFilePath(src_dir);
  if (!exe_dir.IsAbsolute())
    return false;
  if (!src_dir.IsAbsolute())
    return false;

  size_t match, exe_size, src_size;
  std::vector<base::FilePath::StringType> src_parts, exe_parts;

  // Determine point at which src and exe diverge.
  exe_dir.GetComponents(&exe_parts);
  src_dir.GetComponents(&src_parts);
  exe_size = exe_parts.size();
  src_size = src_parts.size();
  for (match = 0; match < exe_size && match < src_size; ++match) {
    if (exe_parts[match] != src_parts[match])
      break;
  }

  // Create a relative path.
  *build_dir = base::FilePath();
  for (size_t tmp_itr = match; tmp_itr < src_size; ++tmp_itr)
    *build_dir = build_dir->Append(FILE_PATH_LITERAL(".."));
  for (; match < exe_size; ++match)
    *build_dir = build_dir->Append(exe_parts[match]);
  return true;
}

AppModalDialog* WaitForAppModalDialog() {
  AppModalDialogQueue* dialog_queue = AppModalDialogQueue::GetInstance();
  if (dialog_queue->HasActiveDialog())
    return dialog_queue->active_dialog();

  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_APP_MODAL_DIALOG_SHOWN,
      content::NotificationService::AllSources());
  observer.Wait();
  return content::Source<AppModalDialog>(observer.source()).ptr();
}

int FindInPage(WebContents* tab,
               const base::string16& search_string,
               bool forward,
               bool match_case,
               int* ordinal,
               gfx::Rect* selection_rect) {
  FindTabHelper* find_tab_helper = FindTabHelper::FromWebContents(tab);
  find_tab_helper->StartFinding(search_string, forward, match_case);
  FindInPageNotificationObserver observer(tab);
  observer.Wait();
  if (ordinal)
    *ordinal = observer.active_match_ordinal();
  if (selection_rect)
    *selection_rect = observer.selection_rect();
  return observer.number_of_matches();
}

void WaitForTemplateURLServiceToLoad(TemplateURLService* service) {
  if (service->loaded())
    return;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;
  scoped_ptr<TemplateURLService::Subscription> subscription =
      service->RegisterOnLoadedCallback(
          message_loop_runner->QuitClosure());
  service->Load();
  message_loop_runner->Run();

  ASSERT_TRUE(service->loaded());
}

void WaitForHistoryToLoad(HistoryService* history_service) {
  content::WindowedNotificationObserver history_loaded_observer(
      chrome::NOTIFICATION_HISTORY_LOADED,
      content::NotificationService::AllSources());
  if (!history_service->BackendLoaded())
    history_loaded_observer.Wait();
}

void DownloadURL(Browser* browser, const GURL& download_url) {
  base::ScopedTempDir downloads_directory;
  ASSERT_TRUE(downloads_directory.CreateUniqueTempDir());
  browser->profile()->GetPrefs()->SetFilePath(
      prefs::kDownloadDefaultDirectory, downloads_directory.path());

  content::DownloadManager* download_manager =
      content::BrowserContext::GetDownloadManager(browser->profile());
  scoped_ptr<content::DownloadTestObserver> observer(
      new content::DownloadTestObserverTerminal(
          download_manager, 1,
          content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT));

  ui_test_utils::NavigateToURL(browser, download_url);
  observer->WaitForFinished();
}

void SendToOmniboxAndSubmit(LocationBar* location_bar,
                            const std::string& input) {
  OmniboxView* omnibox = location_bar->GetOmniboxView();
  omnibox->model()->OnSetFocus(false);
  omnibox->SetUserText(base::ASCIIToUTF16(input));
  location_bar->AcceptInput();
  while (!omnibox->model()->autocomplete_controller()->done()) {
    content::WindowedNotificationObserver observer(
        chrome::NOTIFICATION_AUTOCOMPLETE_CONTROLLER_RESULT_READY,
        content::NotificationService::AllSources());
    observer.Wait();
  }
}

Browser* GetBrowserNotInSet(std::set<Browser*> excluded_browsers) {
  for (chrome::BrowserIterator it; !it.done(); it.Next()) {
    if (excluded_browsers.find(*it) == excluded_browsers.end())
      return *it;
  }
  return NULL;
}

namespace {

void GetCookiesCallback(base::WaitableEvent* event,
                        std::string* cookies,
                        const std::string& cookie_line) {
  *cookies = cookie_line;
  event->Signal();
}

void GetCookiesOnIOThread(
    const GURL& url,
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    base::WaitableEvent* event,
    std::string* cookies) {
  context_getter->GetURLRequestContext()->cookie_store()->
      GetCookiesWithOptionsAsync(
          url, net::CookieOptions(),
          base::Bind(&GetCookiesCallback, event, cookies));
}

}  // namespace

void GetCookies(const GURL& url,
                WebContents* contents,
                int* value_size,
                std::string* value) {
  *value_size = -1;
  if (url.is_valid() && contents) {
    scoped_refptr<net::URLRequestContextGetter> context_getter =
        contents->GetBrowserContext()->GetRequestContextForRenderProcess(
            contents->GetRenderProcessHost()->GetID());
    base::WaitableEvent event(true /* manual reset */,
                              false /* not initially signaled */);
    CHECK(content::BrowserThread::PostTask(
        content::BrowserThread::IO, FROM_HERE,
        base::Bind(&GetCookiesOnIOThread, url, context_getter, &event, value)));
    event.Wait();

    *value_size = static_cast<int>(value->size());
  }
}

WindowedTabAddedNotificationObserver::WindowedTabAddedNotificationObserver(
    const content::NotificationSource& source)
    : WindowedNotificationObserver(chrome::NOTIFICATION_TAB_ADDED, source),
      added_tab_(NULL) {
}

void WindowedTabAddedNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  added_tab_ = content::Details<WebContents>(details).ptr();
  content::WindowedNotificationObserver::Observe(type, source, details);
}

UrlLoadObserver::UrlLoadObserver(const GURL& url,
                                 const content::NotificationSource& source)
    : WindowedNotificationObserver(content::NOTIFICATION_LOAD_STOP, source),
      url_(url) {
}

UrlLoadObserver::~UrlLoadObserver() {}

void UrlLoadObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  NavigationController* controller =
      content::Source<NavigationController>(source).ptr();
  if (controller->GetWebContents()->GetURL() != url_)
    return;

  WindowedNotificationObserver::Observe(type, source, details);
}

BrowserAddedObserver::BrowserAddedObserver()
    : notification_observer_(
          chrome::NOTIFICATION_BROWSER_OPENED,
          content::NotificationService::AllSources()) {
  for (chrome::BrowserIterator it; !it.done(); it.Next())
    original_browsers_.insert(*it);
}

BrowserAddedObserver::~BrowserAddedObserver() {
}

Browser* BrowserAddedObserver::WaitForSingleNewBrowser() {
  notification_observer_.Wait();
  // Ensure that only a single new browser has appeared.
  EXPECT_EQ(original_browsers_.size() + 1, chrome::GetTotalBrowserCount());
  return GetBrowserNotInSet(original_browsers_);
}

#if defined(OS_WIN)

bool SaveScreenSnapshotToDirectory(const base::FilePath& directory,
                                   base::FilePath* screenshot_path) {
  bool succeeded = false;
  base::FilePath out_path(GetSnapshotFileName(directory));

  MONITORINFO monitor_info = {};
  monitor_info.cbSize = sizeof(monitor_info);
  HMONITOR main_monitor = MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY);
  if (GetMonitorInfo(main_monitor, &monitor_info)) {
    RECT& rect = monitor_info.rcMonitor;

    std::vector<unsigned char> png_data;
    gfx::Rect bounds(
        gfx::Size(rect.right - rect.left, rect.bottom - rect.top));
    if (ui::GrabDesktopSnapshot(bounds, &png_data) &&
        png_data.size() <= INT_MAX) {
      int bytes = static_cast<int>(png_data.size());
      int written = base::WriteFile(
          out_path, reinterpret_cast<char*>(&png_data[0]), bytes);
      succeeded = (written == bytes);
    }
  }

  if (succeeded && screenshot_path != NULL)
    *screenshot_path = out_path;

  return succeeded;
}

bool SaveScreenSnapshotToDesktop(base::FilePath* screenshot_path) {
  base::FilePath desktop;

  return PathService::Get(base::DIR_USER_DESKTOP, &desktop) &&
      SaveScreenSnapshotToDirectory(desktop, screenshot_path);
}

#endif  // defined(OS_WIN)

void OverrideGeolocation(double latitude, double longitude) {
  content::Geoposition position;
  position.latitude = latitude;
  position.longitude = longitude;
  position.altitude = 0.;
  position.accuracy = 0.;
  position.timestamp = base::Time::Now();
  content::GeolocationProvider::GetInstance()->OverrideLocationForTesting(
      position);
}

HistoryEnumerator::HistoryEnumerator(Profile* profile) {
  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;

  HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(base::string16(),
                   history::QueryOptions(),
                   base::Bind(&HistoryEnumerator::HistoryQueryComplete,
                              base::Unretained(this),
                              message_loop_runner->QuitClosure()),
                   &tracker_);
  message_loop_runner->Run();
}

HistoryEnumerator::~HistoryEnumerator() {}

void HistoryEnumerator::HistoryQueryComplete(
    const base::Closure& quit_task,
    history::QueryResults* results) {
  for (size_t i = 0; i < results->size(); ++i)
    urls_.push_back((*results)[i].url());
  quit_task.Run();
}

}  // namespace ui_test_utils
