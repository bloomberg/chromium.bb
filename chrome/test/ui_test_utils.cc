// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/ui_test_utils.h"

#include <vector>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/download/download_item.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/render_process_host.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/tab_contents/navigation_controller.h"
#include "chrome/browser/tab_contents/navigation_entry.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/notification_type.h"
#include "chrome/test/automation/javascript_execution_controller.h"
#include "chrome/test/bookmark_load_observer.h"
#if defined(TOOLKIT_VIEWS)
#include "views/focus/accelerator_handler.h"
#endif
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"

namespace ui_test_utils {

namespace {

// Used to block until a navigation completes.
class NavigationNotificationObserver : public NotificationObserver {
 public:
  NavigationNotificationObserver(NavigationController* controller,
                                 int number_of_navigations)
      : navigation_started_(false),
        navigations_completed_(0),
        number_of_navigations_(number_of_navigations) {
    registrar_.Add(this, NotificationType::NAV_ENTRY_COMMITTED,
                   Source<NavigationController>(controller));
    registrar_.Add(this, NotificationType::LOAD_START,
                   Source<NavigationController>(controller));
    registrar_.Add(this, NotificationType::LOAD_STOP,
                   Source<NavigationController>(controller));
    RunMessageLoop();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::NAV_ENTRY_COMMITTED ||
        type == NotificationType::LOAD_START) {
      navigation_started_ = true;
    } else if (type == NotificationType::LOAD_STOP) {
      if (navigation_started_ &&
          ++navigations_completed_ == number_of_navigations_) {
        navigation_started_ = false;
        MessageLoopForUI::current()->Quit();
      }
    }
  }

 private:
  NotificationRegistrar registrar_;

  // If true the navigation has started.
  bool navigation_started_;

  // The number of navigations that have been completed.
  int navigations_completed_;

  // The number of navigations to wait for.
  int number_of_navigations_;

  DISALLOW_COPY_AND_ASSIGN(NavigationNotificationObserver);
};

class DOMOperationObserver : public NotificationObserver {
 public:
  explicit DOMOperationObserver(RenderViewHost* render_view_host)
      : did_respond_(false) {
    registrar_.Add(this, NotificationType::DOM_OPERATION_RESPONSE,
                   Source<RenderViewHost>(render_view_host));
    ui_test_utils::RunMessageLoop();
  }

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) {
    DCHECK(type == NotificationType::DOM_OPERATION_RESPONSE);
    Details<DomOperationNotificationDetails> dom_op_details(details);
    response_ = dom_op_details->json();
    did_respond_ = true;
    MessageLoopForUI::current()->Quit();
  }

  bool GetResponse(std::string* response) WARN_UNUSED_RESULT {
    *response = response_;
    return did_respond_;
  }

 private:
  NotificationRegistrar registrar_;
  std::string response_;
  bool did_respond_;

  DISALLOW_COPY_AND_ASSIGN(DOMOperationObserver);
};

// DownloadsCompleteObserver waits for a given number of downloads to complete.
// Example usage:
//
//     ui_test_utils::NavigateToURL(browser(), zip_url);
//     DownloadsCompleteObserver wait_on_download(
//         browser()->profile()->GetDownloadManager(), 1);
//     /* |zip_url| download will be complete by this line. */
//
class DownloadsCompleteObserver : public DownloadManager::Observer,
                                  public DownloadItem::Observer {
 public:
  explicit DownloadsCompleteObserver(DownloadManager* download_manager,
                                    size_t wait_count)
      : download_manager_(download_manager),
        wait_count_(wait_count),
        waiting_(false) {
    download_manager_->AddObserver(this);
  }

  // CheckAllDownloadsComplete will be called when the DownloadManager
  // fires it's ModelChanged() call, and also when incomplete downloads
  // fire their OnDownloadUpdated().
  bool CheckAllDownloadsComplete() {
    if (downloads_.size() < wait_count_)
      return false;

    bool still_waiting = false;
    std::vector<DownloadItem*>::iterator it = downloads_.begin();
    for (; it != downloads_.end(); ++it) {
      // We always remove ourselves as an observer, then re-add if the download
      // isn't complete. This is to avoid having to track which downloads we
      // are currently observing. Removing has no effect if we are not currently
      // an observer.
      (*it)->RemoveObserver(this);
      if ((*it)->state() != DownloadItem::COMPLETE) {
        (*it)->AddObserver(this);
        still_waiting = true;
      }
    }

    if (still_waiting)
      return false;

    download_manager_->RemoveObserver(this);
    // waiting_ will have been set if not all downloads were complete on first
    // pass below in ModelChanged().
    if (waiting_)
      MessageLoopForUI::current()->Quit();
    return true;
  }

  // DownloadItem::Observer
  virtual void OnDownloadUpdated(DownloadItem* download) {
    if (download->state() == DownloadItem::COMPLETE) {
      CheckAllDownloadsComplete();
    }
  }

  virtual void OnDownloadFileCompleted(DownloadItem* download) { }
  virtual void OnDownloadOpened(DownloadItem* download) {}

  // DownloadManager::Observer
  virtual void ModelChanged() {
    downloads_.clear();
    download_manager_->SearchDownloads(string16(), &downloads_);

    if (CheckAllDownloadsComplete())
      return;

    if (!waiting_) {
      waiting_ = true;
      ui_test_utils::RunMessageLoop();
    }
  }

 private:
  // The observed download manager.
  DownloadManager* download_manager_;

  // The current downloads being tracked.
  std::vector<DownloadItem*> downloads_;

  // The number of downloads to wait on completing.
  size_t wait_count_;

  // Whether an internal message loop has been started and must be quit upon
  // all downloads completing.
  bool waiting_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsCompleteObserver);
};

class FindInPageNotificationObserver : public NotificationObserver {
 public:
  explicit FindInPageNotificationObserver(TabContentsWrapper* parent_tab)
      : parent_tab_(parent_tab),
        active_match_ordinal_(-1),
        number_of_matches_(0) {
    current_find_request_id_ =
        parent_tab->find_tab_helper()->current_find_request_id();
    registrar_.Add(this, NotificationType::FIND_RESULT_AVAILABLE,
                   Source<TabContents>(parent_tab_->tab_contents()));
    ui_test_utils::RunMessageLoop();
  }

  int active_match_ordinal() const { return active_match_ordinal_; }

  int number_of_matches() const { return number_of_matches_; }

  virtual void Observe(NotificationType type, const NotificationSource& source,
                       const NotificationDetails& details) {
    if (type == NotificationType::FIND_RESULT_AVAILABLE) {
      Details<FindNotificationDetails> find_details(details);
      if (find_details->request_id() == current_find_request_id_) {
        // We get multiple responses and one of those will contain the ordinal.
        // This message comes to us before the final update is sent.
        if (find_details->active_match_ordinal() > -1)
          active_match_ordinal_ = find_details->active_match_ordinal();
        if (find_details->final_update()) {
          number_of_matches_ = find_details->number_of_matches();
          MessageLoopForUI::current()->Quit();
        } else {
          DVLOG(1) << "Ignoring, since we only care about the final message";
        }
      }
    } else {
      NOTREACHED();
    }
  }

 private:
  NotificationRegistrar registrar_;
  TabContentsWrapper* parent_tab_;
  // We will at some point (before final update) be notified of the ordinal and
  // we need to preserve it so we can send it later.
  int active_match_ordinal_;
  int number_of_matches_;
  // The id of the current find request, obtained from TabContents. Allows us
  // to monitor when the search completes.
  int current_find_request_id_;

  DISALLOW_COPY_AND_ASSIGN(FindInPageNotificationObserver);
};

class InProcessJavaScriptExecutionController
    : public base::RefCounted<InProcessJavaScriptExecutionController>,
      public JavaScriptExecutionController {
 public:
  explicit InProcessJavaScriptExecutionController(
      RenderViewHost* render_view_host)
      : render_view_host_(render_view_host) {}

 protected:
  // Executes |script| and sets the JSON response |json|.
  virtual bool ExecuteJavaScriptAndGetJSON(const std::string& script,
                                           std::string* json) {
    render_view_host_->ExecuteJavascriptInWebFrame(string16(),
                                                   UTF8ToUTF16(script));
    DOMOperationObserver dom_op_observer(render_view_host_);
    return dom_op_observer.GetResponse(json);
  }

  virtual void FirstObjectAdded() {
    AddRef();
  }

  virtual void LastObjectRemoved() {
    Release();
  }

 private:
  // Weak pointer to the associated RenderViewHost.
  RenderViewHost* render_view_host_;
};

// Specifying a prototype so that we can add the WARN_UNUSED_RESULT attribute.
bool ExecuteJavaScriptHelper(RenderViewHost* render_view_host,
                             const std::wstring& frame_xpath,
                             const std::wstring& original_script,
                             scoped_ptr<Value>* result) WARN_UNUSED_RESULT;

// Executes the passed |original_script| in the frame pointed to by
// |frame_xpath|.  If |result| is not NULL, stores the value that the evaluation
// of the script in |result|.  Returns true on success.
bool ExecuteJavaScriptHelper(RenderViewHost* render_view_host,
                             const std::wstring& frame_xpath,
                             const std::wstring& original_script,
                             scoped_ptr<Value>* result) {
  // TODO(jcampan): we should make the domAutomationController not require an
  //                automation id.
  std::wstring script = L"window.domAutomationController.setAutomationId(0);" +
      original_script;
  render_view_host->ExecuteJavascriptInWebFrame(WideToUTF16Hack(frame_xpath),
                                                WideToUTF16Hack(script));
  DOMOperationObserver dom_op_observer(render_view_host);
  std::string json;
  if (!dom_op_observer.GetResponse(&json))
    return false;

  // Nothing more to do for callers that ignore the returned JS value.
  if (!result)
    return true;

  // Wrap |json| in an array before deserializing because valid JSON has an
  // array or an object as the root.
  json.insert(0, "[");
  json.append("]");

  scoped_ptr<Value> root_val(base::JSONReader::Read(json, true));
  if (!root_val->IsType(Value::TYPE_LIST))
    return false;

  ListValue* list = static_cast<ListValue*>(root_val.get());
  Value* result_val;
  if (!list || !list->GetSize() ||
      !list->Remove(0, &result_val))  // Remove gives us ownership of the value.
    return false;

  result->reset(result_val);
  return true;
}

}  // namespace

void RunMessageLoop() {
  MessageLoopForUI* loop = MessageLoopForUI::current();
  bool did_allow_task_nesting = loop->NestableTasksAllowed();
  loop->SetNestableTasksAllowed(true);
#if defined(TOOLKIT_VIEWS)
  views::AcceleratorHandler handler;
  loop->Run(&handler);
#elif defined(OS_LINUX)
  loop->Run(NULL);
#else
  loop->Run();
#endif
  loop->SetNestableTasksAllowed(did_allow_task_nesting);
}

void RunAllPendingInMessageLoop() {
  MessageLoop::current()->PostTask(FROM_HERE, new MessageLoop::QuitTask());
  ui_test_utils::RunMessageLoop();
}

bool GetCurrentTabTitle(const Browser* browser, string16* title) {
  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return false;
  NavigationEntry* last_entry = tab_contents->controller().GetActiveEntry();
  if (!last_entry)
    return false;
  title->assign(last_entry->title());
  return true;
}

bool WaitForNavigationInCurrentTab(Browser* browser) {
  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return false;
  WaitForNavigation(&tab_contents->controller());
  return true;
}

bool WaitForNavigationsInCurrentTab(Browser* browser,
                                    int number_of_navigations) {
  TabContents* tab_contents = browser->GetSelectedTabContents();
  if (!tab_contents)
    return false;
  WaitForNavigations(&tab_contents->controller(), number_of_navigations);
  return true;
}

void WaitForNavigation(NavigationController* controller) {
  WaitForNavigations(controller, 1);
}

void WaitForNavigations(NavigationController* controller,
                        int number_of_navigations) {
  NavigationNotificationObserver observer(controller, number_of_navigations);
}

void WaitForNewTab(Browser* browser) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, NotificationType::TAB_ADDED,
                  Source<Browser>(browser));
}

void WaitForBrowserActionUpdated(ExtensionAction* browser_action) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, NotificationType::EXTENSION_BROWSER_ACTION_UPDATED,
                  Source<ExtensionAction>(browser_action));
}

void WaitForLoadStop(NavigationController* controller) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, NotificationType::LOAD_STOP,
                  Source<NavigationController>(controller));
}

Browser* WaitForNewBrowser() {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, NotificationType::BROWSER_WINDOW_READY,
                  NotificationService::AllSources());
  return Source<Browser>(observer.source()).ptr();
}

Browser* WaitForBrowserNotInSet(std::set<Browser*> excluded_browsers) {
  TestNotificationObserver observer;
  Browser* new_browser = GetBrowserNotInSet(excluded_browsers);
  if (new_browser == NULL) {
    new_browser = WaitForNewBrowser();
    // The new browser should never be in |excluded_browsers|.
    DCHECK(!ContainsKey(excluded_browsers, new_browser));
  }
  return new_browser;
}

void OpenURLOffTheRecord(Profile* profile, const GURL& url) {
  Browser::OpenURLOffTheRecord(profile, url);
  Browser* browser = BrowserList::FindBrowserWithType(
      profile->GetOffTheRecordProfile(), Browser::TYPE_NORMAL, false);
  WaitForNavigations(&browser->GetSelectedTabContents()->controller(), 1);
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
  std::set<Browser*> initial_browsers;
  for (std::vector<Browser*>::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end();
       ++iter) {
    initial_browsers.insert(*iter);
  }
  browser->OpenURL(url, GURL(), disposition, PageTransition::TYPED);
  if (browser_test_flags & BROWSER_TEST_WAIT_FOR_BROWSER)
    browser = WaitForBrowserNotInSet(initial_browsers);
  if (browser_test_flags & BROWSER_TEST_WAIT_FOR_TAB)
    WaitForNotification(NotificationType::TAB_ADDED);
  if (!(browser_test_flags & BROWSER_TEST_WAIT_FOR_NAVIGATION)) {
    // Some other flag caused the wait prior to this.
    return;
  }
  TabContents* tab_contents = NULL;
  if (disposition == NEW_BACKGROUND_TAB) {
    // We've opened up a new tab, but not selected it.
    tab_contents = browser->GetTabContentsAt(browser->selected_index() + 1);
    EXPECT_TRUE(tab_contents != NULL)
        << " Unable to wait for navigation to \"" << url.spec()
        << "\" because the new tab is not available yet";
    return;
  } else if ((disposition == CURRENT_TAB) ||
      (disposition == NEW_FOREGROUND_TAB) ||
      (disposition == SINGLETON_TAB)) {
    // The currently selected tab is the right one.
    tab_contents = browser->GetSelectedTabContents();
  }
  if (tab_contents) {
    NavigationController* controller = &tab_contents->controller();
    WaitForNavigations(controller, number_of_navigations);
    return;
  }
  EXPECT_TRUE(NULL != tab_contents) << " Unable to wait for navigation to \""
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

DOMElementProxyRef GetActiveDOMDocument(Browser* browser) {
  JavaScriptExecutionController* executor =
      new InProcessJavaScriptExecutionController(
          browser->GetSelectedTabContents()->render_view_host());
  int element_handle;
  executor->ExecuteJavaScriptAndGetReturn("document;", &element_handle);
  return executor->GetObjectProxy<DOMElementProxy>(element_handle);
}

bool ExecuteJavaScript(RenderViewHost* render_view_host,
                       const std::wstring& frame_xpath,
                       const std::wstring& original_script) {
  std::wstring script =
      original_script + L"window.domAutomationController.send(0);";
  return ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, NULL);
}

bool ExecuteJavaScriptAndExtractInt(RenderViewHost* render_view_host,
                                    const std::wstring& frame_xpath,
                                    const std::wstring& script,
                                    int* result) {
  DCHECK(result);
  scoped_ptr<Value> value;
  if (!ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, &value) ||
      !value.get())
    return false;

  return value->GetAsInteger(result);
}

bool ExecuteJavaScriptAndExtractBool(RenderViewHost* render_view_host,
                                     const std::wstring& frame_xpath,
                                     const std::wstring& script,
                                     bool* result) {
  DCHECK(result);
  scoped_ptr<Value> value;
  if (!ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, &value) ||
      !value.get())
    return false;

  return value->GetAsBoolean(result);
}

bool ExecuteJavaScriptAndExtractString(RenderViewHost* render_view_host,
                                       const std::wstring& frame_xpath,
                                       const std::wstring& script,
                                       std::string* result) {
  DCHECK(result);
  scoped_ptr<Value> value;
  if (!ExecuteJavaScriptHelper(render_view_host, frame_xpath, script, &value) ||
      !value.get())
    return false;

  return value->GetAsString(result);
}

FilePath GetTestFilePath(const FilePath& dir, const FilePath& file) {
  FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  return path.Append(dir).Append(file);
}

GURL GetTestUrl(const FilePath& dir, const FilePath& file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}

void WaitForDownloadCount(DownloadManager* download_manager, size_t count) {
  DownloadsCompleteObserver download_observer(download_manager, count);
}

AppModalDialog* WaitForAppModalDialog() {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, NotificationType::APP_MODAL_DIALOG_SHOWN,
                  NotificationService::AllSources());
  return Source<AppModalDialog>(observer.source()).ptr();
}

void CrashTab(TabContents* tab) {
  RenderProcessHost* rph = tab->render_view_host()->process();
  base::KillProcess(rph->GetHandle(), 0, false);
  TestNotificationObserver observer;
  RegisterAndWait(&observer, NotificationType::RENDERER_PROCESS_CLOSED,
                  Source<RenderProcessHost>(rph));
}

void WaitForFocusChange(TabContents* tab_contents) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, NotificationType::FOCUS_CHANGED_IN_PAGE,
                  Source<TabContents>(tab_contents));
}

void WaitForFocusInBrowser(Browser* browser) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, NotificationType::FOCUS_RETURNED_TO_BROWSER,
                  Source<Browser>(browser));
}

int FindInPage(TabContentsWrapper* tab_contents, const string16& search_string,
               bool forward, bool match_case, int* ordinal) {
  tab_contents->
      find_tab_helper()->StartFinding(search_string, forward, match_case);
  FindInPageNotificationObserver observer(tab_contents);
  if (ordinal)
    *ordinal = observer.active_match_ordinal();
  return observer.number_of_matches();
}

void WaitForNotification(NotificationType type) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, type, NotificationService::AllSources());
}

void WaitForNotificationFrom(NotificationType type,
                             const NotificationSource& source) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, type, source);
}

void RegisterAndWait(NotificationObserver* observer,
                     NotificationType type,
                     const NotificationSource& source) {
  NotificationRegistrar registrar;
  registrar.Add(observer, type, source);
  RunMessageLoop();
}

void WaitForBookmarkModelToLoad(BookmarkModel* model) {
  if (model->IsLoaded())
    return;
  BookmarkLoadObserver observer;
  model->AddObserver(&observer);
  RunMessageLoop();
  model->RemoveObserver(&observer);
  ASSERT_TRUE(model->IsLoaded());
}

void WaitForHistoryToLoad(Browser* browser) {
  HistoryService* history_service =
      browser->profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  if (!history_service->BackendLoaded())
    WaitForNotification(NotificationType::HISTORY_LOADED);
}

bool GetNativeWindow(const Browser* browser, gfx::NativeWindow* native_window) {
  BrowserWindow* window = browser->window();
  if (!window)
    return false;

  *native_window = window->GetNativeHandle();
  return *native_window;
}

bool BringBrowserWindowToFront(const Browser* browser) {
  gfx::NativeWindow window = NULL;
  if (!GetNativeWindow(browser, &window))
    return false;

  ui_test_utils::ShowAndFocusNativeWindow(window);
  return true;
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
  base::TimeTicks start_time = base::TimeTicks::Now();

  gfx::NativeWindow window = NULL;
  if (!GetNativeWindow(browser, &window))
    return false;

  if (!ui_controls::SendKeyPressNotifyWhenDone(
          window, key, control, shift, alt, command,
          new MessageLoop::QuitTask())) {
    LOG(ERROR) << "ui_controls::SendKeyPressNotifyWhenDone failed";
    return false;
  }
  // Run the message loop. It'll stop running when either the key was received
  // or the test timed out (in which case testing::Test::HasFatalFailure should
  // be set).
  RunMessageLoop();
  return !testing::Test::HasFatalFailure();
}

bool SendKeyPressAndWait(const Browser* browser,
                         ui::KeyboardCode key,
                         bool control,
                         bool shift,
                         bool alt,
                         bool command,
                         NotificationType type,
                         const NotificationSource& source) {
  WindowedNotificationObserver observer(type, source);

  if (!SendKeyPressSync(browser, key, control, shift, alt, command))
    return false;

  observer.Wait();
  return !testing::Test::HasFatalFailure();
}


TimedMessageLoopRunner::TimedMessageLoopRunner()
    : loop_(new MessageLoopForUI()),
      owned_(true),
      quit_loop_invoked_(false) {
}

TimedMessageLoopRunner::~TimedMessageLoopRunner() {
  if (owned_)
    delete loop_;
}

void TimedMessageLoopRunner::RunFor(int ms) {
  QuitAfter(ms);
  quit_loop_invoked_ = false;
  loop_->Run();
}

void TimedMessageLoopRunner::Quit() {
  quit_loop_invoked_ = true;
  loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask);
}

void TimedMessageLoopRunner::QuitAfter(int ms) {
  quit_loop_invoked_ = true;
  loop_->PostDelayedTask(FROM_HERE, new MessageLoop::QuitTask, ms);
}

namespace {

void AppendToPythonPath(const FilePath& dir) {
#if defined(OS_WIN)
  const wchar_t kPythonPath[] = L"PYTHONPATH";
  // TODO(ukai): handle longer PYTHONPATH variables.
  wchar_t oldpath[4096];
  if (::GetEnvironmentVariable(kPythonPath, oldpath, arraysize(oldpath)) == 0) {
    ::SetEnvironmentVariableW(kPythonPath, dir.value().c_str());
  } else if (!wcsstr(oldpath, dir.value().c_str())) {
    std::wstring newpath(oldpath);
    newpath.append(L";");
    newpath.append(dir.value());
    SetEnvironmentVariableW(kPythonPath, newpath.c_str());
  }
#elif defined(OS_POSIX)
  const char kPythonPath[] = "PYTHONPATH";
  const char* oldpath = getenv(kPythonPath);
  if (!oldpath) {
    setenv(kPythonPath, dir.value().c_str(), 1);
  } else if (!strstr(oldpath, dir.value().c_str())) {
    std::string newpath(oldpath);
    newpath.append(":");
    newpath.append(dir.value());
    setenv(kPythonPath, newpath.c_str(), 1);
  }
#endif
}

}  // anonymous namespace

TestWebSocketServer::TestWebSocketServer() : started_(false) {
}

bool TestWebSocketServer::Start(const FilePath& root_directory) {
  if (started_)
    return true;
  scoped_ptr<CommandLine> cmd_line(CreateWebSocketServerCommandLine());
  cmd_line->AppendSwitchASCII("server", "start");
  cmd_line->AppendSwitch("chromium");
  cmd_line->AppendSwitch("register_cygwin");
  cmd_line->AppendSwitchPath("root", root_directory);
  if (!temp_dir_.CreateUniqueTempDir()) {
    LOG(ERROR) << "Unable to create a temporary directory.";
    return false;
  }
  websocket_pid_file_ = temp_dir_.path().AppendASCII("websocket.pid");
  cmd_line->AppendSwitchPath("pidfile", websocket_pid_file_);
  SetPythonPath();
  if (!base::LaunchApp(*cmd_line.get(), true, false, NULL)) {
    LOG(ERROR) << "Unable to launch websocket server.";
    return false;
  }
  started_ = true;
  return true;
}

CommandLine* TestWebSocketServer::CreatePythonCommandLine() {
  return new CommandLine(FilePath(FILE_PATH_LITERAL("python")));
}

void TestWebSocketServer::SetPythonPath() {
  FilePath scripts_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &scripts_path);

  scripts_path = scripts_path
      .Append(FILE_PATH_LITERAL("third_party"))
      .Append(FILE_PATH_LITERAL("WebKit"))
      .Append(FILE_PATH_LITERAL("Tools"))
      .Append(FILE_PATH_LITERAL("Scripts"));
  AppendToPythonPath(scripts_path);
}

CommandLine* TestWebSocketServer::CreateWebSocketServerCommandLine() {
  FilePath src_path;
  // Get to 'src' dir.
  PathService::Get(base::DIR_SOURCE_ROOT, &src_path);

  FilePath script_path(src_path);
  script_path = script_path.AppendASCII("third_party");
  script_path = script_path.AppendASCII("WebKit");
  script_path = script_path.AppendASCII("Tools");
  script_path = script_path.AppendASCII("Scripts");
  script_path = script_path.AppendASCII("new-run-webkit-websocketserver");

  CommandLine* cmd_line = CreatePythonCommandLine();
  cmd_line->AppendArgPath(script_path);
  return cmd_line;
}

TestWebSocketServer::~TestWebSocketServer() {
  if (!started_)
    return;
  scoped_ptr<CommandLine> cmd_line(CreateWebSocketServerCommandLine());
  cmd_line->AppendSwitchASCII("server", "stop");
  cmd_line->AppendSwitch("chromium");
  cmd_line->AppendSwitchPath("pidfile", websocket_pid_file_);
  base::LaunchApp(*cmd_line.get(), true, false, NULL);
}

TestNotificationObserver::TestNotificationObserver()
    : source_(NotificationService::AllSources()) {
}

TestNotificationObserver::~TestNotificationObserver() {}

void TestNotificationObserver::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  source_ = source;
  details_ = details;
  MessageLoopForUI::current()->Quit();
}

WindowedNotificationObserver::WindowedNotificationObserver(
    NotificationType notification_type,
    const NotificationSource& source)
    : seen_(false),
      running_(false),
      waiting_for_(source) {
  registrar_.Add(this, notification_type, waiting_for_);
}

WindowedNotificationObserver::~WindowedNotificationObserver() {}

void WindowedNotificationObserver::Wait() {
  if (waiting_for_ == NotificationService::AllSources()) {
    LOG(FATAL) << "Wait called when monitoring all sources. You must use "
               << "WaitFor in this case.";
  }

  if (seen_)
    return;

  running_ = true;
  ui_test_utils::RunMessageLoop();
}

void WindowedNotificationObserver::WaitFor(const NotificationSource& source) {
  if (waiting_for_ != NotificationService::AllSources()) {
    LOG(FATAL) << "WaitFor called when already waiting on a specific source."
               << "Use Wait in this case.";
  }

  waiting_for_ = source;
  if (sources_seen_.count(waiting_for_.map_key()) > 0)
    return;

  running_ = true;
  ui_test_utils::RunMessageLoop();
}

void WindowedNotificationObserver::Observe(NotificationType type,
                                           const NotificationSource& source,
                                           const NotificationDetails& details) {
  if (waiting_for_ == source) {
    seen_ = true;
    if (running_)
      MessageLoopForUI::current()->Quit();
  } else {
    sources_seen_.insert(source.map_key());
  }
}

DOMMessageQueue::DOMMessageQueue() {
  registrar_.Add(this, NotificationType::DOM_OPERATION_RESPONSE,
                 NotificationService::AllSources());
}

DOMMessageQueue::~DOMMessageQueue() {}

void DOMMessageQueue::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  Details<DomOperationNotificationDetails> dom_op_details(details);
  Source<RenderViewHost> sender(source);
  message_queue_.push(dom_op_details->json());
  if (waiting_for_message_) {
    waiting_for_message_ = false;
    MessageLoopForUI::current()->Quit();
  }
}

bool DOMMessageQueue::WaitForMessage(std::string* message) {
  if (message_queue_.empty()) {
    waiting_for_message_ = true;
    // This will be quit when a new message comes in.
    RunMessageLoop();
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
        false,  // don't use backing_store
        NewCallback(this, &SnapshotTaker::OnSnapshotTaken),
        page_size,
        desired_size);
    ui_test_utils::RunMessageLoop();
    return snapshot_taken_;
  }

  bool TakeEntirePageSnapshot(RenderViewHost* rvh,
                              SkBitmap* bitmap) WARN_UNUSED_RESULT {
    const wchar_t* script =
        L"window.domAutomationController.send("
        L"    JSON.stringify([document.width, document.height]))";
    std::string json;
    if (!ui_test_utils::ExecuteJavaScriptAndExtractString(
            rvh, L"", script, &json))
      return false;

    // Parse the JSON.
    std::vector<int> dimensions;
    scoped_ptr<Value> value(base::JSONReader::Read(json, true));
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
    MessageLoop::current()->Quit();
  }

  SkBitmap* bitmap_;
  // Whether the snapshot was actually taken and received by this SnapshotTaker.
  // This will be false if the test times out.
  bool snapshot_taken_;

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

}  // namespace ui_test_utils
