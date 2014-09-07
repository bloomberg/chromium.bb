// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/process/kill.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/dom_operation_notification_details.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/cookies/cookie_store.h"
#include "net/test/python_utils.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/events/keycodes/dom4/keycode_converter.h"
#include "ui/resources/grit/webui_resources.h"

#if defined(USE_AURA)
#include "ui/aura/test/window_event_dispatcher_test_api.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#endif  // USE_AURA

namespace content {
namespace {

class DOMOperationObserver : public NotificationObserver,
                             public WebContentsObserver {
 public:
  explicit DOMOperationObserver(RenderViewHost* rvh)
      : WebContentsObserver(WebContents::FromRenderViewHost(rvh)),
        did_respond_(false) {
    registrar_.Add(this, NOTIFICATION_DOM_OPERATION_RESPONSE,
                   Source<WebContents>(web_contents()));
    message_loop_runner_ = new MessageLoopRunner;
  }

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE {
    DCHECK(type == NOTIFICATION_DOM_OPERATION_RESPONSE);
    Details<DomOperationNotificationDetails> dom_op_details(details);
    response_ = dom_op_details->json;
    did_respond_ = true;
    message_loop_runner_->Quit();
  }

  // Overridden from WebContentsObserver:
  virtual void RenderProcessGone(base::TerminationStatus status) OVERRIDE {
    message_loop_runner_->Quit();
  }

  bool WaitAndGetResponse(std::string* response) WARN_UNUSED_RESULT {
    message_loop_runner_->Run();
    *response = response_;
    return did_respond_;
  }

 private:
  NotificationRegistrar registrar_;
  std::string response_;
  bool did_respond_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(DOMOperationObserver);
};

// Specifying a prototype so that we can add the WARN_UNUSED_RESULT attribute.
bool ExecuteScriptHelper(
    RenderFrameHost* render_frame_host,
    const std::string& original_script,
    scoped_ptr<base::Value>* result) WARN_UNUSED_RESULT;

// Executes the passed |original_script| in the frame specified by
// |render_frame_host|.  If |result| is not NULL, stores the value that the
// evaluation of the script in |result|.  Returns true on success.
bool ExecuteScriptHelper(RenderFrameHost* render_frame_host,
                         const std::string& original_script,
                         scoped_ptr<base::Value>* result) {
  // TODO(jcampan): we should make the domAutomationController not require an
  //                automation id.
  std::string script =
      "window.domAutomationController.setAutomationId(0);" + original_script;
  DOMOperationObserver dom_op_observer(render_frame_host->GetRenderViewHost());
  render_frame_host->ExecuteJavaScriptForTests(base::UTF8ToUTF16(script));
  std::string json;
  if (!dom_op_observer.WaitAndGetResponse(&json)) {
    DLOG(ERROR) << "Cannot communicate with DOMOperationObserver.";
    return false;
  }

  // Nothing more to do for callers that ignore the returned JS value.
  if (!result)
    return true;

  base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
  result->reset(reader.ReadToValue(json));
  if (!result->get()) {
    DLOG(ERROR) << reader.GetErrorMessage();
    return false;
  }

  return true;
}

void BuildSimpleWebKeyEvent(blink::WebInputEvent::Type type,
                            ui::KeyboardCode key_code,
                            int native_key_code,
                            int modifiers,
                            NativeWebKeyboardEvent* event) {
  event->nativeKeyCode = native_key_code;
  event->windowsKeyCode = key_code;
  event->setKeyIdentifierFromWindowsKeyCode();
  event->type = type;
  event->modifiers = modifiers;
  event->isSystemKey = false;
  event->timeStampSeconds = base::Time::Now().ToDoubleT();
  event->skip_in_browser = true;

  if (type == blink::WebInputEvent::Char ||
      type == blink::WebInputEvent::RawKeyDown) {
    event->text[0] = key_code;
    event->unmodifiedText[0] = key_code;
  }
}

void InjectRawKeyEvent(WebContents* web_contents,
                       blink::WebInputEvent::Type type,
                       ui::KeyboardCode key_code,
                       int native_key_code,
                       int modifiers) {
  NativeWebKeyboardEvent event;
  BuildSimpleWebKeyEvent(type, key_code, native_key_code, modifiers, &event);
  web_contents->GetRenderViewHost()->ForwardKeyboardEvent(event);
}

void GetCookiesCallback(std::string* cookies_out,
                        base::WaitableEvent* event,
                        const std::string& cookies) {
  *cookies_out = cookies;
  event->Signal();
}

void GetCookiesOnIOThread(const GURL& url,
                          net::URLRequestContextGetter* context_getter,
                          base::WaitableEvent* event,
                          std::string* cookies) {
  net::CookieStore* cookie_store =
      context_getter->GetURLRequestContext()->cookie_store();
  cookie_store->GetCookiesWithOptionsAsync(
      url, net::CookieOptions(),
      base::Bind(&GetCookiesCallback, cookies, event));
}

void SetCookieCallback(bool* result,
                       base::WaitableEvent* event,
                       bool success) {
  *result = success;
  event->Signal();
}

void SetCookieOnIOThread(const GURL& url,
                         const std::string& value,
                         net::URLRequestContextGetter* context_getter,
                         base::WaitableEvent* event,
                         bool* result) {
  net::CookieStore* cookie_store =
      context_getter->GetURLRequestContext()->cookie_store();
  cookie_store->SetCookieWithOptionsAsync(
      url, value, net::CookieOptions(),
      base::Bind(&SetCookieCallback, result, event));
}

}  // namespace


GURL GetFileUrlWithQuery(const base::FilePath& path,
                         const std::string& query_string) {
  GURL url = net::FilePathToFileURL(path);
  if (!query_string.empty()) {
    GURL::Replacements replacements;
    replacements.SetQueryStr(query_string);
    return url.ReplaceComponents(replacements);
  }
  return url;
}

void WaitForLoadStop(WebContents* web_contents) {
  // In many cases, the load may have finished before we get here.  Only wait if
  // the tab still has a pending navigation.
  if (web_contents->IsLoading()) {
    WindowedNotificationObserver load_stop_observer(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(&web_contents->GetController()));
    load_stop_observer.Wait();
  }
}

void CrashTab(WebContents* web_contents) {
  RenderProcessHost* rph = web_contents->GetRenderProcessHost();
  RenderProcessHostWatcher watcher(
      rph, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  base::KillProcess(rph->GetHandle(), 0, false);
  watcher.Wait();
}

#if defined(USE_AURA)
bool IsResizeComplete(aura::test::WindowEventDispatcherTestApi* dispatcher_test,
                      RenderWidgetHostImpl* widget_host) {
  return !dispatcher_test->HoldingPointerMoves() &&
      !widget_host->resize_ack_pending_for_testing();
}

void WaitForResizeComplete(WebContents* web_contents) {
  aura::Window* content = web_contents->GetContentNativeView();
  if (!content)
    return;

  aura::WindowTreeHost* window_host = content->GetHost();
  aura::WindowEventDispatcher* dispatcher = window_host->dispatcher();
  aura::test::WindowEventDispatcherTestApi dispatcher_test(dispatcher);
  RenderWidgetHostImpl* widget_host =
      RenderWidgetHostImpl::From(web_contents->GetRenderViewHost());
  if (!IsResizeComplete(&dispatcher_test, widget_host)) {
    WindowedNotificationObserver resize_observer(
        NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
        base::Bind(IsResizeComplete, &dispatcher_test, widget_host));
    resize_observer.Wait();
  }
}
#endif  // USE_AURA

void SimulateMouseClick(WebContents* web_contents,
                        int modifiers,
                        blink::WebMouseEvent::Button button) {
  int x = web_contents->GetContainerBounds().width() / 2;
  int y = web_contents->GetContainerBounds().height() / 2;
  SimulateMouseClickAt(web_contents, modifiers, button, gfx::Point(x, y));
}

void SimulateMouseClickAt(WebContents* web_contents,
                          int modifiers,
                          blink::WebMouseEvent::Button button,
                          const gfx::Point& point) {
  blink::WebMouseEvent mouse_event;
  mouse_event.type = blink::WebInputEvent::MouseDown;
  mouse_event.button = button;
  mouse_event.x = point.x();
  mouse_event.y = point.y();
  mouse_event.modifiers = modifiers;
  // Mac needs globalX/globalY for events to plugins.
  gfx::Rect offset = web_contents->GetContainerBounds();
  mouse_event.globalX = point.x() + offset.x();
  mouse_event.globalY = point.y() + offset.y();
  mouse_event.clickCount = 1;
  web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
  mouse_event.type = blink::WebInputEvent::MouseUp;
  web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
}

void SimulateMouseEvent(WebContents* web_contents,
                        blink::WebInputEvent::Type type,
                        const gfx::Point& point) {
  blink::WebMouseEvent mouse_event;
  mouse_event.type = type;
  mouse_event.x = point.x();
  mouse_event.y = point.y();
  web_contents->GetRenderViewHost()->ForwardMouseEvent(mouse_event);
}

void SimulateTapAt(WebContents* web_contents, const gfx::Point& point) {
  blink::WebGestureEvent tap;
  tap.type = blink::WebGestureEvent::GestureTap;
  tap.x = point.x();
  tap.y = point.y();
  RenderWidgetHostImpl* widget_host =
      RenderWidgetHostImpl::From(web_contents->GetRenderViewHost());
  widget_host->ForwardGestureEvent(tap);
}

void SimulateKeyPress(WebContents* web_contents,
                      ui::KeyboardCode key_code,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) {
  SimulateKeyPressWithCode(
      web_contents, key_code, NULL, control, shift, alt, command);
}

void SimulateKeyPressWithCode(WebContents* web_contents,
                              ui::KeyboardCode key_code,
                              const char* code,
                              bool control,
                              bool shift,
                              bool alt,
                              bool command) {
  int native_key_code = ui::KeycodeConverter::CodeToNativeKeycode(code);

  int modifiers = 0;

  // The order of these key down events shouldn't matter for our simulation.
  // For our simulation we can use either the left keys or the right keys.
  if (control) {
    modifiers |= blink::WebInputEvent::ControlKey;
    InjectRawKeyEvent(web_contents,
                      blink::WebInputEvent::RawKeyDown,
                      ui::VKEY_CONTROL,
                      ui::KeycodeConverter::CodeToNativeKeycode("ControlLeft"),
                      modifiers);
  }

  if (shift) {
    modifiers |= blink::WebInputEvent::ShiftKey;
    InjectRawKeyEvent(web_contents,
                      blink::WebInputEvent::RawKeyDown,
                      ui::VKEY_SHIFT,
                      ui::KeycodeConverter::CodeToNativeKeycode("ShiftLeft"),
                      modifiers);
  }

  if (alt) {
    modifiers |= blink::WebInputEvent::AltKey;
    InjectRawKeyEvent(web_contents,
                      blink::WebInputEvent::RawKeyDown,
                      ui::VKEY_MENU,
                      ui::KeycodeConverter::CodeToNativeKeycode("AltLeft"),
                      modifiers);
  }

  if (command) {
    modifiers |= blink::WebInputEvent::MetaKey;
    InjectRawKeyEvent(web_contents,
                      blink::WebInputEvent::RawKeyDown,
                      ui::VKEY_COMMAND,
                      ui::KeycodeConverter::CodeToNativeKeycode("OSLeft"),
                      modifiers);
  }

  InjectRawKeyEvent(
      web_contents,
      blink::WebInputEvent::RawKeyDown,
      key_code,
      native_key_code,
      modifiers);

  InjectRawKeyEvent(
      web_contents,
      blink::WebInputEvent::Char,
      key_code,
      native_key_code,
      modifiers);

  InjectRawKeyEvent(
      web_contents,
      blink::WebInputEvent::KeyUp,
      key_code,
      native_key_code,
      modifiers);

  // The order of these key releases shouldn't matter for our simulation.
  if (control) {
    modifiers &= ~blink::WebInputEvent::ControlKey;
    InjectRawKeyEvent(web_contents,
                      blink::WebInputEvent::KeyUp,
                      ui::VKEY_CONTROL,
                      ui::KeycodeConverter::CodeToNativeKeycode("ControlLeft"),
                      modifiers);
  }

  if (shift) {
    modifiers &= ~blink::WebInputEvent::ShiftKey;
    InjectRawKeyEvent(web_contents,
                      blink::WebInputEvent::KeyUp,
                      ui::VKEY_SHIFT,
                      ui::KeycodeConverter::CodeToNativeKeycode("ShiftLeft"),
                      modifiers);
  }

  if (alt) {
    modifiers &= ~blink::WebInputEvent::AltKey;
    InjectRawKeyEvent(web_contents,
                      blink::WebInputEvent::KeyUp,
                      ui::VKEY_MENU,
                      ui::KeycodeConverter::CodeToNativeKeycode("AltLeft"),
                      modifiers);
  }

  if (command) {
    modifiers &= ~blink::WebInputEvent::MetaKey;
    InjectRawKeyEvent(web_contents,
                      blink::WebInputEvent::KeyUp,
                      ui::VKEY_COMMAND,
                      ui::KeycodeConverter::CodeToNativeKeycode("OSLeft"),
                      modifiers);
  }

  ASSERT_EQ(modifiers, 0);
}

namespace internal {

ToRenderFrameHost::ToRenderFrameHost(WebContents* web_contents)
    : render_frame_host_(web_contents->GetMainFrame()) {
}

ToRenderFrameHost::ToRenderFrameHost(RenderViewHost* render_view_host)
    : render_frame_host_(render_view_host->GetMainFrame()) {
}

ToRenderFrameHost::ToRenderFrameHost(RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
}

}  // namespace internal

bool ExecuteScript(const internal::ToRenderFrameHost& adapter,
                   const std::string& script) {
  std::string new_script =
      script + ";window.domAutomationController.send(0);";
  return ExecuteScriptHelper(adapter.render_frame_host(), new_script, NULL);
}

bool ExecuteScriptAndExtractInt(const internal::ToRenderFrameHost& adapter,
                                const std::string& script, int* result) {
  DCHECK(result);
  scoped_ptr<base::Value> value;
  if (!ExecuteScriptHelper(adapter.render_frame_host(), script, &value) ||
      !value.get()) {
    return false;
  }

  return value->GetAsInteger(result);
}

bool ExecuteScriptAndExtractBool(const internal::ToRenderFrameHost& adapter,
                                 const std::string& script, bool* result) {
  DCHECK(result);
  scoped_ptr<base::Value> value;
  if (!ExecuteScriptHelper(adapter.render_frame_host(), script, &value) ||
      !value.get()) {
    return false;
  }

  return value->GetAsBoolean(result);
}

bool ExecuteScriptAndExtractString(const internal::ToRenderFrameHost& adapter,
                                   const std::string& script,
                                   std::string* result) {
  DCHECK(result);
  scoped_ptr<base::Value> value;
  if (!ExecuteScriptHelper(adapter.render_frame_host(), script, &value) ||
      !value.get()) {
    return false;
  }

  return value->GetAsString(result);
}

namespace {
void AddToSetIfFrameMatchesPredicate(
    std::set<RenderFrameHost*>* frame_set,
    const base::Callback<bool(RenderFrameHost*)>& predicate,
    RenderFrameHost* host) {
  if (predicate.Run(host))
    frame_set->insert(host);
}
}

RenderFrameHost* FrameMatchingPredicate(
    WebContents* web_contents,
    const base::Callback<bool(RenderFrameHost*)>& predicate) {
  std::set<RenderFrameHost*> frame_set;
  web_contents->ForEachFrame(
      base::Bind(&AddToSetIfFrameMatchesPredicate, &frame_set, predicate));
  DCHECK_EQ(1U, frame_set.size());
  return *frame_set.begin();
}

bool FrameMatchesName(const std::string& name, RenderFrameHost* frame) {
  return frame->GetFrameName() == name;
}

bool FrameIsChildOfMainFrame(RenderFrameHost* frame) {
  return frame->GetParent() && !frame->GetParent()->GetParent();
}

bool FrameHasSourceUrl(const GURL& url, RenderFrameHost* frame) {
  return frame->GetLastCommittedURL() == url;
}

bool ExecuteWebUIResourceTest(WebContents* web_contents,
                              const std::vector<int>& js_resource_ids) {
  // Inject WebUI test runner script first prior to other scripts required to
  // run the test as scripts may depend on it being declared.
  std::vector<int> ids;
  ids.push_back(IDR_WEBUI_JS_WEBUI_RESOURCE_TEST);
  ids.insert(ids.end(), js_resource_ids.begin(), js_resource_ids.end());

  std::string script;
  for (std::vector<int>::iterator iter = ids.begin();
       iter != ids.end();
       ++iter) {
    ResourceBundle::GetSharedInstance().GetRawDataResource(*iter)
        .AppendToString(&script);
    script.append("\n");
  }
  if (!ExecuteScript(web_contents, script))
    return false;

  DOMMessageQueue message_queue;
  if (!ExecuteScript(web_contents, "runTests()"))
    return false;

  std::string message;
  do {
    if (!message_queue.WaitForMessage(&message))
      return false;
  } while (message.compare("\"PENDING\"") == 0);

  return message.compare("\"SUCCESS\"") == 0;
}

std::string GetCookies(BrowserContext* browser_context, const GURL& url) {
  std::string cookies;
  base::WaitableEvent event(true, false);
  net::URLRequestContextGetter* context_getter =
      browser_context->GetRequestContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetCookiesOnIOThread, url,
                 make_scoped_refptr(context_getter), &event, &cookies));
  event.Wait();
  return cookies;
}

bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value) {
  bool result = false;
  base::WaitableEvent event(true, false);
  net::URLRequestContextGetter* context_getter =
      browser_context->GetRequestContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SetCookieOnIOThread, url, value,
                 make_scoped_refptr(context_getter), &event, &result));
  event.Wait();
  return result;
}

void FetchHistogramsFromChildProcesses() {
  scoped_refptr<content::MessageLoopRunner> runner = new MessageLoopRunner;

  FetchHistogramsAsynchronously(
      base::MessageLoop::current(),
      runner->QuitClosure(),
      // If this call times out, it means that a child process is not
      // responding, which is something we should not ignore.  The timeout is
      // set to be longer than the normal browser test timeout so that it will
      // be prempted by the normal timeout.
      TestTimeouts::action_max_timeout());
  runner->Run();
}

TitleWatcher::TitleWatcher(WebContents* web_contents,
                           const base::string16& expected_title)
    : WebContentsObserver(web_contents),
      message_loop_runner_(new MessageLoopRunner) {
  EXPECT_TRUE(web_contents != NULL);
  expected_titles_.push_back(expected_title);
}

void TitleWatcher::AlsoWaitForTitle(const base::string16& expected_title) {
  expected_titles_.push_back(expected_title);
}

TitleWatcher::~TitleWatcher() {
}

const base::string16& TitleWatcher::WaitAndGetTitle() {
  TestTitle();
  message_loop_runner_->Run();
  return observed_title_;
}

void TitleWatcher::DidStopLoading(RenderViewHost* render_view_host) {
  // When navigating through the history, the restored NavigationEntry's title
  // will be used. If the entry ends up having the same title after we return
  // to it, as will usually be the case, then WebContentsObserver::TitleSet
  // will then be suppressed, since the NavigationEntry's title hasn't changed.
  TestTitle();
}

void TitleWatcher::TitleWasSet(NavigationEntry* entry, bool explicit_set) {
  TestTitle();
}

void TitleWatcher::TestTitle() {
  std::vector<base::string16>::const_iterator it =
      std::find(expected_titles_.begin(),
                expected_titles_.end(),
                web_contents()->GetTitle());
  if (it == expected_titles_.end())
    return;

  observed_title_ = *it;
  message_loop_runner_->Quit();
}

WebContentsDestroyedWatcher::WebContentsDestroyedWatcher(
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      message_loop_runner_(new MessageLoopRunner) {
  EXPECT_TRUE(web_contents != NULL);
}

WebContentsDestroyedWatcher::~WebContentsDestroyedWatcher() {
}

void WebContentsDestroyedWatcher::Wait() {
  message_loop_runner_->Run();
}

void WebContentsDestroyedWatcher::WebContentsDestroyed() {
  message_loop_runner_->Quit();
}

RenderProcessHostWatcher::RenderProcessHostWatcher(
    RenderProcessHost* render_process_host, WatchType type)
    : render_process_host_(render_process_host),
      type_(type),
      message_loop_runner_(new MessageLoopRunner) {
  render_process_host_->AddObserver(this);
}

RenderProcessHostWatcher::RenderProcessHostWatcher(
    WebContents* web_contents, WatchType type)
    : render_process_host_(web_contents->GetRenderProcessHost()),
      type_(type),
      message_loop_runner_(new MessageLoopRunner) {
  render_process_host_->AddObserver(this);
}

RenderProcessHostWatcher::~RenderProcessHostWatcher() {
  if (render_process_host_)
    render_process_host_->RemoveObserver(this);
}

void RenderProcessHostWatcher::Wait() {
  message_loop_runner_->Run();
}

void RenderProcessHostWatcher::RenderProcessExited(
    RenderProcessHost* host,
    base::ProcessHandle handle,
    base::TerminationStatus status,
    int exit_code) {
  if (type_ == WATCH_FOR_PROCESS_EXIT)
    message_loop_runner_->Quit();
}

void RenderProcessHostWatcher::RenderProcessHostDestroyed(
    RenderProcessHost* host) {
  render_process_host_ = NULL;
  if (type_ == WATCH_FOR_HOST_DESTRUCTION)
    message_loop_runner_->Quit();
}

DOMMessageQueue::DOMMessageQueue() {
  registrar_.Add(this, NOTIFICATION_DOM_OPERATION_RESPONSE,
                 NotificationService::AllSources());
}

DOMMessageQueue::~DOMMessageQueue() {}

void DOMMessageQueue::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  Details<DomOperationNotificationDetails> dom_op_details(details);
  message_queue_.push(dom_op_details->json);
  if (message_loop_runner_.get())
    message_loop_runner_->Quit();
}

void DOMMessageQueue::ClearQueue() {
  message_queue_ = std::queue<std::string>();
}

bool DOMMessageQueue::WaitForMessage(std::string* message) {
  DCHECK(message);
  if (message_queue_.empty()) {
    // This will be quit when a new message comes in.
    message_loop_runner_ = new MessageLoopRunner;
    message_loop_runner_->Run();
  }
  // The queue should not be empty, unless we were quit because of a timeout.
  if (message_queue_.empty())
    return false;
  *message = message_queue_.front();
  message_queue_.pop();
  return true;
}

}  // namespace content
