// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/browser_test_utils.h"

#include <stddef.h>

#include <set>
#include <tuple>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/process/kill.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/pattern.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"
#include "components/viz/service/surfaces/surface.h"
#include "components/viz/service/surfaces/surface_manager.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/browser_plugin/browser_plugin_guest.h"
#include "content/browser/compositor/surface_utils.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_child_frame.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/common/fileapi/file_system_messages.h"
#include "content/common/fileapi/webblob_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_plugin_guest_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/histogram_fetcher.h"
#include "content/public/browser/interstitial_page.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_fileapi_operation_waiter.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/test/accessibility_browser_test_utils.h"
#include "ipc/ipc_security_test_util.h"
#include "net/base/filename_util.h"
#include "net/cookies/cookie_store.h"
#include "net/filter/gzip_header.h"
#include "net/filter/gzip_source_stream.h"
#include "net/filter/mock_source_stream.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/test/python_utils.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/test/test_clipboard.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/latency/latency_info.h"
#include "ui/resources/grit/webui_resources.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "ui/aura/test/window_event_dispatcher_test_api.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#endif  // USE_AURA

namespace content {
namespace {

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
                         bool user_gesture,
                         std::unique_ptr<base::Value>* result)
    WARN_UNUSED_RESULT;

// Executes the passed |script| in the frame specified by |render_frame_host|.
// If |result| is not NULL, stores the value that the evaluation of the script
// in |result|.  Returns true on success.
bool ExecuteScriptHelper(RenderFrameHost* render_frame_host,
                         const std::string& script,
                         bool user_gesture,
                         std::unique_ptr<base::Value>* result) {
  // TODO(lukasza): Only get messages from the specific |render_frame_host|.
  DOMMessageQueue dom_message_queue(
      WebContents::FromRenderFrameHost(render_frame_host));
  if (user_gesture) {
    render_frame_host->ExecuteJavaScriptWithUserGestureForTests(
        base::UTF8ToUTF16(script));
  } else {
    render_frame_host->ExecuteJavaScriptForTests(base::UTF8ToUTF16(script));
  }
  std::string json;
  if (!dom_message_queue.WaitForMessage(&json)) {
    DLOG(ERROR) << "Cannot communicate with DOMMessageQueue.";
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

bool ExecuteScriptWithUserGestureControl(RenderFrameHost* frame,
                                         const std::string& script,
                                         bool user_gesture) {
  // TODO(lukasza): ExecuteScript should just call
  // ExecuteJavaScriptWithUserGestureForTests and avoid modifying the original
  // script (and at that point we should merge it with and remove
  // ExecuteScriptAsync).  This is difficult to change, because many tests
  // depend on the message loop pumping done by ExecuteScriptHelper below (this
  // is fragile - these tests should wait on a more specific thing instead).

  std::string expected_response = "ExecuteScript-" + base::GenerateGUID();
  std::string new_script = base::StringPrintf(
      R"( %s;  // Original script.
          window.domAutomationController.send('%s'); )",
      script.c_str(), expected_response.c_str());

  std::unique_ptr<base::Value> value;
  if (!ExecuteScriptHelper(frame, new_script, user_gesture, &value) ||
      !value.get()) {
    return false;
  }

  DCHECK_EQ(base::Value::Type::STRING, value->GetType());
  std::string actual_response;
  if (value->GetAsString(&actual_response))
    DCHECK_EQ(expected_response, actual_response);

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
                                        const std::string& script,
                                        std::unique_ptr<base::Value>* result) {
  // TODO(lukasza): Only get messages from the specific |render_frame_host|.
  DOMMessageQueue dom_message_queue(
      WebContents::FromRenderFrameHost(render_frame_host));
  render_frame_host->ExecuteJavaScriptInIsolatedWorld(
      base::UTF8ToUTF16(script),
      content::RenderFrameHost::JavaScriptResultCallback(), world_id);
  std::string json;
  if (!dom_message_queue.WaitForMessage(&json)) {
    DLOG(ERROR) << "Cannot communicate with DOMMessageQueue.";
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
                            ui::DomKey key,
                            ui::DomCode code,
                            ui::KeyboardCode key_code,
                            NativeWebKeyboardEvent* event) {
  event->dom_key = key;
  event->dom_code = static_cast<int>(code);
  event->native_key_code = ui::KeycodeConverter::DomCodeToNativeKeycode(code);
  event->windows_key_code = key_code;
  event->is_system_key = false;
  event->skip_in_browser = true;

  if (type == blink::WebInputEvent::kChar ||
      type == blink::WebInputEvent::kRawKeyDown) {
    event->text[0] = key_code;
    event->unmodified_text[0] = key_code;
  }
}

void InjectRawKeyEvent(WebContents* web_contents,
                       blink::WebInputEvent::Type type,
                       ui::DomKey key,
                       ui::DomCode code,
                       ui::KeyboardCode key_code,
                       int modifiers) {
  NativeWebKeyboardEvent event(type, modifiers, base::TimeTicks::Now());
  BuildSimpleWebKeyEvent(type, key, code, key_code, &event);
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  RenderWidgetHostImpl* main_frame_rwh =
      web_contents_impl->GetMainFrame()->GetRenderWidgetHost();
  web_contents_impl->GetFocusedRenderWidgetHost(main_frame_rwh)
      ->ForwardKeyboardEvent(event);
}

int SimulateModifierKeysDown(WebContents* web_contents,
                             bool control,
                             bool shift,
                             bool alt,
                             bool command) {
  int modifiers = 0;

  // The order of these key down events shouldn't matter for our simulation.
  // For our simulation we can use either the left keys or the right keys.
  if (control) {
    modifiers |= blink::WebInputEvent::kControlKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kRawKeyDown,
                      ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT,
                      ui::VKEY_CONTROL, modifiers);
  }
  if (shift) {
    modifiers |= blink::WebInputEvent::kShiftKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kRawKeyDown,
                      ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT,
                      ui::VKEY_SHIFT, modifiers);
  }
  if (alt) {
    modifiers |= blink::WebInputEvent::kAltKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kRawKeyDown,
                      ui::DomKey::ALT, ui::DomCode::ALT_LEFT, ui::VKEY_MENU,
                      modifiers);
  }
  if (command) {
    modifiers |= blink::WebInputEvent::kMetaKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kRawKeyDown,
                      ui::DomKey::META, ui::DomCode::META_LEFT,
                      ui::VKEY_COMMAND, modifiers);
  }
  return modifiers;
}

int SimulateModifierKeysUp(WebContents* web_contents,
                           bool control,
                           bool shift,
                           bool alt,
                           bool command,
                           int modifiers) {
  // The order of these key releases shouldn't matter for our simulation.
  if (control) {
    modifiers &= ~blink::WebInputEvent::kControlKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kKeyUp,
                      ui::DomKey::CONTROL, ui::DomCode::CONTROL_LEFT,
                      ui::VKEY_CONTROL, modifiers);
  }

  if (shift) {
    modifiers &= ~blink::WebInputEvent::kShiftKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kKeyUp,
                      ui::DomKey::SHIFT, ui::DomCode::SHIFT_LEFT,
                      ui::VKEY_SHIFT, modifiers);
  }

  if (alt) {
    modifiers &= ~blink::WebInputEvent::kAltKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kKeyUp,
                      ui::DomKey::ALT, ui::DomCode::ALT_LEFT, ui::VKEY_MENU,
                      modifiers);
  }

  if (command) {
    modifiers &= ~blink::WebInputEvent::kMetaKey;
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kKeyUp,
                      ui::DomKey::META, ui::DomCode::META_LEFT,
                      ui::VKEY_COMMAND, modifiers);
  }
  return modifiers;
}

void SimulateKeyEvent(WebContents* web_contents,
                      ui::DomKey key,
                      ui::DomCode code,
                      ui::KeyboardCode key_code,
                      bool send_char,
                      int modifiers) {
  InjectRawKeyEvent(web_contents, blink::WebInputEvent::kRawKeyDown, key, code,
                    key_code, modifiers);
  if (send_char) {
    InjectRawKeyEvent(web_contents, blink::WebInputEvent::kChar, key, code,
                      key_code, modifiers);
  }
  InjectRawKeyEvent(web_contents, blink::WebInputEvent::kKeyUp, key, code,
                    key_code, modifiers);
}

void SimulateKeyPressImpl(WebContents* web_contents,
                          ui::DomKey key,
                          ui::DomCode code,
                          ui::KeyboardCode key_code,
                          bool control,
                          bool shift,
                          bool alt,
                          bool command,
                          bool send_char) {
  int modifiers =
      SimulateModifierKeysDown(web_contents, control, shift, alt, command);
  SimulateKeyEvent(web_contents, key, code, key_code, send_char, modifiers);
  modifiers = SimulateModifierKeysUp(web_contents, control, shift, alt, command,
                                     modifiers);
  ASSERT_EQ(modifiers, 0);
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
      base::BindOnce(&GetCookiesCallback, cookies, event));
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
      base::BindOnce(&SetCookieCallback, result, event));
}

std::unique_ptr<net::test_server::HttpResponse>
CrossSiteRedirectResponseHandler(const net::EmbeddedTestServer* test_server,
                                 const net::test_server::HttpRequest& request) {
  net::HttpStatusCode http_status_code;

  // Inspect the prefix and extract the remainder of the url into |params|.
  size_t length_of_chosen_prefix;
  std::string prefix_302("/cross-site/");
  std::string prefix_307("/cross-site-307/");
  if (base::StartsWith(request.relative_url, prefix_302,
                       base::CompareCase::SENSITIVE)) {
    http_status_code = net::HTTP_MOVED_PERMANENTLY;
    length_of_chosen_prefix = prefix_302.length();
  } else if (base::StartsWith(request.relative_url, prefix_307,
                              base::CompareCase::SENSITIVE)) {
    http_status_code = net::HTTP_TEMPORARY_REDIRECT;
    length_of_chosen_prefix = prefix_307.length();
  } else {
    // Unrecognized prefix - let somebody else handle this request.
    return std::unique_ptr<net::test_server::HttpResponse>();
  }
  std::string params = request.relative_url.substr(length_of_chosen_prefix);

  // A hostname to redirect to must be included in the URL, therefore at least
  // one '/' character is expected.
  size_t slash = params.find('/');
  if (slash == std::string::npos)
    return std::unique_ptr<net::test_server::HttpResponse>();

  // Replace the host of the URL with the one passed in the URL.
  GURL::Replacements replace_host;
  replace_host.SetHostStr(base::StringPiece(params).substr(0, slash));
  GURL redirect_server =
      test_server->base_url().ReplaceComponents(replace_host);

  // Append the real part of the path to the new URL.
  std::string path = params.substr(slash + 1);
  GURL redirect_target(redirect_server.Resolve(path));
  DCHECK(redirect_target.is_valid());

  std::unique_ptr<net::test_server::BasicHttpResponse> http_response(
      new net::test_server::BasicHttpResponse);
  http_response->set_code(http_status_code);
  http_response->AddCustomHeader("Location", redirect_target.spec());
  return std::move(http_response);
}

// Helper class used by the TestNavigationManager to pause navigations.
// Note: the throttle should be added to the *end* of the list of throttles,
// so all NavigationThrottles that should be attached observe the
// WillStartRequest callback. RegisterThrottleForTesting has this behavior.
class TestNavigationManagerThrottle : public NavigationThrottle {
 public:
  TestNavigationManagerThrottle(NavigationHandle* handle,
                                base::Closure on_will_start_request_closure,
                                base::Closure on_will_process_response_closure)
      : NavigationThrottle(handle),
        on_will_start_request_closure_(on_will_start_request_closure),
        on_will_process_response_closure_(on_will_process_response_closure) {}
  ~TestNavigationManagerThrottle() override {}

  const char* GetNameForLogging() override {
    return "TestNavigationManagerThrottle";
  }

 private:
  // NavigationThrottle:
  NavigationThrottle::ThrottleCheckResult WillStartRequest() override {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            on_will_start_request_closure_);
    return NavigationThrottle::DEFER;
  }

  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            on_will_process_response_closure_);
    return NavigationThrottle::DEFER;
  }

  base::Closure on_will_start_request_closure_;
  base::Closure on_will_process_response_closure_;
};

bool HasGzipHeader(const base::RefCountedMemory& maybe_gzipped) {
  net::GZipHeader header;
  net::GZipHeader::Status header_status = net::GZipHeader::INCOMPLETE_HEADER;
  const char* header_end = nullptr;
  while (header_status == net::GZipHeader::INCOMPLETE_HEADER) {
    header_status = header.ReadMore(maybe_gzipped.front_as<char>(),
                                    maybe_gzipped.size(),
                                    &header_end);
  }
  return header_status == net::GZipHeader::COMPLETE_HEADER;
}

void AppendGzippedResource(const base::RefCountedMemory& encoded,
                           std::string* to_append) {
  std::unique_ptr<net::MockSourceStream> source_stream(
      new net::MockSourceStream());
  source_stream->AddReadResult(encoded.front_as<char>(), encoded.size(),
                               net::OK, net::MockSourceStream::SYNC);
  // Add an EOF.
  source_stream->AddReadResult(encoded.front_as<char>() + encoded.size(), 0,
                               net::OK, net::MockSourceStream::SYNC);
  std::unique_ptr<net::GzipSourceStream> filter = net::GzipSourceStream::Create(
      std::move(source_stream), net::SourceStream::TYPE_GZIP);
  scoped_refptr<net::IOBufferWithSize> dest_buffer =
      new net::IOBufferWithSize(4096);
  net::CompletionCallback callback;
  while (true) {
    int rv = filter->Read(dest_buffer.get(), dest_buffer->size(), callback);
    ASSERT_LE(0, rv);
    if (rv <= 0)
      break;
    to_append->append(dest_buffer->data(), rv);
  }
}

// Queries for video input devices on the current system using the getSources
// API.
//
// This does not guarantee that a getUserMedia with video will succeed, as the
// camera could be busy for instance.
//
// Returns has-video-input-device to the test if there is a webcam available,
// no-video-input-devices otherwise.
const char kHasVideoInputDeviceOnSystem[] = R"(
    (function() {
      navigator.mediaDevices.enumerateDevices()
      .then(function(devices) {
        if (devices.some((device) => device.kind == 'videoinput')) {
          window.domAutomationController.send('has-video-input-device');
        } else {
          window.domAutomationController.send('no-video-input-devices');
        }
      });
    })()
)";

const char kHasVideoInputDevice[] = "has-video-input-device";

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

void PrepContentsForBeforeUnloadTest(WebContents* web_contents) {
  for (auto* frame : web_contents->GetAllFrames()) {
    // JavaScript onbeforeunload dialogs are ignored unless the frame received a
    // user gesture. Make sure the frames have user gestures.
    frame->ExecuteJavaScriptWithUserGestureForTests(base::string16());

    // Disable the hang monitor, otherwise there will be a race between the
    // beforeunload dialog and the beforeunload hang timer.
    frame->DisableBeforeUnloadHangMonitorForTesting();
  }
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

void SimulateUnresponsiveRenderer(WebContents* web_contents,
                                  RenderWidgetHost* widget) {
  static_cast<WebContentsImpl*>(web_contents)
      ->RendererUnresponsive(RenderWidgetHostImpl::From(widget));
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
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, modifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  mouse_event.button = button;
  mouse_event.SetPositionInWidget(point.x(), point.y());
  // Mac needs positionInScreen for events to plugins.
  gfx::Rect offset = web_contents->GetContainerBounds();
  mouse_event.SetPositionInScreen(point.x() + offset.x(),
                                  point.y() + offset.y());
  mouse_event.click_count = 1;
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
}

void SimulateMouseEvent(WebContents* web_contents,
                        blink::WebInputEvent::Type type,
                        const gfx::Point& point) {
  blink::WebMouseEvent mouse_event(
      type, blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  mouse_event.SetPositionInWidget(point.x(), point.y());
  web_contents->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
}

void SimulateMouseWheelEvent(WebContents* web_contents,
                             const gfx::Point& point,
                             const gfx::Vector2d& delta) {
  blink::WebMouseWheelEvent wheel_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));

  wheel_event.SetPositionInWidget(point.x(), point.y());
  wheel_event.delta_x = delta.x();
  wheel_event.delta_y = delta.y();
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardWheelEvent(wheel_event);
}

void SimulateGestureScrollSequence(WebContents* web_contents,
                                   const gfx::Point& point,
                                   const gfx::Vector2dF& delta) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());

  blink::WebGestureEvent scroll_begin(
      blink::WebGestureEvent::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  scroll_begin.source_device = blink::kWebGestureDeviceTouchpad;
  scroll_begin.x = point.x();
  scroll_begin.y = point.y();
  scroll_begin.data.scroll_begin.delta_x_hint = delta.x();
  scroll_begin.data.scroll_begin.delta_y_hint = delta.y();
  widget_host->ForwardGestureEvent(scroll_begin);

  blink::WebGestureEvent scroll_update(
      blink::WebGestureEvent::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  scroll_update.source_device = blink::kWebGestureDeviceTouchpad;
  scroll_update.x = point.x();
  scroll_update.y = point.y();
  scroll_update.data.scroll_update.delta_x = delta.x();
  scroll_update.data.scroll_update.delta_y = delta.y();
  scroll_update.data.scroll_update.velocity_x = 0;
  scroll_update.data.scroll_update.velocity_y = 0;
  widget_host->ForwardGestureEvent(scroll_update);

  blink::WebGestureEvent scroll_end(
      blink::WebGestureEvent::kGestureScrollEnd,
      blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  scroll_end.source_device = blink::kWebGestureDeviceTouchpad;
  scroll_end.x = point.x() + delta.x();
  scroll_end.y = point.y() + delta.y();
  widget_host->ForwardGestureEvent(scroll_end);
}

void SimulateGestureFlingSequence(WebContents* web_contents,
                                  const gfx::Point& point,
                                  const gfx::Vector2dF& velocity) {
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());

  blink::WebGestureEvent scroll_begin(
      blink::WebGestureEvent::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  scroll_begin.source_device = blink::kWebGestureDeviceTouchpad;
  scroll_begin.x = point.x();
  scroll_begin.y = point.y();
  widget_host->ForwardGestureEvent(scroll_begin);

  blink::WebGestureEvent scroll_end(
      blink::WebGestureEvent::kGestureScrollEnd,
      blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  scroll_end.source_device = blink::kWebGestureDeviceTouchpad;
  scroll_end.x = point.x();
  scroll_end.y = point.y();
  widget_host->ForwardGestureEvent(scroll_end);

  blink::WebGestureEvent fling_start(
      blink::WebGestureEvent::kGestureFlingStart,
      blink::WebInputEvent::kNoModifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  fling_start.source_device = blink::kWebGestureDeviceTouchpad;
  fling_start.x = point.x();
  fling_start.y = point.y();
  fling_start.data.fling_start.target_viewport = false;
  fling_start.data.fling_start.velocity_x = velocity.x();
  fling_start.data.fling_start.velocity_y = velocity.y();
  widget_host->ForwardGestureEvent(fling_start);
}

void SimulateTapAt(WebContents* web_contents, const gfx::Point& point) {
  blink::WebGestureEvent tap(
      blink::WebGestureEvent::kGestureTap, 0,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  tap.source_device = blink::kWebGestureDeviceTouchpad;
  tap.x = point.x();
  tap.y = point.y();
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardGestureEvent(tap);
}

void SimulateTapWithModifiersAt(WebContents* web_contents,
                                unsigned modifiers,
                                const gfx::Point& point) {
  blink::WebGestureEvent tap(
      blink::WebGestureEvent::kGestureTap, modifiers,
      ui::EventTimeStampToSeconds(ui::EventTimeForNow()));
  tap.source_device = blink::kWebGestureDeviceTouchpad;
  tap.x = point.x();
  tap.y = point.y();
  RenderWidgetHostImpl* widget_host = RenderWidgetHostImpl::From(
      web_contents->GetRenderViewHost()->GetWidget());
  widget_host->ForwardGestureEvent(tap);
}

#if defined(USE_AURA)
void SimulateTouchPressAt(WebContents* web_contents, const gfx::Point& point) {
  ui::TouchEvent touch(
      ui::ET_TOUCH_PRESSED, point, base::TimeTicks(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView())
      ->OnTouchEvent(&touch);
}
#endif

void SimulateKeyPress(WebContents* web_contents,
                      ui::DomKey key,
                      ui::DomCode code,
                      ui::KeyboardCode key_code,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command) {
  SimulateKeyPressImpl(web_contents, key, code, key_code, control, shift, alt,
                       command, /*send_char=*/true);
}

void SimulateKeyPressWithoutChar(WebContents* web_contents,
                                 ui::DomKey key,
                                 ui::DomCode code,
                                 ui::KeyboardCode key_code,
                                 bool control,
                                 bool shift,
                                 bool alt,
                                 bool command) {
  SimulateKeyPressImpl(web_contents, key, code, key_code, control, shift, alt,
                       command, /*send_char=*/false);
}

ScopedSimulateModifierKeyPress::ScopedSimulateModifierKeyPress(
    WebContents* web_contents,
    bool control,
    bool shift,
    bool alt,
    bool command)
    : web_contents_(web_contents),
      modifiers_(0),
      control_(control),
      shift_(shift),
      alt_(alt),
      command_(command) {
  modifiers_ =
      SimulateModifierKeysDown(web_contents_, control_, shift_, alt_, command_);
}

ScopedSimulateModifierKeyPress::~ScopedSimulateModifierKeyPress() {
  modifiers_ = SimulateModifierKeysUp(web_contents_, control_, shift_, alt_,
                                      command_, modifiers_);
  DCHECK_EQ(0, modifiers_);
}

void ScopedSimulateModifierKeyPress::MouseClickAt(
    int additional_modifiers,
    blink::WebMouseEvent::Button button,
    const gfx::Point& point) {
  SimulateMouseClickAt(web_contents_, modifiers_ | additional_modifiers, button,
                       point);
}

void ScopedSimulateModifierKeyPress::KeyPress(ui::DomKey key,
                                              ui::DomCode code,
                                              ui::KeyboardCode key_code) {
  SimulateKeyEvent(web_contents_, key, code, key_code, /*send_char=*/true,
                   modifiers_);
}

void ScopedSimulateModifierKeyPress::KeyPressWithoutChar(
    ui::DomKey key,
    ui::DomCode code,
    ui::KeyboardCode key_code) {
  SimulateKeyEvent(web_contents_, key, code, key_code, /*send_char=*/false,
                   modifiers_);
}

bool IsWebcamAvailableOnSystem(WebContents* web_contents) {
  std::string result;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents, kHasVideoInputDeviceOnSystem, &result));
  return result == kHasVideoInputDevice;
}

RenderFrameHost* ConvertToRenderFrameHost(WebContents* web_contents) {
  return web_contents->GetMainFrame();
}

RenderFrameHost* ConvertToRenderFrameHost(RenderViewHost* render_view_host) {
  return render_view_host->GetMainFrame();
}

RenderFrameHost* ConvertToRenderFrameHost(RenderFrameHost* render_frame_host) {
  return render_frame_host;
}

bool ExecuteScript(const ToRenderFrameHost& adapter,
                   const std::string& script) {
  return ExecuteScriptWithUserGestureControl(adapter.render_frame_host(),
                                             script, true);
}

bool ExecuteScriptWithoutUserGesture(const ToRenderFrameHost& adapter,
                                     const std::string& script) {
  return ExecuteScriptWithUserGestureControl(adapter.render_frame_host(),
                                             script, false);
}

void ExecuteScriptAsync(const ToRenderFrameHost& adapter,
                        const std::string& script) {
  adapter.render_frame_host()->ExecuteJavaScriptWithUserGestureForTests(
      base::UTF8ToUTF16(script));
}

bool ExecuteScriptAndExtractDouble(const ToRenderFrameHost& adapter,
                                   const std::string& script, double* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  if (!ExecuteScriptHelper(adapter.render_frame_host(), script, true, &value) ||
      !value.get()) {
    return false;
  }

  return value->GetAsDouble(result);
}

bool ExecuteScriptAndExtractInt(const ToRenderFrameHost& adapter,
                                const std::string& script, int* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  if (!ExecuteScriptHelper(adapter.render_frame_host(), script, true, &value) ||
      !value.get()) {
    return false;
  }

  return value->GetAsInteger(result);
}

bool ExecuteScriptAndExtractBool(const ToRenderFrameHost& adapter,
                                 const std::string& script, bool* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  if (!ExecuteScriptHelper(adapter.render_frame_host(), script, true, &value) ||
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
  if (!ExecuteScriptHelper(adapter.render_frame_host(), script, true, &value) ||
      !value.get()) {
    return false;
  }

  return value->GetAsString(result);
}

bool ExecuteScriptWithoutUserGestureAndExtractDouble(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    double* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, false,
                             &value) &&
         value && value->GetAsDouble(result);
}

bool ExecuteScriptWithoutUserGestureAndExtractInt(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    int* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, false,
                             &value) &&
         value && value->GetAsInteger(result);
}

bool ExecuteScriptWithoutUserGestureAndExtractBool(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    bool* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, false,
                             &value) &&
         value && value->GetAsBoolean(result);
}

bool ExecuteScriptWithoutUserGestureAndExtractString(
    const ToRenderFrameHost& adapter,
    const std::string& script,
    std::string* result) {
  DCHECK(result);
  std::unique_ptr<base::Value> value;
  return ExecuteScriptHelper(adapter.render_frame_host(), script, false,
                             &value) &&
         value && value->GetAsString(result);
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
  EXPECT_EQ(1U, frame_set.size());
  return frame_set.size() == 1 ? *frame_set.begin() : nullptr;
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
    scoped_refptr<base::RefCountedMemory> bytes =
        ResourceBundle::GetSharedInstance().LoadDataResourceBytes(*iter);

    if (HasGzipHeader(*bytes))
      AppendGzippedResource(*bytes, &script);
    else
      script.append(bytes->front_as<char>(), bytes->size());

    script.append("\n");
  }
  ExecuteScriptAsync(web_contents, script);

  DOMMessageQueue message_queue;
  ExecuteScriptAsync(web_contents, "runTests()");

  std::string message;
  do {
    if (!message_queue.WaitForMessage(&message))
      return false;
  } while (message.compare("\"PENDING\"") == 0);

  return message.compare("\"SUCCESS\"") == 0;
}

std::string GetCookies(BrowserContext* browser_context, const GURL& url) {
  std::string cookies;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  net::URLRequestContextGetter* context_getter =
      BrowserContext::GetDefaultStoragePartition(browser_context)->
          GetURLRequestContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&GetCookiesOnIOThread, url,
                     base::RetainedRef(context_getter), &event, &cookies));
  event.Wait();
  return cookies;
}

bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value) {
  bool result = false;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  net::URLRequestContextGetter* context_getter =
      BrowserContext::GetDefaultStoragePartition(browser_context)->
          GetURLRequestContext();

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SetCookieOnIOThread, url, value,
                     base::RetainedRef(context_getter), &event, &result));
  event.Wait();
  return result;
}

void FetchHistogramsFromChildProcesses() {
  scoped_refptr<content::MessageLoopRunner> runner = new MessageLoopRunner;

  FetchHistogramsAsynchronously(
      base::ThreadTaskRunnerHandle::Get(), runner->QuitClosure(),
      // If this call times out, it means that a child process is not
      // responding, which is something we should not ignore.  The timeout is
      // set to be longer than the normal browser test timeout so that it will
      // be prempted by the normal timeout.
      TestTimeouts::action_max_timeout());
  runner->Run();
}

void SetupCrossSiteRedirector(net::EmbeddedTestServer* embedded_test_server) {
  embedded_test_server->RegisterRequestHandler(
      base::Bind(&CrossSiteRedirectResponseHandler, embedded_test_server));
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
  web_contents_impl->SetAccessibilityMode(ui::kAXModeComplete);
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

bool AccessibilityTreeContainsNodeWithName(BrowserAccessibility* node,
                                           const std::string& name) {
  if (node->GetStringAttribute(ui::AX_ATTR_NAME) == name)
    return true;
  for (unsigned i = 0; i < node->PlatformChildCount(); i++) {
    if (AccessibilityTreeContainsNodeWithName(node->PlatformGetChild(i), name))
      return true;
  }
  return false;
}

bool ListenToGuestWebContents(
    AccessibilityNotificationWaiter* accessibility_waiter,
    WebContents* web_contents) {
  accessibility_waiter->ListenToAdditionalFrame(
      static_cast<RenderFrameHostImpl*>(web_contents->GetMainFrame()));
  return true;
}

void WaitForAccessibilityTreeToContainNodeWithName(WebContents* web_contents,
                                                   const std::string& name) {
  WebContentsImpl* web_contents_impl = static_cast<WebContentsImpl*>(
      web_contents);
  RenderFrameHostImpl* main_frame = static_cast<RenderFrameHostImpl*>(
      web_contents_impl->GetMainFrame());
  BrowserAccessibilityManager* main_frame_manager =
      main_frame->browser_accessibility_manager();
  FrameTree* frame_tree = web_contents_impl->GetFrameTree();
  while (!main_frame_manager || !AccessibilityTreeContainsNodeWithName(
             main_frame_manager->GetRoot(), name)) {
    AccessibilityNotificationWaiter accessibility_waiter(main_frame,
                                                         ui::AX_EVENT_NONE);
    for (FrameTreeNode* node : frame_tree->Nodes()) {
      accessibility_waiter.ListenToAdditionalFrame(
          node->current_frame_host());
    }

    content::BrowserPluginGuestManager* guest_manager =
        web_contents_impl->GetBrowserContext()->GetGuestManager();
    if (guest_manager) {
      guest_manager->ForEachGuest(web_contents_impl,
                                  base::Bind(&ListenToGuestWebContents,
                                             &accessibility_waiter));
    }

    accessibility_waiter.WaitForNotification();
    main_frame_manager = main_frame->browser_accessibility_manager();
  }
}

ui::AXTreeUpdate GetAccessibilityTreeSnapshot(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  BrowserAccessibilityManager* manager =
      web_contents_impl->GetRootBrowserAccessibilityManager();
  if (!manager)
    return ui::AXTreeUpdate();
  return manager->SnapshotAXTreeForTesting();
}

bool IsWebContentsBrowserPluginFocused(content::WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  BrowserPluginGuest* browser_plugin_guest =
      web_contents_impl->GetBrowserPluginGuest();
  return browser_plugin_guest ? browser_plugin_guest->focused() : false;
}

RenderWidgetHost* GetMouseLockWidget(WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)->GetMouseLockWidget();
}

bool IsInnerInterstitialPageConnected(InterstitialPage* interstitial_page) {
  InterstitialPageImpl* impl =
      static_cast<InterstitialPageImpl*>(interstitial_page);

  RenderWidgetHostViewBase* rwhvb =
      static_cast<RenderWidgetHostViewBase*>(impl->GetView());
  EXPECT_TRUE(rwhvb->IsRenderWidgetHostViewChildFrame());
  RenderWidgetHostViewChildFrame* rwhvcf =
      static_cast<RenderWidgetHostViewChildFrame*>(rwhvb);

  CrossProcessFrameConnector* frame_connector =
      static_cast<CrossProcessFrameConnector*>(
          rwhvcf->FrameConnectorForTesting());

  WebContentsImpl* inner_web_contents =
      static_cast<WebContentsImpl*>(impl->GetWebContents());
  FrameTreeNode* outer_node = FrameTreeNode::GloballyFindByID(
      inner_web_contents->GetOuterDelegateFrameTreeNodeId());

  return outer_node->current_frame_host()->GetView() ==
         frame_connector->GetParentRenderWidgetHostView();
}

std::vector<RenderWidgetHostView*> GetInputEventRouterRenderWidgetHostViews(
    WebContents* web_contents) {
  return static_cast<WebContentsImpl*>(web_contents)
      ->GetInputEventRouter()
      ->GetRenderWidgetHostViewsForTests();
}

RenderWidgetHost* GetFocusedRenderWidgetHost(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  return web_contents_impl->GetFocusedRenderWidgetHost(
      web_contents_impl->GetMainFrame()->GetRenderWidgetHost());
}

WebContents* GetFocusedWebContents(WebContents* web_contents) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  return web_contents_impl->GetFocusedWebContents();
}

void RouteMouseEvent(WebContents* web_contents, blink::WebMouseEvent* event) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents);
  web_contents_impl->GetInputEventRouter()->RouteMouseEvent(
      static_cast<RenderWidgetHostViewBase*>(
          web_contents_impl->GetMainFrame()->GetView()),
      event, ui::LatencyInfo());
}

#if defined(USE_AURA)
void SendRoutedTouchTapSequence(content::WebContents* web_contents,
                                gfx::Point point) {
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  ui::TouchEvent touch_start(
      ui::ET_TOUCH_PRESSED, point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_start);
  ui::TouchEvent touch_end(
      ui::ET_TOUCH_RELEASED, point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH, 0));
  rwhva->OnTouchEvent(&touch_end);
}

void SendRoutedGestureTapSequence(content::WebContents* web_contents,
                                  gfx::Point point) {
  RenderWidgetHostViewAura* rwhva = static_cast<RenderWidgetHostViewAura*>(
      web_contents->GetRenderWidgetHostView());
  ui::GestureEventDetails gesture_tap_down_details(ui::ET_GESTURE_TAP_DOWN);
  gesture_tap_down_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_tap_down(point.x(), point.y(), 0,
                                    base::TimeTicks::Now(),
                                    gesture_tap_down_details);
  rwhva->OnGestureEvent(&gesture_tap_down);
  ui::GestureEventDetails gesture_tap_details(ui::ET_GESTURE_TAP);
  gesture_tap_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  gesture_tap_details.set_tap_count(1);
  ui::GestureEvent gesture_tap(point.x(), point.y(), 0, base::TimeTicks::Now(),
                               gesture_tap_details);
  rwhva->OnGestureEvent(&gesture_tap);
}

#endif

namespace {

class SurfaceHitTestReadyNotifier {
 public:
  explicit SurfaceHitTestReadyNotifier(RenderWidgetHostViewBase* target_view);
  ~SurfaceHitTestReadyNotifier() {}

  void WaitForSurfaceReady(RenderWidgetHostViewBase* root_container);

 private:
  bool ContainsSurfaceId(const viz::SurfaceId& container_surface_id);

  viz::SurfaceManager* surface_manager_;
  RenderWidgetHostViewBase* target_view_;

  DISALLOW_COPY_AND_ASSIGN(SurfaceHitTestReadyNotifier);
};

SurfaceHitTestReadyNotifier::SurfaceHitTestReadyNotifier(
    RenderWidgetHostViewBase* target_view)
    : target_view_(target_view) {
  surface_manager_ = GetFrameSinkManager()->surface_manager();
}

void SurfaceHitTestReadyNotifier::WaitForSurfaceReady(
    RenderWidgetHostViewBase* root_view) {
  viz::SurfaceId root_surface_id = root_view->SurfaceIdForTesting();
  while (!ContainsSurfaceId(root_surface_id)) {
    // TODO(kenrb): Need a better way to do this. Needs investigation on
    // whether we can add a callback through RenderWidgetHostViewBaseObserver
    // from OnSwapCompositorFrame and avoid this busy waiting. A callback on
    // every compositor frame might be generally undesirable for performance,
    // however.
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }
}

bool SurfaceHitTestReadyNotifier::ContainsSurfaceId(
    const viz::SurfaceId& container_surface_id) {
  if (!container_surface_id.is_valid())
    return false;

  viz::Surface* container_surface =
      surface_manager_->GetSurfaceForId(container_surface_id);
  if (!container_surface || !container_surface->active_referenced_surfaces())
    return false;

  for (const viz::SurfaceId& id :
       *container_surface->active_referenced_surfaces()) {
    if (id == target_view_->SurfaceIdForTesting() || ContainsSurfaceId(id))
      return true;
  }
  return false;
}

}  // namespace

#if defined(USE_AURA)
void WaitForGuestSurfaceReady(content::WebContents* guest_web_contents) {
  RenderWidgetHostViewChildFrame* child_view =
      static_cast<RenderWidgetHostViewChildFrame*>(
          guest_web_contents->GetRenderWidgetHostView());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      static_cast<content::WebContentsImpl*>(guest_web_contents)
          ->GetOuterWebContents()
          ->GetRenderWidgetHostView());

  SurfaceHitTestReadyNotifier notifier(child_view);
  notifier.WaitForSurfaceReady(root_view);
}

#endif

void WaitForChildFrameSurfaceReady(content::RenderFrameHost* child_frame) {
  RenderWidgetHostViewBase* child_view =
      static_cast<RenderFrameHostImpl*>(child_frame)
          ->GetRenderWidgetHost()
          ->GetView();
  if (!child_view || !child_view->IsRenderWidgetHostViewChildFrame())
    return;

  RenderWidgetHostViewBase* root_view =
      static_cast<CrossProcessFrameConnector*>(
          static_cast<RenderWidgetHostViewChildFrame*>(child_view)
              ->FrameConnectorForTesting())
          ->GetRootRenderWidgetHostViewForTesting();

  SurfaceHitTestReadyNotifier notifier(child_view);
  notifier.WaitForSurfaceReady(root_view);
}

TitleWatcher::TitleWatcher(WebContents* web_contents,
                           const base::string16& expected_title)
    : WebContentsObserver(web_contents) {
  expected_titles_.push_back(expected_title);
}

void TitleWatcher::AlsoWaitForTitle(const base::string16& expected_title) {
  expected_titles_.push_back(expected_title);
}

TitleWatcher::~TitleWatcher() {
}

const base::string16& TitleWatcher::WaitAndGetTitle() {
  TestTitle();
  run_loop_.Run();
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
  const base::string16& current_title = web_contents()->GetTitle();
  if (base::ContainsValue(expected_titles_, current_title)) {
    observed_title_ = current_title;
    run_loop_.Quit();
  }
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

DOMMessageQueue::DOMMessageQueue(WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  registrar_.Add(this, NOTIFICATION_DOM_OPERATION_RESPONSE,
                 Source<WebContents>(web_contents));
}

DOMMessageQueue::~DOMMessageQueue() {}

void DOMMessageQueue::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  Details<std::string> dom_op_result(details);
  message_queue_.push(*dom_op_result.ptr());
  if (message_loop_runner_)
    message_loop_runner_->Quit();
}

void DOMMessageQueue::RenderProcessGone(base::TerminationStatus status) {
  VLOG(0) << "DOMMessageQueue::RenderProcessGone " << status;
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
    case base::TERMINATION_STATUS_STILL_RUNNING:
      break;
    default:
      renderer_crashed_ = true;
      if (message_loop_runner_.get())
        message_loop_runner_->Quit();
      break;
  }
}

void DOMMessageQueue::ClearQueue() {
  message_queue_ = base::queue<std::string>();
}

bool DOMMessageQueue::WaitForMessage(std::string* message) {
  DCHECK(message);
  if (!renderer_crashed_ && message_queue_.empty()) {
    // This will be quit when a new message comes in.
    message_loop_runner_ =
        new MessageLoopRunner(MessageLoopRunner::QuitMode::IMMEDIATE);
    message_loop_runner_->Run();
  }
  return PopMessage(message);
}

bool DOMMessageQueue::PopMessage(std::string* message) {
  DCHECK(message);
  if (renderer_crashed_ || message_queue_.empty())
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
  WebContentsImpl::FriendWrapper::AddCreatedCallbackForTesting(
      web_contents_created_callback_);
}

WebContentsAddedObserver::~WebContentsAddedObserver() {
  WebContentsImpl::FriendWrapper::RemoveCreatedCallbackForTesting(
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

FrameWatcher::FrameWatcher() = default;

FrameWatcher::FrameWatcher(WebContents* web_contents)
    : WebContentsObserver(web_contents) {}

FrameWatcher::~FrameWatcher() = default;

void FrameWatcher::WaitFrames(int frames_to_wait) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (frames_to_wait <= 0)
    return;
  base::RunLoop run_loop;
  base::AutoReset<base::Closure> reset_quit(&quit_, run_loop.QuitClosure());
  base::AutoReset<int> reset_frames_to_wait(&frames_to_wait_, frames_to_wait);
  run_loop.Run();
}

const cc::CompositorFrameMetadata& FrameWatcher::LastMetadata() {
  return RenderWidgetHostImpl::From(
             web_contents()->GetRenderViewHost()->GetWidget())
      ->last_frame_metadata();
}

void FrameWatcher::DidReceiveCompositorFrame() {
  --frames_to_wait_;
  if (frames_to_wait_ == 0)
    quit_.Run();
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
        base::BindOnce(&MainThreadFrameObserver::Quit, base::Unretained(this)));
  }
  return true;
}

InputMsgWatcher::InputMsgWatcher(RenderWidgetHost* render_widget_host,
                                 blink::WebInputEvent::Type type)
    : BrowserMessageFilter(InputMsgStart),
      wait_for_type_(type),
      ack_result_(INPUT_EVENT_ACK_STATE_UNKNOWN),
      ack_source_(static_cast<uint32_t>(InputEventAckSource::UNKNOWN)) {
  render_widget_host->GetProcess()->AddFilter(this);
}

InputMsgWatcher::~InputMsgWatcher() {}

void InputMsgWatcher::ReceivedAck(blink::WebInputEvent::Type ack_type,
                                  uint32_t ack_state,
                                  uint32_t ack_source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (wait_for_type_ == ack_type) {
    ack_result_ = ack_state;
    ack_source_ = ack_source;
    if (!quit_.is_null())
      quit_.Run();
  }
}

bool InputMsgWatcher::OnMessageReceived(const IPC::Message& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (message.type() == InputHostMsg_HandleInputEvent_ACK::ID) {
    InputHostMsg_HandleInputEvent_ACK::Param params;
    InputHostMsg_HandleInputEvent_ACK::Read(&message, &params);
    blink::WebInputEvent::Type ack_type = std::get<0>(params).type;
    InputEventAckState ack_state = std::get<0>(params).state;
    InputEventAckSource ack_source = std::get<0>(params).source;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&InputMsgWatcher::ReceivedAck, this, ack_type, ack_state,
                       static_cast<uint32_t>(ack_source)));
  }
  return false;
}

bool InputMsgWatcher::HasReceivedAck() const {
  return ack_result_ != INPUT_EVENT_ACK_STATE_UNKNOWN;
}

uint32_t InputMsgWatcher::WaitForAck() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (HasReceivedAck())
    return ack_result_;
  base::RunLoop run_loop;
  base::AutoReset<base::Closure> reset_quit(&quit_, run_loop.QuitClosure());
  run_loop.Run();
  return ack_result_;
}

#if defined(OS_WIN)
static void RunTaskAndSignalCompletion(const base::Closure& task,
                                       base::WaitableEvent* completion) {
  task.Run();
  completion->Signal();
}

static void RunTaskOnIOThreadAndWait(const base::Closure& task) {
  base::WaitableEvent completion(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&RunTaskAndSignalCompletion, task, &completion));
  completion.Wait();
}
#endif

static void SetUpTestClipboard() {
#if defined(OS_WIN)
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    RunTaskOnIOThreadAndWait(base::Bind(&SetUpTestClipboard));
    return;
  }
#endif
  ui::TestClipboard::CreateForCurrentThread();
}

// TODO(dcheng): Make the test clipboard on different threads share the
// same backing store. crbug.com/629765
BrowserTestClipboardScope::BrowserTestClipboardScope() {
  SetUpTestClipboard();
}

static void TearDownTestClipboard() {
#if defined(OS_WIN)
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    RunTaskOnIOThreadAndWait(base::Bind(&TearDownTestClipboard));
    return;
  }
#endif
  ui::Clipboard::DestroyClipboardForCurrentThread();
}

BrowserTestClipboardScope::~BrowserTestClipboardScope() {
  TearDownTestClipboard();
}

void BrowserTestClipboardScope::SetRtf(const std::string& rtf) {
#if defined(OS_WIN)
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    RunTaskOnIOThreadAndWait(base::Bind(&BrowserTestClipboardScope::SetRtf,
                                        base::Unretained(this), rtf));
    return;
  }
#endif
  ui::ScopedClipboardWriter clipboard_writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
  clipboard_writer.WriteRTF(rtf);
}

void BrowserTestClipboardScope::SetText(const std::string& text) {
#if defined(OS_WIN)
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    RunTaskOnIOThreadAndWait(base::Bind(&BrowserTestClipboardScope::SetText,
                                        base::Unretained(this), text));
    return;
  }
#endif
  ui::ScopedClipboardWriter clipboard_writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
  clipboard_writer.WriteText(base::ASCIIToUTF16(text));
}

class FrameFocusedObserver::FrameTreeNodeObserverImpl
    : public FrameTreeNode::Observer {
 public:
  explicit FrameTreeNodeObserverImpl(FrameTreeNode* owner)
      : owner_(owner), message_loop_runner_(new MessageLoopRunner) {
    owner->AddObserver(this);
  }
  ~FrameTreeNodeObserverImpl() override { owner_->RemoveObserver(this); }

  void Run() { message_loop_runner_->Run(); }

  void OnFrameTreeNodeFocused(FrameTreeNode* node) override {
    if (node == owner_)
      message_loop_runner_->Quit();
  }

 private:
  FrameTreeNode* owner_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
};

FrameFocusedObserver::FrameFocusedObserver(RenderFrameHost* owner_host)
    : impl_(new FrameTreeNodeObserverImpl(
          static_cast<RenderFrameHostImpl*>(owner_host)->frame_tree_node())) {}

FrameFocusedObserver::~FrameFocusedObserver() {}

void FrameFocusedObserver::Wait() {
  impl_->Run();
}

TestNavigationManager::TestNavigationManager(WebContents* web_contents,
                                             const GURL& url)
    : WebContentsObserver(web_contents),
      url_(url),
      handle_(nullptr),
      navigation_paused_(false),
      current_state_(NavigationState::INITIAL),
      desired_state_(NavigationState::STARTED),
      weak_factory_(this) {}

TestNavigationManager::~TestNavigationManager() {
  if (navigation_paused_)
    handle_->CallResumeForTesting();
}

bool TestNavigationManager::WaitForRequestStart() {
  // This is the default desired state. In PlzNavigate, a browser-initiated
  // navigation can reach this state synchronously, so the TestNavigationManager
  // is set to always pause navigations at WillStartRequest. This ensures the
  // user can always call WaitForWillStartRequest.
  DCHECK(desired_state_ == NavigationState::STARTED);
  return WaitForDesiredState();
}

bool TestNavigationManager::WaitForResponse() {
  desired_state_ = NavigationState::RESPONSE;
  return WaitForDesiredState();
}

void TestNavigationManager::WaitForNavigationFinished() {
  desired_state_ = NavigationState::FINISHED;
  WaitForDesiredState();
}

void TestNavigationManager::DidStartNavigation(NavigationHandle* handle) {
  if (!ShouldMonitorNavigation(handle))
    return;

  handle_ = handle;
  std::unique_ptr<NavigationThrottle> throttle(
      new TestNavigationManagerThrottle(
          handle_, base::Bind(&TestNavigationManager::OnWillStartRequest,
                              weak_factory_.GetWeakPtr()),
          base::Bind(&TestNavigationManager::OnWillProcessResponse,
                     weak_factory_.GetWeakPtr())));
  handle_->RegisterThrottleForTesting(std::move(throttle));
}

void TestNavigationManager::DidFinishNavigation(NavigationHandle* handle) {
  if (handle != handle_)
    return;
  current_state_ = NavigationState::FINISHED;
  navigation_paused_ = false;
  handle_ = nullptr;
  OnNavigationStateChanged();
}

void TestNavigationManager::OnWillStartRequest() {
  current_state_ = NavigationState::STARTED;
  navigation_paused_ = true;
  OnNavigationStateChanged();
}

void TestNavigationManager::OnWillProcessResponse() {
  current_state_ = NavigationState::RESPONSE;
  navigation_paused_ = true;
  OnNavigationStateChanged();
}

// TODO(csharrison): Remove CallResumeForTesting method calls in favor of doing
// it through the throttle.
bool TestNavigationManager::WaitForDesiredState() {
  // If the desired state has laready been reached, just return.
  if (current_state_ == desired_state_)
    return true;

  // Resume the navigation if it was paused.
  if (navigation_paused_)
    handle_->CallResumeForTesting();

  // Wait for the desired state if needed.
  if (current_state_ < desired_state_) {
    DCHECK(!loop_runner_);
    loop_runner_ = new MessageLoopRunner();
    loop_runner_->Run();
    loop_runner_ = nullptr;
  }

  // Return false if the navigation did not reach the state specified by the
  // user.
  return current_state_ == desired_state_;
}

void TestNavigationManager::OnNavigationStateChanged() {
  // If the state the user was waiting for has been reached, exit the message
  // loop.
  if (current_state_ >= desired_state_) {
    if (loop_runner_)
      loop_runner_->Quit();
    return;
  }

  // Otherwise, the navigation should be resumed if it was previously paused.
  if (navigation_paused_)
    handle_->CallResumeForTesting();
}

bool TestNavigationManager::ShouldMonitorNavigation(NavigationHandle* handle) {
  if (handle_ || handle->GetURL() != url_)
    return false;
  if (current_state_ != NavigationState::INITIAL)
    return false;
  return true;
}

NavigationHandleCommitObserver::NavigationHandleCommitObserver(
    content::WebContents* web_contents,
    const GURL& url)
    : content::WebContentsObserver(web_contents),
      url_(url),
      has_committed_(false),
      was_same_document_(false),
      was_renderer_initiated_(false) {}

void NavigationHandleCommitObserver::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (handle->GetURL() != url_)
    return;
  has_committed_ = true;
  was_same_document_ = handle->IsSameDocument();
  was_renderer_initiated_ = handle->IsRendererInitiated();
}

ConsoleObserverDelegate::ConsoleObserverDelegate(WebContents* web_contents,
                                                 const std::string& filter)
    : web_contents_(web_contents),
      filter_(filter),
      message_loop_runner_(new MessageLoopRunner) {}

ConsoleObserverDelegate::~ConsoleObserverDelegate() {}

void ConsoleObserverDelegate::Wait() {
  message_loop_runner_->Run();
}

bool ConsoleObserverDelegate::DidAddMessageToConsole(
    WebContents* source,
    int32_t level,
    const base::string16& message,
    int32_t line_no,
    const base::string16& source_id) {
  DCHECK(source == web_contents_);

  std::string ascii_message = base::UTF16ToASCII(message);
  if (base::MatchPattern(ascii_message, filter_)) {
    message_ = ascii_message;
    message_loop_runner_->Quit();
  }
  return false;
}

// static
void PwnMessageHelper::CreateBlobWithPayload(RenderProcessHost* process,
                                             std::string uuid,
                                             std::string content_type,
                                             std::string content_disposition,
                                             std::string payload) {
  std::vector<storage::DataElement> data_elements(1);
  data_elements[0].SetToBytes(payload.c_str(), payload.size());

  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      process->GetChannel(),
      BlobStorageMsg_RegisterBlob(uuid, content_type, content_disposition,
                                  data_elements));
}


// static
void PwnMessageHelper::RegisterBlobURL(RenderProcessHost* process,
                                       GURL url,
                                       std::string uuid) {
  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      process->GetChannel(), BlobHostMsg_RegisterPublicURL(url, uuid));
}

// static
void PwnMessageHelper::FileSystemCreate(RenderProcessHost* process,
                                        int request_id,
                                        GURL path,
                                        bool exclusive,
                                        bool is_directory,
                                        bool recursive) {
  TestFileapiOperationWaiter waiter(
      process->GetStoragePartition()->GetFileSystemContext());

  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      process->GetChannel(),
      FileSystemHostMsg_Create(request_id, path, exclusive, is_directory,
                               recursive));

  // If this started an async operation, wait for it to complete.
  if (waiter.did_start_update())
    waiter.WaitForEndUpdate();
}

// static
void PwnMessageHelper::FileSystemWrite(RenderProcessHost* process,
                                       int request_id,
                                       GURL file_path,
                                       std::string blob_uuid,
                                       int64_t position) {
  TestFileapiOperationWaiter waiter(
      process->GetStoragePartition()->GetFileSystemContext());

  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      process->GetChannel(),
      FileSystemHostMsg_Write(request_id, file_path, blob_uuid, position));

  // If this started an async operation, wait for it to complete.
  if (waiter.did_start_update())
    waiter.WaitForEndUpdate();
}

void PwnMessageHelper::LockMouse(RenderProcessHost* process,
                                 int routing_id,
                                 bool user_gesture,
                                 bool privileged) {
  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      process->GetChannel(),
      ViewHostMsg_LockMouse(routing_id, user_gesture, privileged));
}

#if defined(USE_AURA)
namespace {
class MockOverscrollControllerImpl : public OverscrollController,
                                     public MockOverscrollController {
 public:
  MockOverscrollControllerImpl()
      : content_scrolling_(false),
        message_loop_runner_(new MessageLoopRunner) {}
  ~MockOverscrollControllerImpl() override {}

  // OverscrollController:
  void ReceivedEventACK(const blink::WebInputEvent& event,
                        bool processed) override {
    // Since we're only mocking this one method of OverscrollController and its
    // other methods are non-virtual, we'll delegate to it so that it doesn't
    // get into an inconsistent state.
    OverscrollController::ReceivedEventACK(event, processed);

    if (event.GetType() == blink::WebInputEvent::kGestureScrollUpdate &&
        processed) {
      content_scrolling_ = true;
      if (message_loop_runner_->loop_running())
        message_loop_runner_->Quit();
    }
  }

  // MockOverscrollController:
  void WaitForConsumedScroll() override {
    if (!content_scrolling_)
      message_loop_runner_->Run();
  }

 private:
  bool content_scrolling_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockOverscrollControllerImpl);
};
}  // namespace

// static
MockOverscrollController* MockOverscrollController::Create(
    RenderWidgetHostView* rwhv) {
  std::unique_ptr<MockOverscrollControllerImpl> mock =
      base::MakeUnique<MockOverscrollControllerImpl>();
  MockOverscrollController* raw_mock = mock.get();

  RenderWidgetHostViewAura* rwhva =
      static_cast<RenderWidgetHostViewAura*>(rwhv);
  rwhva->SetOverscrollControllerForTesting(std::move(mock));

  return raw_mock;
}

#endif  // defined(USE_AURA)

}  // namespace content
