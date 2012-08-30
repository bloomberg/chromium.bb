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
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/test/test_timeouts.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/ui/app_modal_dialogs/app_modal_dialog.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/bookmark_load_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/geolocation.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/geoposition.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/net_util.h"
#include "net/test/python_utils.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"
#include "ui/ui_controls/ui_controls.h"

#if defined(USE_AURA)
#include "ash/shell.h"
#include "ui/aura/root_window.h"
#endif

using content::BrowserThread;
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

class FindInPageNotificationObserver : public content::NotificationObserver {
 public:
  explicit FindInPageNotificationObserver(TabContents* parent_tab)
      : parent_tab_(parent_tab),
        active_match_ordinal_(-1),
        number_of_matches_(0) {
    current_find_request_id_ =
        parent_tab->find_tab_helper()->current_find_request_id();
    registrar_.Add(this, chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
                   content::Source<WebContents>(parent_tab_->web_contents()));
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

  int active_match_ordinal() const { return active_match_ordinal_; }
  int number_of_matches() const { return number_of_matches_; }
  gfx::Rect selection_rect() const { return selection_rect_; }

  virtual void Observe(int type, const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    if (type == chrome::NOTIFICATION_FIND_RESULT_AVAILABLE) {
      content::Details<FindNotificationDetails> find_details(details);
      if (find_details->request_id() == current_find_request_id_) {
        // We get multiple responses and one of those will contain the ordinal.
        // This message comes to us before the final update is sent.
        if (find_details->active_match_ordinal() > -1) {
          active_match_ordinal_ = find_details->active_match_ordinal();
          selection_rect_ = find_details->selection_rect();
        }
        if (find_details->final_update()) {
          number_of_matches_ = find_details->number_of_matches();
          message_loop_runner_->Quit();
        } else {
          DVLOG(1) << "Ignoring, since we only care about the final message";
        }
      }
    } else {
      NOTREACHED();
    }
  }

 private:
  content::NotificationRegistrar registrar_;
  TabContents* parent_tab_;
  // We will at some point (before final update) be notified of the ordinal and
  // we need to preserve it so we can send it later.
  int active_match_ordinal_;
  int number_of_matches_;
  gfx::Rect selection_rect_;
  // The id of the current find request, obtained from WebContents. Allows us
  // to monitor when the search completes.
  int current_find_request_id_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(FindInPageNotificationObserver);
};

}  // namespace

bool GetCurrentTabTitle(const Browser* browser, string16* title) {
  WebContents* web_contents = chrome::GetActiveWebContents(browser);
  if (!web_contents)
    return false;
  NavigationEntry* last_entry = web_contents->GetController().GetActiveEntry();
  if (!last_entry)
    return false;
  title->assign(last_entry->GetTitleForDisplay(""));
  return true;
}

void WaitForNavigations(NavigationController* controller,
                        int number_of_navigations) {
  content::TestNavigationObserver observer(
      content::Source<NavigationController>(controller), NULL,
      number_of_navigations);
  base::RunLoop run_loop;
  observer.WaitForObservation(
      base::Bind(&content::RunThisRunLoop, base::Unretained(&run_loop)),
      content::GetQuitTaskForRunLoop(&run_loop));
}

void WaitForNewTab(Browser* browser) {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_TAB_ADDED,
      content::Source<content::WebContentsDelegate>(browser));
  observer.Wait();
}

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

Browser* OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  chrome::OpenURLOffTheRecord(profile, url);
  Browser* browser = browser::FindTabbedBrowser(
      profile->GetOffTheRecordProfile(), false);
  WaitForNavigations(&chrome::GetActiveWebContents(browser)->GetController(),
                     1);
  return browser;
}

void NavigateToURL(chrome::NavigateParams* params) {
  content::TestNavigationObserver observer(
      content::NotificationService::AllSources(), NULL, 1);
  chrome::Navigate(params);
  base::RunLoop run_loop;
  observer.WaitForObservation(
      base::Bind(&content::RunThisRunLoop, base::Unretained(&run_loop)),
      content::GetQuitTaskForRunLoop(&run_loop));
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
  if (disposition == CURRENT_TAB && chrome::GetActiveWebContents(browser))
      content::WaitForLoadStop(chrome::GetActiveWebContents(browser));
  NavigationController* controller =
      chrome::GetActiveWebContents(browser) ?
      &chrome::GetActiveWebContents(browser)->GetController() : NULL;
  content::TestNavigationObserver same_tab_observer(
      content::Source<NavigationController>(controller),
      NULL,
      number_of_navigations);

  std::set<Browser*> initial_browsers;
  for (std::vector<Browser*>::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end();
       ++iter) {
    initial_browsers.insert(*iter);
  }

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
    web_contents =
        chrome::GetWebContentsAt(browser, browser->active_index() + 1);
    EXPECT_TRUE(web_contents != NULL)
        << " Unable to wait for navigation to \"" << url.spec()
        << "\" because the new tab is not available yet";
    if (!web_contents)
      return;
  } else if ((disposition == CURRENT_TAB) ||
      (disposition == NEW_FOREGROUND_TAB) ||
      (disposition == SINGLETON_TAB)) {
    // The currently selected tab is the right one.
    web_contents = chrome::GetActiveWebContents(browser);
  }
  if (disposition == CURRENT_TAB) {
    base::RunLoop run_loop;
    same_tab_observer.WaitForObservation(
        base::Bind(&content::RunThisRunLoop, base::Unretained(&run_loop)),
        content::GetQuitTaskForRunLoop(&run_loop));
    return;
  } else if (web_contents) {
    NavigationController* controller = &web_contents->GetController();
    WaitForNavigations(controller, number_of_navigations);
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

FilePath GetTestFilePath(const FilePath& dir, const FilePath& file) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.Append(dir).Append(file);
}

GURL GetTestUrl(const FilePath& dir, const FilePath& file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}

bool GetRelativeBuildDirectory(FilePath* build_dir) {
  // This function is used to find the build directory so TestServer can serve
  // built files (nexes, etc).  TestServer expects a path relative to the source
  // root.
  FilePath exe_dir = CommandLine::ForCurrentProcess()->GetProgram().DirName();
  FilePath src_dir;
  if (!PathService::Get(base::DIR_SOURCE_ROOT, &src_dir))
    return false;

  // We must first generate absolute paths to SRC and EXE and from there
  // generate a relative path.
  if (!exe_dir.IsAbsolute())
    file_util::AbsolutePath(&exe_dir);
  if (!src_dir.IsAbsolute())
    file_util::AbsolutePath(&src_dir);
  if (!exe_dir.IsAbsolute())
    return false;
  if (!src_dir.IsAbsolute())
    return false;

  size_t match, exe_size, src_size;
  std::vector<FilePath::StringType> src_parts, exe_parts;

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
  *build_dir = FilePath();
  for (size_t tmp_itr = match; tmp_itr < src_size; ++tmp_itr)
    *build_dir = build_dir->Append(FILE_PATH_LITERAL(".."));
  for (; match < exe_size; ++match)
    *build_dir = build_dir->Append(exe_parts[match]);
  return true;
}

AppModalDialog* WaitForAppModalDialog() {
  content::WindowedNotificationObserver observer(
      chrome::NOTIFICATION_APP_MODAL_DIALOG_SHOWN,
      content::NotificationService::AllSources());
  observer.Wait();
  return content::Source<AppModalDialog>(observer.source()).ptr();
}

int FindInPage(TabContents* tab_contents, const string16& search_string,
               bool forward, bool match_case, int* ordinal,
               gfx::Rect* selection_rect) {
  tab_contents->
      find_tab_helper()->StartFinding(search_string, forward, match_case);
  FindInPageNotificationObserver observer(tab_contents);
  if (ordinal)
    *ordinal = observer.active_match_ordinal();
  if (selection_rect)
    *selection_rect = observer.selection_rect();
  return observer.number_of_matches();
}

void CloseAllInfoBars(TabContents* tab) {
  InfoBarTabHelper* infobar_helper = tab->infobar_tab_helper();
  while (infobar_helper->GetInfoBarCount() > 0)
    infobar_helper->RemoveInfoBar(infobar_helper->GetInfoBarDelegateAt(0));
}

void RegisterAndWait(content::NotificationObserver* observer,
                     int type,
                     const content::NotificationSource& source) {
  content::NotificationRegistrar registrar;
  registrar.Add(observer, type, source);
  content::RunMessageLoop();
}

void WaitForBookmarkModelToLoad(BookmarkModel* model) {
  if (model->IsLoaded())
    return;
  base::RunLoop run_loop;
  BookmarkLoadObserver observer(content::GetQuitTaskForRunLoop(&run_loop));
  model->AddObserver(&observer);
  content::RunThisRunLoop(&run_loop);
  model->RemoveObserver(&observer);
  ASSERT_TRUE(model->IsLoaded());
}

void WaitForTemplateURLServiceToLoad(TemplateURLService* service) {
  if (service->loaded())
    return;
  service->Load();
  TemplateURLServiceTestUtil::BlockTillServiceProcessesRequests();
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
  ScopedTempDir downloads_directory;
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

bool GetNativeWindow(const Browser* browser, gfx::NativeWindow* native_window) {
  BrowserWindow* window = browser->window();
  if (!window)
    return false;

  *native_window = window->GetNativeWindow();
  return *native_window;
}

bool BringBrowserWindowToFront(const Browser* browser) {
  gfx::NativeWindow window = NULL;
  if (!GetNativeWindow(browser, &window))
    return false;

  return ui_test_utils::ShowAndFocusNativeWindow(window);
}

Browser* GetBrowserNotInSet(std::set<Browser*> excluded_browsers) {
  for (BrowserList::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end();
       ++iter) {
    if (excluded_browsers.find(*iter) == excluded_browsers.end())
      return *iter;
  }

  return NULL;
}

bool SendKeyPressSync(const Browser* browser,
                      ui::KeyboardCode key,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) {
  gfx::NativeWindow window = NULL;
  if (!GetNativeWindow(browser, &window))
    return false;
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  bool result;
  result = ui_controls::SendKeyPressNotifyWhenDone(
      window, key, control, shift, alt, command, runner->QuitClosure());
#if defined(OS_WIN)
  if (!result && BringBrowserWindowToFront(browser)) {
    result = ui_controls::SendKeyPressNotifyWhenDone(
        window, key, control, shift, alt, command, runner->QuitClosure());
  }
#endif
  if (!result) {
    LOG(ERROR) << "ui_controls::SendKeyPressNotifyWhenDone failed";
    return false;
  }

  // Run the message loop. It'll stop running when either the key was received
  // or the test timed out (in which case testing::Test::HasFatalFailure should
  // be set).
  runner->Run();
  return !testing::Test::HasFatalFailure();
}

bool SendKeyPressAndWait(const Browser* browser,
                         ui::KeyboardCode key,
                         bool control,
                         bool shift,
                         bool alt,
                         bool command,
                         int type,
                         const content::NotificationSource& source) {
  content::WindowedNotificationObserver observer(type, source);

  if (!SendKeyPressSync(browser, key, control, shift, alt, command))
    return false;

  observer.Wait();
  return !testing::Test::HasFatalFailure();
}

bool SendMouseMoveSync(const gfx::Point& location) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  if (!ui_controls::SendMouseMoveNotifyWhenDone(
          location.x(), location.y(), runner->QuitClosure())) {
    return false;
  }
  runner->Run();
  return !testing::Test::HasFatalFailure();
}

bool SendMouseEventsSync(ui_controls::MouseButton type, int state) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  if (!ui_controls::SendMouseEventsNotifyWhenDone(
          type, state, runner->QuitClosure())) {
    return false;
  }
  runner->Run();
  return !testing::Test::HasFatalFailure();
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
  original_browsers_.insert(BrowserList::begin(), BrowserList::end());
}

BrowserAddedObserver::~BrowserAddedObserver() {
}

Browser* BrowserAddedObserver::WaitForSingleNewBrowser() {
  notification_observer_.Wait();
  // Ensure that only a single new browser has appeared.
  EXPECT_EQ(original_browsers_.size() + 1, BrowserList::size());
  return GetBrowserNotInSet(original_browsers_);
}

DOMMessageQueue::DOMMessageQueue() : waiting_for_message_(false) {
  registrar_.Add(this, content::NOTIFICATION_DOM_OPERATION_RESPONSE,
                 content::NotificationService::AllSources());
}

DOMMessageQueue::~DOMMessageQueue() {}

void DOMMessageQueue::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  content::Details<DomOperationNotificationDetails> dom_op_details(details);
  content::Source<RenderViewHost> sender(source);
  message_queue_.push(dom_op_details->json);
  if (waiting_for_message_) {
    waiting_for_message_ = false;
    message_loop_runner_->Quit();
  }
}

void DOMMessageQueue::ClearQueue() {
  message_queue_ = std::queue<std::string>();
}

bool DOMMessageQueue::WaitForMessage(std::string* message) {
  if (message_queue_.empty()) {
    waiting_for_message_ = true;
    // This will be quit when a new message comes in.
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }
  // The queue should not be empty, unless we were quit because of a timeout.
  if (message_queue_.empty())
    return false;
  if (message)
    *message = message_queue_.front();
  return true;
}

// Coordinates taking snapshots of a |RenderWidget|.
class SnapshotTaker {
 public:
  SnapshotTaker() : bitmap_(NULL) {}

  bool TakeRenderWidgetSnapshot(RenderWidgetHost* rwh,
                                const gfx::Size& page_size,
                                const gfx::Size& desired_size,
                                SkBitmap* bitmap) WARN_UNUSED_RESULT {
    bitmap_ = bitmap;
    ThumbnailGenerator* generator = g_browser_process->GetThumbnailGenerator();
    generator->MonitorRenderer(rwh, true);
    snapshot_taken_ = false;
    generator->AskForSnapshot(
        rwh,
        base::Bind(&SnapshotTaker::OnSnapshotTaken, base::Unretained(this)),
        page_size,
        desired_size);
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
    return snapshot_taken_;
  }

  bool TakeEntirePageSnapshot(RenderViewHost* rvh,
                              SkBitmap* bitmap) WARN_UNUSED_RESULT {
    const wchar_t* script =
        L"window.domAutomationController.send("
        L"    JSON.stringify([document.width, document.height]))";
    std::string json;
    if (!content::ExecuteJavaScriptAndExtractString(rvh, L"", script, &json))
      return false;

    // Parse the JSON.
    std::vector<int> dimensions;
    scoped_ptr<Value> value(
        base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS));
    if (!value->IsType(Value::TYPE_LIST))
      return false;
    ListValue* list = static_cast<ListValue*>(value.get());
    int width, height;
    if (!list->GetInteger(0, &width) || !list->GetInteger(1, &height))
      return false;

    // Take the snapshot.
    gfx::Size page_size(width, height);
    return TakeRenderWidgetSnapshot(rvh, page_size, page_size, bitmap);
  }

 private:
  // Called when the ThumbnailGenerator has taken the snapshot.
  void OnSnapshotTaken(const SkBitmap& bitmap) {
    *bitmap_ = bitmap;
    snapshot_taken_ = true;
    message_loop_runner_->Quit();
  }

  SkBitmap* bitmap_;
  // Whether the snapshot was actually taken and received by this SnapshotTaker.
  // This will be false if the test times out.
  bool snapshot_taken_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(SnapshotTaker);
};

bool TakeRenderWidgetSnapshot(RenderWidgetHost* rwh,
                              const gfx::Size& page_size,
                              SkBitmap* bitmap) {
  DCHECK(bitmap);
  SnapshotTaker taker;
  return taker.TakeRenderWidgetSnapshot(rwh, page_size, page_size, bitmap);
}

bool TakeEntirePageSnapshot(RenderViewHost* rvh, SkBitmap* bitmap) {
  DCHECK(bitmap);
  SnapshotTaker taker;
  return taker.TakeEntirePageSnapshot(rvh, bitmap);
}

void OverrideGeolocation(double latitude, double longitude) {
  content::Geoposition position;
  position.latitude = latitude;
  position.longitude = longitude;
  position.altitude = 0.;
  position.accuracy = 0.;
  position.timestamp = base::Time::Now();
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  content::OverrideLocationForTesting(position, runner->QuitClosure());
  runner->Run();
}

namespace internal {

void ClickTask(ui_controls::MouseButton button,
               int state,
               const base::Closure& followup) {
  if (!followup.is_null())
    ui_controls::SendMouseEventsNotifyWhenDone(button, state, followup);
  else
    ui_controls::SendMouseEvents(button, state);
}

}  // namespace internal

HistoryEnumerator::HistoryEnumerator(Profile* profile) {
  scoped_refptr<content::MessageLoopRunner> message_loop_runner =
      new content::MessageLoopRunner;

  HistoryService* hs = HistoryServiceFactory::GetForProfile(
      profile, Profile::EXPLICIT_ACCESS);
  hs->QueryHistory(
      string16(),
      history::QueryOptions(),
      &consumer_,
      base::Bind(&HistoryEnumerator::HistoryQueryComplete,
                 base::Unretained(this), message_loop_runner->QuitClosure()));
  message_loop_runner->Run();
}

HistoryEnumerator::~HistoryEnumerator() {}

void HistoryEnumerator::HistoryQueryComplete(
    const base::Closure& quit_task,
    HistoryService::Handle request_handle,
    history::QueryResults* results) {
  for (size_t i = 0; i < results->size(); ++i)
    urls_.push_back((*results)[i].url());
  quit_task.Run();
}

}  // namespace ui_test_utils
