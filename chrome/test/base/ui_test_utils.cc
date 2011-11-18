// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/ui_test_utils.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/automation/ui_controls.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/dom_operation_notification_details.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_notification_details.h"
#include "chrome/browser/ui/find_bar/find_tab_helper.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/test/automation/javascript_execution_controller.h"
#include "chrome/test/base/bookmark_load_observer.h"
#include "chrome/test/test_navigation_observer.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/navigation_controller.h"
#include "content/browser/tab_contents/navigation_entry.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/size.h"

#if defined(TOOLKIT_VIEWS)
#include "views/focus/accelerator_handler.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/desktop.h"
#endif

namespace ui_test_utils {

namespace {

class DOMOperationObserver : public content::NotificationObserver {
 public:
  explicit DOMOperationObserver(RenderViewHost* render_view_host)
      : did_respond_(false) {
    registrar_.Add(this, chrome::NOTIFICATION_DOM_OPERATION_RESPONSE,
                   content::Source<RenderViewHost>(render_view_host));
    ui_test_utils::RunMessageLoop();
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    DCHECK(type == chrome::NOTIFICATION_DOM_OPERATION_RESPONSE);
    content::Details<DomOperationNotificationDetails> dom_op_details(details);
    response_ = dom_op_details->json();
    did_respond_ = true;
    MessageLoopForUI::current()->Quit();
  }

  bool GetResponse(std::string* response) WARN_UNUSED_RESULT {
    *response = response_;
    return did_respond_;
  }

 private:
  content::NotificationRegistrar registrar_;
  std::string response_;
  bool did_respond_;

  DISALLOW_COPY_AND_ASSIGN(DOMOperationObserver);
};

class FindInPageNotificationObserver : public content::NotificationObserver {
 public:
  explicit FindInPageNotificationObserver(TabContentsWrapper* parent_tab)
      : parent_tab_(parent_tab),
        active_match_ordinal_(-1),
        number_of_matches_(0) {
    current_find_request_id_ =
        parent_tab->find_tab_helper()->current_find_request_id();
    registrar_.Add(this, chrome::NOTIFICATION_FIND_RESULT_AVAILABLE,
                   content::Source<TabContents>(parent_tab_->tab_contents()));
    ui_test_utils::RunMessageLoop();
  }

  int active_match_ordinal() const { return active_match_ordinal_; }

  int number_of_matches() const { return number_of_matches_; }

  virtual void Observe(int type, const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    if (type == chrome::NOTIFICATION_FIND_RESULT_AVAILABLE) {
      content::Details<FindNotificationDetails> find_details(details);
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
  content::NotificationRegistrar registrar_;
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
#if defined(USE_AURA)
  aura::Desktop::GetInstance()->Run();
#elif defined(TOOLKIT_VIEWS)
  views::AcceleratorHandler handler;
  loop->RunWithDispatcher(&handler);
#elif defined(OS_POSIX) && !defined(OS_MACOSX)
  loop->RunWithDispatcher(NULL);
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
  title->assign(last_entry->GetTitleForDisplay(""));
  return true;
}

void WaitForNavigations(NavigationController* controller,
                        int number_of_navigations) {
  TestNavigationObserver observer(
      content::Source<NavigationController>(controller), NULL,
      number_of_navigations);
  observer.WaitForObservation();
}

void WaitForNewTab(Browser* browser) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, content::NOTIFICATION_TAB_ADDED,
                  content::Source<TabContentsDelegate>(browser));
}

void WaitForBrowserActionUpdated(ExtensionAction* browser_action) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer,
                  chrome::NOTIFICATION_EXTENSION_BROWSER_ACTION_UPDATED,
                  content::Source<ExtensionAction>(browser_action));
}

void WaitForLoadStop(TabContents* tab) {
  WindowedNotificationObserver load_stop_observer(
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(&tab->controller()));
  // In many cases, the load may have finished before we get here.  Only wait if
  // the tab still has a pending navigation.
  if (!tab->IsLoading())
    return;
  load_stop_observer.Wait();
}

Browser* WaitForNewBrowser() {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, chrome::NOTIFICATION_BROWSER_WINDOW_READY,
                   content::NotificationService::AllSources());
  return content::Source<Browser>(observer.source()).ptr();
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
  Browser* browser = BrowserList::FindTabbedBrowser(
      profile->GetOffTheRecordProfile(), false);
  WaitForNavigations(&browser->GetSelectedTabContents()->controller(), 1);
}

void NavigateToURL(browser::NavigateParams* params) {
  TestNavigationObserver observer(
      content::NotificationService::AllSources(), NULL, 1);
  browser::Navigate(params);
  observer.WaitForObservation();
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
  if (disposition == CURRENT_TAB && browser->GetSelectedTabContents())
    WaitForLoadStop(browser->GetSelectedTabContents());
  TestNavigationObserver same_tab_observer(
      content::Source<NavigationController>(
          &browser->GetSelectedTabContents()->controller()),
      NULL,
      number_of_navigations);

  std::set<Browser*> initial_browsers;
  for (std::vector<Browser*>::const_iterator iter = BrowserList::begin();
       iter != BrowserList::end();
       ++iter) {
    initial_browsers.insert(*iter);
  }

  WindowedNotificationObserver tab_added_observer(
      content::NOTIFICATION_TAB_ADDED,
      content::NotificationService::AllSources());

  browser->OpenURL(url, GURL(), disposition, content::PAGE_TRANSITION_TYPED);
  if (browser_test_flags & BROWSER_TEST_WAIT_FOR_BROWSER)
    browser = WaitForBrowserNotInSet(initial_browsers);
  if (browser_test_flags & BROWSER_TEST_WAIT_FOR_TAB)
    tab_added_observer.Wait();
  if (!(browser_test_flags & BROWSER_TEST_WAIT_FOR_NAVIGATION)) {
    // Some other flag caused the wait prior to this.
    return;
  }
  TabContents* tab_contents = NULL;
  if (disposition == NEW_BACKGROUND_TAB) {
    // We've opened up a new tab, but not selected it.
    tab_contents = browser->GetTabContentsAt(browser->active_index() + 1);
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
  if (disposition == CURRENT_TAB) {
    same_tab_observer.WaitForObservation();
    return;
  } else if (tab_contents) {
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

GURL GetFileUrlWithQuery(const FilePath& path,
                         const std::string& query_string) {
  GURL url = net::FilePathToFileURL(path);
  if (!query_string.empty()) {
    GURL::Replacements replacements;
    replacements.SetQueryStr(query_string);
    return url.ReplaceComponents(replacements);
  }
  return url;
}

AppModalDialog* WaitForAppModalDialog() {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, chrome::NOTIFICATION_APP_MODAL_DIALOG_SHOWN,
                  content::NotificationService::AllSources());
  return content::Source<AppModalDialog>(observer.source()).ptr();
}

void CrashTab(TabContents* tab) {
  content::RenderProcessHost* rph = tab->render_view_host()->process();
  base::KillProcess(rph->GetHandle(), 0, false);
  TestNotificationObserver observer;
  RegisterAndWait(&observer, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                  content::Source<content::RenderProcessHost>(rph));
}

void WaitForFocusChange(TabContents* tab_contents) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, content::NOTIFICATION_FOCUS_CHANGED_IN_PAGE,
                  content::Source<TabContents>(tab_contents));
}

void WaitForFocusInBrowser(Browser* browser) {
  TestNotificationObserver observer;
  RegisterAndWait(&observer, chrome::NOTIFICATION_FOCUS_RETURNED_TO_BROWSER,
                  content::Source<Browser>(browser));
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

void RegisterAndWait(content::NotificationObserver* observer,
                     int type,
                     const content::NotificationSource& source) {
  content::NotificationRegistrar registrar;
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

void WaitForTemplateURLServiceToLoad(TemplateURLService* service) {
  if (service->loaded())
    return;
  service->Load();
  TemplateURLServiceTestUtil::BlockTillServiceProcessesRequests();
  ASSERT_TRUE(service->loaded());
}

void WaitForHistoryToLoad(Browser* browser) {
  HistoryService* history_service =
      browser->profile()->GetHistoryService(Profile::EXPLICIT_ACCESS);
  WindowedNotificationObserver history_loaded_observer(
      chrome::NOTIFICATION_HISTORY_LOADED,
      content::NotificationService::AllSources());
  if (!history_service->BackendLoaded())
    history_loaded_observer.Wait();
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

  if (!ui_controls::SendKeyPressNotifyWhenDone(
          window, key, control, shift, alt, command,
          MessageLoop::QuitClosure())) {
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
                         int type,
                         const content::NotificationSource& source) {
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
  // Append CommandLine arguments after the server script, switches won't work.
  scoped_ptr<CommandLine> cmd_line(CreateWebSocketServerCommandLine());
  cmd_line->AppendArg("--server=start");
  cmd_line->AppendArg("--chromium");
  cmd_line->AppendArg("--register_cygwin");
  cmd_line->AppendArgNative(FILE_PATH_LITERAL("--root=") +
                            root_directory.value());
  if (!temp_dir_.CreateUniqueTempDir()) {
    LOG(ERROR) << "Unable to create a temporary directory.";
    return false;
  }
  websocket_pid_file_ = temp_dir_.path().AppendASCII("websocket.pid");
  cmd_line->AppendArgNative(FILE_PATH_LITERAL("--pidfile=") +
                            websocket_pid_file_.value());
  SetPythonPath();
  base::LaunchOptions options;
  options.wait = true;
  if (!base::LaunchProcess(*cmd_line.get(), options, NULL)) {
    LOG(ERROR) << "Unable to launch websocket server.";
    return false;
  }
  started_ = true;
  return true;
}

CommandLine* TestWebSocketServer::CreatePythonCommandLine() {
  // Note: Python's first argument must be the script; do not append CommandLine
  // switches, as they would precede the script path and break this CommandLine.
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
  // Append CommandLine arguments after the server script, switches won't work.
  scoped_ptr<CommandLine> cmd_line(CreateWebSocketServerCommandLine());
  cmd_line->AppendArg("--server=stop");
  cmd_line->AppendArg("--chromium");
  cmd_line->AppendArgNative(FILE_PATH_LITERAL("--pidfile=") +
                            websocket_pid_file_.value());
  base::LaunchOptions options;
  options.wait = true;
  base::LaunchProcess(*cmd_line.get(), options, NULL);
}

TestNotificationObserver::TestNotificationObserver()
    : source_(content::NotificationService::AllSources()) {
}

TestNotificationObserver::~TestNotificationObserver() {}

void TestNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  source_ = source;
  details_ = details;
  MessageLoopForUI::current()->Quit();
}

WindowedNotificationObserver::WindowedNotificationObserver(
    int notification_type,
    const content::NotificationSource& source)
    : seen_(false),
      running_(false),
      waiting_for_(source) {
  registrar_.Add(this, notification_type, waiting_for_);
}

WindowedNotificationObserver::~WindowedNotificationObserver() {}

void WindowedNotificationObserver::Wait() {
  if (seen_ || (waiting_for_ == content::NotificationService::AllSources() &&
                !sources_seen_.empty())) {
    return;
  }

  running_ = true;
  ui_test_utils::RunMessageLoop();
}

void WindowedNotificationObserver::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (waiting_for_ == source ||
      (running_ && waiting_for_ == content::NotificationService::AllSources())) {
    seen_ = true;
    if (running_)
      MessageLoopForUI::current()->Quit();
  } else {
    sources_seen_.insert(source.map_key());
  }
}

TitleWatcher::TitleWatcher(TabContents* tab_contents,
                           const string16& expected_title)
    : tab_contents_(tab_contents),
      expected_title_observed_(false),
      quit_loop_on_observation_(false) {
  EXPECT_TRUE(tab_contents != NULL);
  expected_titles_.push_back(expected_title);
  notification_registrar_.Add(this,
                              content::NOTIFICATION_TAB_CONTENTS_TITLE_UPDATED,
                              content::Source<TabContents>(tab_contents));

  // When navigating through the history, the restored NavigationEntry's title
  // will be used. If the entry ends up having the same title after we return
  // to it, as will usually be the case, the
  // NOTIFICATION_TAB_CONTENTS_TITLE_UPDATED will then be suppressed, since the
  // NavigationEntry's title hasn't changed.
  notification_registrar_.Add(
      this,
      content::NOTIFICATION_LOAD_STOP,
      content::Source<NavigationController>(&tab_contents->controller()));
}

void TitleWatcher::AlsoWaitForTitle(const string16& expected_title) {
  expected_titles_.push_back(expected_title);
}

TitleWatcher::~TitleWatcher() {
}

const string16& TitleWatcher::WaitAndGetTitle() {
  if (expected_title_observed_)
    return observed_title_;
  quit_loop_on_observation_ = true;
  ui_test_utils::RunMessageLoop();
  return observed_title_;
}

void TitleWatcher::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  if (type == content::NOTIFICATION_TAB_CONTENTS_TITLE_UPDATED) {
    TabContents* source_contents = content::Source<TabContents>(source).ptr();
    ASSERT_EQ(tab_contents_, source_contents);
  } else if (type == content::NOTIFICATION_LOAD_STOP) {
    NavigationController* controller =
        content::Source<NavigationController>(source).ptr();
    ASSERT_EQ(&tab_contents_->controller(), controller);
  } else {
    FAIL() << "Unexpected notification received.";
  }

  std::vector<string16>::const_iterator it =
      std::find(expected_titles_.begin(),
                expected_titles_.end(),
                tab_contents_->GetTitle());
  if (it == expected_titles_.end())
    return;
  observed_title_ = *it;
  expected_title_observed_ = true;
  if (quit_loop_on_observation_)
    MessageLoopForUI::current()->Quit();
}

DOMMessageQueue::DOMMessageQueue() {
  registrar_.Add(this, chrome::NOTIFICATION_DOM_OPERATION_RESPONSE,
                 content::NotificationService::AllSources());
}

DOMMessageQueue::~DOMMessageQueue() {}

void DOMMessageQueue::Observe(int type,
                              const content::NotificationSource& source,
                              const content::NotificationDetails& details) {
  content::Details<DomOperationNotificationDetails> dom_op_details(details);
  content::Source<RenderViewHost> sender(source);
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
        base::Bind(&SnapshotTaker::OnSnapshotTaken, base::Unretained(this)),
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
