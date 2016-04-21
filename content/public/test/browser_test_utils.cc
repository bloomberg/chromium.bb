// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#include <stddef.h>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_mode_helper.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/base/filename_util.h"
#include "net/cookies/cookie_store.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/python_utils.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/latency_info.h"
#include "ui/resources/grit/webui_resources.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/aura/test/window_event_dispatcher_test_api.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#endif  // USE_AURA

namespace content {
namespace {

class DOMOperationObserver : public NotificationObserver,
                             public WebContentsObserver {
 public:
  explicit DOMOperationObserver(RenderFrameHost* rfh)
      : WebContentsObserver(WebContents::FromRenderFrameHost(rfh)),
        did_respond_(false) {
    registrar_.Add(this, NOTIFICATION_DOM_OPERATION_RESPONSE,
                   Source<WebContents>(web_contents()));
    message_loop_runner_ = new MessageLoopRunner;
  }

  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override {
    DCHECK(type == NOTIFICATION_DOM_OPERATION_RESPONSE);
    Details<std::string> dom_op_result(details);
    if (!did_respond_) {
      response_ = *dom_op_result.ptr();
      did_respond_ = true;
      message_loop_runner_->Quit();
    }
  }

  // Overridden from WebContentsObserver:
  void RenderProcessGone(base::TerminationStatus status) override {
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

class InterstitialObserver : public content::WebContentsObserver {
 public:
  InterstitialObserver(content::WebContents* web_contents,
                       const base::Closure& attach_callback,
                       const base::Closure& detach_callback)
      : WebContentsObserver(web_contents),
        attach_callback_(attach_callback),
        detach_callback_(detach_callback) {
  }
  ~InterstitialObserver() override {}

  // WebContentsObserver methods:
  void DidAttachInterstitialPage() override { attach_callback_.Run(); }
  void DidDetachInterstitialPage() override { detach_callback_.Run(); }

 private:
  base::Closure attach_callback_;
  base::Closure detach_callback_;

  DISALLOW_COPY_AND_ASSIGN(InterstitialObserver);
};

// Specifying a prototype so that we can add the WARN_UNUSED_RESULT attribute.
bool ExecuteScriptHelper(RenderFrameHost* render_frame_host,
                         const std::string& original_script,
                         std::unique_ptr<base::Value>* result)
    WARN_UNUSED_RESULT;

// Executes the passed |original_script| in the frame specified by
// |render_frame_host|.  If |result| is not NULL, stores the value that the
// evaluation of the script in |result|.  Returns true on success.
bool ExecuteScriptHelper(RenderFrameHost* render_frame_host,
                         const std::string& original_script,
                         std::unique_ptr<base::Value>* result) {
  // TODO(jcampan): we should make the domAutomationController not require an
  //                automation id.
  std::string script =
      "window.domAutomationController.setAutomationId(0);" + original_script;
  DOMOperationObserver dom_op_observer(render_frame_host);
  render_frame_host->ExecuteJavaScriptWithUserGestureForTests(
      base::UTF8ToUTF16(script));
  std::string json;
  if (!dom_op_observer.WaitAndGetResponse(&json)) {
    DLOG(ERROR) << "Cannot communicate with DOMOperationObserver.";
    return false;
  }

  // Nothing more to do for callers that ignore the returned JS value.
  if (!result)
    return true;

  base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
  *result = reader.ReadToValue(json);
  if (!*result) {
    DLOG(ERROR) << reader.GetErrorMessage();
    return false;
  }

  return true;
}

// Specifying a prototype so that we can add the WARN_UNUSED_RESULT attribute.
bool ExecuteScriptInIsolatedWorldHelper(RenderFrameHost* render_frame_host,
                                        const int world_id,
                                        const std::string& original_script,
                                        std::unique_ptr<base::Value>* result)
    WARN_UNUSED_RESULT;

bool ExecuteScriptInIsolatedWorldHelper(RenderFrameHost* render_frame_host,
                                        const int world_id,
                                        const std::string& original_script,
                                        std::unique_ptr<base::Value>* result) {
  std::string script =
      "window.domAutomationController.setAutomationId(0);" + original_script;
  DOMOperationObserver dom_op_observer(render_frame_host);
  render_frame_host->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(script),
      content::RenderFrameHost::JavaScriptResultCallback(), world_id);
  std::string json;
  if (!dom_op_observer.WaitAndGetResponse(&json)) {
    DLOG(ERROR) << "Cannot communicate with DOMOperationObserver.";
    return false;
  }

  // Nothing more to do for callers that ignore the returned JS value.
  if (!result)
    return true;

  base::JSONReader reader(base::JSON_ALLOW_TRAILING_COMMAS);
  *result = reader.ReadToValue(json);
  if (!*result) {
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
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  RenderWidgetHostImpl* main_frame_rwh =
      web_contents_impl->GetMainFrame()->GetRenderWidgetHost();
  web_contents_impl->GetFocusedRenderWidgetHost(main_frame_rwh)
      ->ForwardKeyboardEvent(event);
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

std::unique_ptr<net::test_server::HttpResponse>
CrossSiteRedirectResponseHandler(const GURL& server_base_url,
                                 const net::test_server::HttpRequest& request) {
  std::string prefix("/cross-site/");
  if (!base::StartsWith(request.relative_url, prefix,
                        base::CompareCase::SENSITIVE))
    return std::unique_ptr<net::test_server::HttpResponse>();

  std::string params = request.relative_url.substr(prefix.length());

  // A hostname to redirect to must be included in the URL, therefore at least
  // one '/' character is expected.
  size_t slash = params.find('/');
  if (slash == std::string::npos)
    return std::unique_ptr<net::test_server::HttpResponse>();

  // Replace the host of the URL with the one passed in the URL.
  GURL::Replacements replace_host;
  replace_host.SetHostStr(base::StringPiece(params).substr(0, slash));
  GURL redirect_server = server_base_url.ReplaceComponents(replace_host);

  // Append the real part of the path to the new URL.
  std::string path = params.substr(slash + 1);
  GURL redirect_target(redirect_server.Resolve(path));
  DCHECK(redirect_target.is_valid());

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(net::HTTP_MOVED_PERMANENTLY);
  http_response->AddCustomHeader("Location", redirect_target.spec());
  return std::move(http_response);
}

}  // namespace

bool NavigateIframeToURL(WebContents* web_contents,
                         std::string iframe_id,
                         const GURL& url) {
  std::string script = base::StringPrintf(
      "setTimeout(\""
      "var iframes = document.getElementById('%s');iframes.src='%s';"
      "\",0)",
      iframe_id.c_str(), url.spec().c_str());
  TestNavigationObserver load_observer(web_contents);
  bool result = ExecuteScript(web_contents, script);
  load_observer.Wait();
  return result;
}

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

void WaitForLoadStopWithoutSuccessCheck(WebContents* web_contents) {
  // In many cases, the load may have finished before we get here.  Only wait if
  // the tab still has a pending navigation.
  if (web_contents->IsLoading()) {
    WindowedNotificationObserver load_stop_observer(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(&web_contents->GetController()));
    load_stop_observer.Wait();
  }
}

bool WaitForLoadStop(WebContents* web_contents) {
  WaitForLoadStopWithoutSuccessCheck(web_contents);
  return IsLastCommittedEntryOfPageType(web_contents, PAGE_TYPE_NORMAL);
}

bool IsLastCommittedEntryOfPageType(WebContents* web_contents,
                                    content::PageType page_type) {
  NavigationEntry* last_entry =
      web_contents->GetController().GetLastCommittedEntry();
  if (!last_entry)
    return false;
  return last_entry->GetPageType() == page_type;
}

void CrashTab(WebContents* web_contents) {
  RenderProcessHost* rph = web_contents->GetRenderProcessHost();
  RenderProcessHostWatcher watcher(
      rph, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  rph->Shutdown(0, false);
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
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  if (!IsResizeComplete(&dispatcher_test, widget_host)) {
    WindowedNotificationObserver resize_observer(
        NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
        base::Bind(IsResizeComplete, &dispatcher_test, widget_host));
    resize_observer.Wait();
  }
}
#elif defined(OS_ANDROID)
bool IsResizeComplete(RenderWidgetHostImpl* widget_host) {
  return !widget_host->resize_ack_pending_for_testing();
}

void WaitForResizeComplete(WebContents* web_contents) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  if (!IsResizeComplete(widget_host)) {
    WindowedNotificationObserver resize_observer(
        NOTIFICATION_RENDER_WIDGET_HOST_DID_UPDATE_BACKING_STORE,
        base::Bind(IsResizeComplete, widget_host));
    resize_observer.Wait();
  }
}
#endif

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
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
  mouse_event.type = blink::WebInputEvent::MouseUp;
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
}

void SimulateMouseEvent(WebContents* web_contents,
                        blink::WebInputEvent::Type type,
                        const gfx::Point& point) {
  blink::WebMouseEvent mouse_event;
  mouse_event.type = type;
  mouse_event.x = point.x();
  mouse_event.y = point.y();
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
}

void SimulateMouseWheelEvent(WebContents* web_contents,
                             const gfx::Point& point,
                             const gfx::Vector2d& delta) {
  blink::WebMouseWheelEvent wheel_event;
  wheel_event.type = blink::WebInputEvent::MouseWheel;
  wheel_event.x = point.x();
  wheel_event.y = point.y();
  wheel_event.deltaX = delta.x();
  wheel_event.deltaY = delta.y();
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardWheelEvent(wheel_event);
}

void SimulateGestureScrollSequence(WebContents* web_contents,
                                   const gfx::Point& point,
                                   const gfx::Vector2dF& delta) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());

  blink::WebGestureEvent scroll_begin;
  scroll_begin.type = blink::WebGestureEvent::GestureScrollBegin;
  scroll_begin.sourceDevice = blink::WebGestureDeviceTouchpad;
  scroll_begin.x = point.x();
  scroll_begin.y = point.y();
  widget_host->ForwardGestureEvent(scroll_begin);

  blink::WebGestureEvent scroll_update;
  scroll_update.type = blink::WebGestureEvent::GestureScrollUpdate;
  scroll_update.sourceDevice = blink::WebGestureDeviceTouchpad;
  scroll_update.x = point.x();
  scroll_update.y = point.y();
  scroll_update.data.scrollUpdate.deltaX = delta.x();
  scroll_update.data.scrollUpdate.deltaY = delta.y();
  scroll_update.data.scrollUpdate.velocityX = 0;
  scroll_update.data.scrollUpdate.velocityY = 0;
  widget_host->ForwardGestureEvent(scroll_update);

  blink::WebGestureEvent scroll_end;
  scroll_end.type = blink::WebGestureEvent::GestureScrollEnd;
  scroll_end.sourceDevice = blink::WebGestureDeviceTouchpad;
  scroll_end.x = point.x() + delta.x();
  scroll_end.y = point.y() + delta.y();
  widget_host->ForwardGestureEvent(scroll_end);
}

void SimulateTapAt(WebContents* web_contents, const gfx::Point& point) {
  blink::WebGestureEvent tap;
  tap.type = blink::WebGestureEvent::GestureTap;
  tap.sourceDevice = blink::WebGestureDeviceTouchpad;
  tap.x = point.x();
  tap.y = point.y();
  tap.modifiers = blink::WebInputEvent::ControlKey;
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardGestureEvent(tap);
}

void SimulateTapWithModifiersAt(WebContents* web_contents,
                                unsigned modifiers,
                                const gfx::Point& point) {
  blink::WebGestureEvent tap;
  tap.type = blink::WebGestureEvent::GestureTap;
  tap.sourceDevice = blink::WebGestureDeviceTouchpad;
  tap.x = point.x();
  tap.y = point.y();
  tap.modifiers = modifiers;
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardGestureEvent(tap);
}

#if defined(USE_AURA)
void SimulateTouchPressAt(WebContents* web_contents, const gfx::Point& point) {
  ui::TouchEvent touch(ui::ET_TOUCH_PRESSED, point, 0, base::TimeDelta());
  static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView())
      ->OnTouchEvent(&touch);
}
#endif

void SimulateKeyPress(WebContents* web_contents,
                      ui::KeyboardCode key_code,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) {
  SimulateKeyPressWithCode(
      web_contents, key_code, std::string(), control, shift, alt, command);
}

void SimulateKeyPressWithCode(WebContents* web_contents,
                              ui::KeyboardCode key_code,
                              const std::string& code,
                              bool control,
                              bool shift,
                              bool alt,
                              bool command) {
  int native_key_code = ui::KeycodeConverter::DomCodeToNativeKeycode(
      ui::KeycodeConverter::CodeStringToDomCode(code));

  int modifiers = 0;

  // The order of these key down events shouldn't matter for our simulation.
  // For our simulation we can use either the left keys or the right keys.
  if (control) {
    modifiers |= blink::WebInputEvent::ControlKey;
    InjectRawKeyEvent(
        web_contents, blink::WebInputEvent::RawKeyDown, ui::VKEY_CONTROL,
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::CONTROL_LEFT),
        modifiers);
  }

  if (shift) {
    modifiers |= blink::WebInputEvent::ShiftKey;
    InjectRawKeyEvent(
        web_contents, blink::WebInputEvent::RawKeyDown, ui::VKEY_SHIFT,
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::SHIFT_LEFT),
        modifiers);
  }

  if (alt) {
    modifiers |= blink::WebInputEvent::AltKey;
    InjectRawKeyEvent(
        web_contents, blink::WebInputEvent::RawKeyDown, ui::VKEY_MENU,
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::ALT_LEFT),
        modifiers);
  }

  if (command) {
    modifiers |= blink::WebInputEvent::MetaKey;
    InjectRawKeyEvent(
        web_contents, blink::WebInputEvent::RawKeyDown, ui::VKEY_COMMAND,
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::META_LEFT),
        modifiers);
  }
  InjectRawKeyEvent(web_contents, blink::WebInputEvent::RawKeyDown, key_code,
                    native_key_code, modifiers);

  InjectRawKeyEvent(web_contents, blink::WebInputEvent::Char, key_code,
                    native_key_code, modifiers);

  InjectRawKeyEvent(web_contents, blink::WebInputEvent::KeyUp, key_code,
                    native_key_code, modifiers);

  // The order of these key releases shouldn't matter for our simulation.
  if (control) {
    modifiers &= ~blink::WebInputEvent::ControlKey;
    InjectRawKeyEvent(
        web_contents, blink::WebInputEvent::KeyUp, ui::VKEY_CONTROL,
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::CONTROL_LEFT),
        modifiers);
  }

  if (shift) {
    modifiers &= ~blink::WebInputEvent::ShiftKey;
    InjectRawKeyEvent(
        web_contents, blink::WebInputEvent::KeyUp, ui::VKEY_SHIFT,
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::SHIFT_LEFT),
        modifiers);
  }

  if (alt) {
    modifiers &= ~blink::WebInputEvent::AltKey;
    InjectRawKeyEvent(
        web_contents, blink::WebInputEvent::KeyUp, ui::VKEY_MENU,
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::ALT_LEFT),
        modifiers);
  }

  if (command) {
    modifiers &= ~blink::WebInputEvent::MetaKey;
    InjectRawKeyEvent(
        web_contents, blink::WebInputEvent::KeyUp, ui::VKEY_COMMAND,
        ui::KeycodeConverter::DomCodeToNativeKeycode(ui::DomCode::META_LEFT),
        modifiers);
  }

  ASSERT_EQ(modifiers, 0);
}

ToRenderFrameHost::ToRenderFrameHost(WebContents* web_contents)
    : render_frame_host_(web_contents->GetMainFrame()) {
}

ToRenderFrameHost::ToRenderFrameHost(RenderViewHost* render_view_host)
    : render_frame_host_(render_view_host->GetMainFrame()) {
}

ToRenderFrameHost::ToRenderFrameHost(RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {
}

bool ExecuteScript(const ToRenderFrameHost& adapter,
                   const std::string& script) {
  std::string new_script =
      script + ";window.domAutomationController.send(0);";
  return ExecuteScriptHelper(adapter.render_frame_host(), new_script, NULL);
}

bool ExecuteScriptAndExtractInt(const ToRenderFrameHost& adapter,
                                const std::string& script, int* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  if (!ExecuteScriptHelper(adapter.render_frame_host(), script, &value) ||
      !value.get()) {
    return false;
  }

  return value->GetAsInteger(result);
}

bool ExecuteScriptAndExtractBool(const ToRenderFrameHost& adapter,
                                 const std::string& script, bool* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  if (!ExecuteScriptHelper(adapter.render_frame_host(), script, &value) ||
      !value.get()) {
    return false;
  }

  return value->GetAsBoolean(result);
}

bool ExecuteScriptInIsolatedWorldAndExtractBool(
    const ToRenderFrameHost& adapter,
    const int world_id,
    const std::string& script,
    bool* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  if (!ExecuteScriptInIsolatedWorldHelper(adapter.render_frame_host(), world_id,
                                          script, &value) ||
      !value.get()) {
    return false;
  }

  return value->GetAsBoolean(result);
}

bool ExecuteScriptAndExtractString(const ToRenderFrameHost& adapter,
                                   const std::string& script,
                                   std::string* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
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

RenderFrameHost* ChildFrameAt(RenderFrameHost* frame, size_t index) {
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(frame);
  if (index >= rfh->frame_tree_node()->child_count())
    return nullptr;
  return rfh->frame_tree_node()->child_at(index)->current_frame_host();
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
      BrowserContext::GetDefaultStoragePartition(browser_context)->
          GetURLRequestContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&GetCookiesOnIOThread, url, base::RetainedRef(context_getter),
                 &event, &cookies));
  event.Wait();
  return cookies;
}

bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value) {
  bool result = false;
  base::WaitableEvent event(true, false);
  net::URLRequestContextGetter* context_getter =
      BrowserContext::GetDefaultStoragePartition(browser_context)->
          GetURLRequestContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&SetCookieOnIOThread, url, value,
                 base::RetainedRef(context_getter), &event, &result));
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

void SetupCrossSiteRedirector(net::EmbeddedTestServer* embedded_test_server) {
   embedded_test_server->RegisterRequestHandler(
       base::Bind(&CrossSiteRedirectResponseHandler,
                  embedded_test_server->base_url()));
}

void WaitForInterstitialAttach(content::WebContents* web_contents) {
  if (web_contents->ShowingInterstitialPage())
    return;
  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  InterstitialObserver observer(web_contents,
                                loop_runner->QuitClosure(),
                                base::Closure());
  loop_runner->Run();
}

void WaitForInterstitialDetach(content::WebContents* web_contents) {
  RunTaskAndWaitForInterstitialDetach(web_contents, base::Closure());
}

void RunTaskAndWaitForInterstitialDetach(content::WebContents* web_contents,
                                         const base::Closure& task) {
  if (!web_contents || !web_contents->ShowingInterstitialPage())
    return;
  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  InterstitialObserver observer(web_contents,
                                base::Closure(),
                                loop_runner->QuitClosure());
  if (!task.is_null())
    task.Run();
  // At this point, web_contents may have been deleted.
  loop_runner->Run();
}

bool WaitForRenderFrameReady(RenderFrameHost* rfh) {
  if (!rfh)
    return false;
  std::string result;
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractString(
          rfh,
          "(function() {"
          "  var done = false;"
          "  function checkState() {"
          "    if (!done && document.readyState == 'complete') {"
          "      done = true;"
          "      window.domAutomationController.send('pageLoadComplete');"
          "    }"
          "  }"
          "  checkState();"
          "  document.addEventListener('readystatechange', checkState);"
          "})();",
          &result));
  return result == "pageLoadComplete";
}

void EnableAccessibilityForWebContents(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  web_contents_impl->SetAccessibilityMode(AccessibilityModeComplete);
}

void WaitForAccessibilityFocusChange() {
  scoped_refptr<content::MessageLoopRunner> loop_runner(
      new content::MessageLoopRunner);
  BrowserAccessibilityManager::SetFocusChangeCallbackForTesting(
      loop_runner->QuitClosure());
  loop_runner->Run();
}

ui::AXNodeData GetFocusedAccessibilityNodeInfo(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  BrowserAccessibilityManager* manager =
      web_contents_impl->GetRootBrowserAccessibilityManager();
  if (!manager)
    return ui::AXNodeData();
  BrowserAccessibility* focused_node = manager->GetFocus();
  return focused_node->GetData();
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

void TitleWatcher::DidStopLoading() {
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

RenderProcessHostWatcher::RenderProcessHostWatcher(
    RenderProcessHost* render_process_host, WatchType type)
    : render_process_host_(render_process_host),
      type_(type),
      did_exit_normally_(true),
      message_loop_runner_(new MessageLoopRunner) {
  render_process_host_->AddObserver(this);
}

RenderProcessHostWatcher::RenderProcessHostWatcher(
    WebContents* web_contents, WatchType type)
    : render_process_host_(web_contents->GetRenderProcessHost()),
      type_(type),
      did_exit_normally_(true),
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
    base::TerminationStatus status,
    int exit_code) {
  did_exit_normally_ = status == base::TERMINATION_STATUS_NORMAL_TERMINATION;
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
  Details<std::string> dom_op_result(details);
  message_queue_.push(*dom_op_result.ptr());
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

class WebContentsAddedObserver::RenderViewCreatedObserver
    : public WebContentsObserver {
 public:
  explicit RenderViewCreatedObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents),
        render_view_created_called_(false),
        main_frame_created_called_(false) {}

  // WebContentsObserver:
  void RenderViewCreated(RenderViewHost* rvh) override {
    render_view_created_called_ = true;
  }

  void RenderFrameCreated(RenderFrameHost* rfh) override {
    if (rfh == web_contents()->GetMainFrame())
      main_frame_created_called_ = true;
  }

  bool render_view_created_called_;
  bool main_frame_created_called_;
};

WebContentsAddedObserver::WebContentsAddedObserver()
    : web_contents_created_callback_(
          base::Bind(&WebContentsAddedObserver::WebContentsCreated,
                     base::Unretained(this))),
      web_contents_(NULL) {
  WebContentsImpl::FriendZone::AddCreatedCallbackForTesting(
      web_contents_created_callback_);
}

WebContentsAddedObserver::~WebContentsAddedObserver() {
  WebContentsImpl::FriendZone::RemoveCreatedCallbackForTesting(
      web_contents_created_callback_);
}

void WebContentsAddedObserver::WebContentsCreated(WebContents* web_contents) {
  DCHECK(!web_contents_);
  web_contents_ = web_contents;
  child_observer_.reset(new RenderViewCreatedObserver(web_contents));

  if (runner_.get())
    runner_->QuitClosure().Run();
}

WebContents* WebContentsAddedObserver::GetWebContents() {
  if (web_contents_)
    return web_contents_;

  runner_ = new MessageLoopRunner();
  runner_->Run();
  return web_contents_;
}

bool WebContentsAddedObserver::RenderViewCreatedCalled() {
  if (child_observer_) {
    return child_observer_->render_view_created_called_ &&
           child_observer_->main_frame_created_called_;
  }
  return false;
}

bool RequestFrame(WebContents* web_contents) {
  DCHECK(web_contents);
  return RenderWidgetHostImpl::From(
             web_contents->GetRenderViewHost()->GetWidget())
      ->ScheduleComposite();
}

FrameWatcher::FrameWatcher() : MessageFilter(), frames_to_wait_(0) {}

FrameWatcher::~FrameWatcher() {
}

void FrameWatcher::ReceivedFrameSwap(cc::CompositorFrameMetadata metadata) {
  --frames_to_wait_;
  last_metadata_ = metadata;
  if (frames_to_wait_ == 0)
    quit_.Run();
}

bool FrameWatcher::OnMessageReceived(const IPC::Message& message) {
  if (message.type() == ViewHostMsg_SwapCompositorFrame::ID) {
    ViewHostMsg_SwapCompositorFrame::Param param;
    if (!ViewHostMsg_SwapCompositorFrame::Read(&message, &param))
      return false;
    std::unique_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
    base::get<1>(param).AssignTo(frame.get());

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&FrameWatcher::ReceivedFrameSwap, this, frame->metadata));
  }
  return false;
}

void FrameWatcher::AttachTo(WebContents* web_contents) {
  DCHECK(web_contents);
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->GetProcess()->GetChannel()->AddFilter(this);
}

void FrameWatcher::WaitFrames(int frames_to_wait) {
  if (frames_to_wait <= 0)
    return;
  base::RunLoop run_loop;
  base::AutoReset<base::Closure> reset_quit(&quit_, run_loop.QuitClosure());
  base::AutoReset<int> reset_frames_to_wait(&frames_to_wait_, frames_to_wait);
  run_loop.Run();
}

const cc::CompositorFrameMetadata& FrameWatcher::LastMetadata() {
  return last_metadata_;
}

MainThreadFrameObserver::MainThreadFrameObserver(
    RenderWidgetHost* render_widget_host)
    : render_widget_host_(render_widget_host),
      routing_id_(render_widget_host_->GetProcess()->GetNextRoutingID()) {
  // TODO(lfg): We should look into adding a way to observe RenderWidgetHost
  // messages similarly to what WebContentsObserver can do with RFH and RVW.
  render_widget_host_->GetProcess()->AddRoute(routing_id_, this);
}

MainThreadFrameObserver::~MainThreadFrameObserver() {
  render_widget_host_->GetProcess()->RemoveRoute(routing_id_);
}

void MainThreadFrameObserver::Wait() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  render_widget_host_->Send(new ViewMsg_WaitForNextFrameForTests(
      render_widget_host_->GetRoutingID(), routing_id_));
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();
  run_loop_.reset(nullptr);
}

void MainThreadFrameObserver::Quit() {
  if (run_loop_)
    run_loop_->Quit();
}

bool MainThreadFrameObserver::OnMessageReceived(const IPC::Message& msg) {
  if (msg.type() == ViewHostMsg_WaitForNextFrameForTests_ACK::ID &&
      msg.routing_id() == routing_id_) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&MainThreadFrameObserver::Quit, base::Unretained(this)));
  }
  return true;
}

InputMsgWatcher::InputMsgWatcher(RenderWidgetHost* render_widget_host,
                                 blink::WebInputEvent::Type type)
    : BrowserMessageFilter(InputMsgStart),
      wait_for_type_(type),
      ack_result_(INPUT_EVENT_ACK_STATE_UNKNOWN) {
  render_widget_host->GetProcess()->AddFilter(this);
}

InputMsgWatcher::~InputMsgWatcher() {}

void InputMsgWatcher::ReceivedAck(blink::WebInputEvent::Type ack_type,
                                  uint32_t ack_state) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (wait_for_type_ == ack_type) {
    ack_result_ = ack_state;
    if (!quit_.is_null())
      quit_.Run();
  }
}

bool InputMsgWatcher::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (message.type() == InputHostMsg_HandleInputEvent_ACK::ID) {
    InputHostMsg_HandleInputEvent_ACK::Param params;
    InputHostMsg_HandleInputEvent_ACK::Read(&message, &params);
    blink::WebInputEvent::Type ack_type = base::get<0>(params).type;
    InputEventAckState ack_state = base::get<0>(params).state;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&InputMsgWatcher::ReceivedAck, this, ack_type, ack_state));
  }
  return false;
}

uint32_t InputMsgWatcher::WaitForAck() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ack_result_ != INPUT_EVENT_ACK_STATE_UNKNOWN)
    return ack_result_;
  base::RunLoop run_loop;
  base::AutoReset<base::Closure> reset_quit(&quit_, run_loop.QuitClosure());
  run_loop.Run();
  return ack_result_;
}

}  // namespace content
