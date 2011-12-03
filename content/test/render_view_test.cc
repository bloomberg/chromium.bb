// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/render_view_test.h"

#include "content/common/dom_storage_common.h"
#include "content/common/view_messages.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/renderer_preferences.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_main_platform_delegate.h"
#include "content/test/mock_render_process.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webkit_glue.h"

#if defined(OS_LINUX) && !defined(USE_AURA)
#include "ui/base/gtk/event_synthesis_gtk.h"
#endif

using WebKit::WebFrame;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebScriptController;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebURLRequest;

namespace {
const int32 kOpenerId = 7;
const int32 kRouteId = 5;
}  // namespace

namespace content {

RenderViewTest::RenderViewTest() : view_(NULL) {
}

RenderViewTest::~RenderViewTest() {
}

void RenderViewTest::ProcessPendingMessages() {
  msg_loop_.PostTask(FROM_HERE, new MessageLoop::QuitTask());
  msg_loop_.Run();
}

WebFrame* RenderViewTest::GetMainFrame() {
  return view_->GetWebView()->mainFrame();
}

void RenderViewTest::ExecuteJavaScript(const char* js) {
  GetMainFrame()->executeScript(WebScriptSource(WebString::fromUTF8(js)));
}

bool RenderViewTest::ExecuteJavaScriptAndReturnIntValue(
    const string16& script,
    int* int_result) {
  v8::Handle<v8::Value> result =
      GetMainFrame()->executeScriptAndReturnValue(WebScriptSource(script));
  if (result.IsEmpty() || !result->IsInt32())
    return false;

  if (int_result)
    *int_result = result->Int32Value();

  return true;
}

void RenderViewTest::LoadHTML(const char* html) {
  std::string url_str = "data:text/html;charset=utf-8,";
  url_str.append(html);
  GURL url(url_str);

  GetMainFrame()->loadRequest(WebURLRequest(url));

  // The load actually happens asynchronously, so we pump messages to process
  // the pending continuation.
  ProcessPendingMessages();
}

void RenderViewTest::SetUp() {
  // Subclasses can set the ContentClient's renderer before calling
  // RenderViewTest::SetUp().
  if (!GetContentClient()->renderer())
    GetContentClient()->set_renderer(&mock_content_renderer_client_);

  // Subclasses can set render_thread_ with their own implementation before
  // calling RenderViewTest::SetUp().
  if (!render_thread_.get())
    render_thread_.reset(new MockRenderThread());
  render_thread_->set_routing_id(kRouteId);

  command_line_.reset(new CommandLine(CommandLine::NO_PROGRAM));
  params_.reset(new content::MainFunctionParams(*command_line_));
  platform_.reset(new RendererMainPlatformDelegate(*params_));
  platform_->PlatformInitialize();

  // Setting flags and really doing anything with WebKit is fairly fragile and
  // hacky, but this is the world we live in...
  webkit_glue::SetJavaScriptFlags(" --expose-gc");
  WebKit::initialize(&webkit_platform_support_);

  mock_process_.reset(new MockRenderProcess);

  // This needs to pass the mock render thread to the view.
  RenderViewImpl* view = RenderViewImpl::Create(
      0,
      kOpenerId,
      content::RendererPreferences(),
      WebPreferences(),
      new SharedRenderViewCounter(0),
      kRouteId,
      kInvalidSessionStorageNamespaceId,
      string16());
  view->AddRef();
  view_ = view;

  // Attach a pseudo keyboard device to this object.
  mock_keyboard_.reset(new MockKeyboard());
}

void RenderViewTest::TearDown() {
  // Try very hard to collect garbage before shutting down.
  GetMainFrame()->collectGarbage();
  GetMainFrame()->collectGarbage();

  // Run the loop so the release task from the renderwidget executes.
  ProcessPendingMessages();

  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->Release();
  view_ = NULL;

  mock_process_.reset();

  // After resetting the view_ and mock_process_ we may get some new tasks
  // which need to be processed before shutting down WebKit
  // (http://crbug.com/21508).
  msg_loop_.RunAllPending();

  WebKit::shutdown();

  mock_keyboard_.reset();

  platform_->PlatformUninitialize();
  platform_.reset();
  params_.reset();
  command_line_.reset();
}

int RenderViewTest::SendKeyEvent(MockKeyboard::Layout layout,
                                 int key_code,
                                 MockKeyboard::Modifiers modifiers,
                                 std::wstring* output) {
#if defined(USE_AURA)
  NOTIMPLEMENTED();
  return L'\0';
#elif defined(OS_WIN)
  // Retrieve the Unicode character for the given tuple (keyboard-layout,
  // key-code, and modifiers).
  // Exit when a keyboard-layout driver cannot assign a Unicode character to
  // the tuple to prevent sending an invalid key code to the RenderView object.
  CHECK(mock_keyboard_.get());
  CHECK(output);
  int length = mock_keyboard_->GetCharacters(layout, key_code, modifiers,
                                             output);
  if (length != 1)
    return -1;

  // Create IPC messages from Windows messages and send them to our
  // back-end.
  // A keyboard event of Windows consists of three Windows messages:
  // WM_KEYDOWN, WM_CHAR, and WM_KEYUP.
  // WM_KEYDOWN and WM_KEYUP sends virtual-key codes. On the other hand,
  // WM_CHAR sends a composed Unicode character.
  MSG msg1 = { NULL, WM_KEYDOWN, key_code, 0 };
  NativeWebKeyboardEvent keydown_event(msg1);
  SendNativeKeyEvent(keydown_event);

  MSG msg2 = { NULL, WM_CHAR, (*output)[0], 0 };
  NativeWebKeyboardEvent char_event(msg2);
  SendNativeKeyEvent(char_event);

  MSG msg3 = { NULL, WM_KEYUP, key_code, 0 };
  NativeWebKeyboardEvent keyup_event(msg3);
  SendNativeKeyEvent(keyup_event);

  return length;
#elif defined(OS_LINUX)
  // We ignore |layout|, which means we are only testing the layout of the
  // current locale. TODO(estade): fix this to respect |layout|.
  std::vector<GdkEvent*> events;
  ui::SynthesizeKeyPressEvents(
      NULL, static_cast<ui::KeyboardCode>(key_code),
      modifiers & (MockKeyboard::LEFT_CONTROL | MockKeyboard::RIGHT_CONTROL),
      modifiers & (MockKeyboard::LEFT_SHIFT | MockKeyboard::RIGHT_SHIFT),
      modifiers & (MockKeyboard::LEFT_ALT | MockKeyboard::RIGHT_ALT),
      &events);

  guint32 unicode_key = 0;
  for (size_t i = 0; i < events.size(); ++i) {
    // Only send the up/down events for key press itself (skip the up/down
    // events for the modifier keys).
    if ((i + 1) == (events.size() / 2) || i == (events.size() / 2)) {
      unicode_key = gdk_keyval_to_unicode(events[i]->key.keyval);
      NativeWebKeyboardEvent webkit_event(events[i]);
      SendNativeKeyEvent(webkit_event);

      // Need to add a char event after the key down.
      if (webkit_event.type == WebKit::WebInputEvent::RawKeyDown) {
        NativeWebKeyboardEvent char_event = webkit_event;
        char_event.type = WebKit::WebInputEvent::Char;
        char_event.skip_in_browser = true;
        SendNativeKeyEvent(char_event);
      }
    }
    gdk_event_free(events[i]);
  }

  *output = std::wstring(1, unicode_key);
  return 1;
#else
  NOTIMPLEMENTED();
  return L'\0';
#endif
}

void RenderViewTest::SendNativeKeyEvent(
    const NativeWebKeyboardEvent& key_event) {
  SendWebKeyboardEvent(key_event);
}

void RenderViewTest::SendWebKeyboardEvent(
    const WebKit::WebKeyboardEvent& key_event) {
  scoped_ptr<IPC::Message> input_message(new ViewMsg_HandleInputEvent(0));
  input_message->WriteData(reinterpret_cast<const char*>(&key_event),
                           sizeof(WebKit::WebKeyboardEvent));
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->OnMessageReceived(*input_message);
}

const char* const kGetCoordinatesScript =
    "(function() {"
    "  function GetCoordinates(elem) {"
    "    if (!elem)"
    "      return [ 0, 0];"
    "    var coordinates = [ elem.offsetLeft, elem.offsetTop];"
    "    var parent_coordinates = GetCoordinates(elem.offsetParent);"
    "    coordinates[0] += parent_coordinates[0];"
    "    coordinates[1] += parent_coordinates[1];"
    "    return coordinates;"
    "  };"
    "  var elem = document.getElementById('$1');"
    "  if (!elem)"
    "    return null;"
    "  var bounds = GetCoordinates(elem);"
    "  bounds[2] = elem.offsetWidth;"
    "  bounds[3] = elem.offsetHeight;"
    "  return bounds;"
    "})();";
gfx::Rect RenderViewTest::GetElementBounds(const std::string& element_id) {
  std::vector<std::string> params;
  params.push_back(element_id);
  std::string script =
      ReplaceStringPlaceholders(kGetCoordinatesScript, params, NULL);

  v8::HandleScope handle_scope;
  v8::Handle<v8::Value>  value = GetMainFrame()->executeScriptAndReturnValue(
      WebScriptSource(WebString::fromUTF8(script)));
  if (value.IsEmpty() || !value->IsArray())
    return gfx::Rect();

  v8::Handle<v8::Array> array = value.As<v8::Array>();
  if (array->Length() != 4)
    return gfx::Rect();
  std::vector<int> coords;
  for (int i = 0; i < 4; ++i) {
    v8::Handle<v8::Number> index = v8::Number::New(i);
    v8::Local<v8::Value> value = array->Get(index);
    if (value.IsEmpty() || !value->IsInt32())
      return gfx::Rect();
    coords.push_back(value->Int32Value());
  }
  return gfx::Rect(coords[0], coords[1], coords[2], coords[3]);
}

bool RenderViewTest::SimulateElementClick(const std::string& element_id) {
  gfx::Rect bounds = GetElementBounds(element_id);
  if (bounds.IsEmpty())
    return false;
  WebMouseEvent mouse_event;
  mouse_event.type = WebInputEvent::MouseDown;
  mouse_event.button = WebMouseEvent::ButtonLeft;
  mouse_event.x = bounds.CenterPoint().x();
  mouse_event.y = bounds.CenterPoint().y();
  mouse_event.clickCount = 1;
  ViewMsg_HandleInputEvent input_event(0);
  scoped_ptr<IPC::Message> input_message(new ViewMsg_HandleInputEvent(0));
  input_message->WriteData(reinterpret_cast<const char*>(&mouse_event),
                           sizeof(WebMouseEvent));
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->OnMessageReceived(*input_message);
  return true;
}

void RenderViewTest::ClearHistory() {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->page_id_ = -1;
  impl->history_list_offset_ = -1;
  impl->history_list_length_ = 0;
  impl->history_page_ids_.clear();
}

void RenderViewTest::Reload(const GURL& url) {
  ViewMsg_Navigate_Params params;
  params.url = url;
  params.navigation_type = ViewMsg_Navigate_Type::RELOAD;
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->OnNavigate(params);
}

uint32 RenderViewTest::GetNavigationIPCType() {
  return ViewHostMsg_FrameNavigate::ID;
}

bool RenderViewTest::OnMessageReceived(const IPC::Message& msg) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  return impl->OnMessageReceived(msg);
}

void RenderViewTest::DidNavigateWithinPage(WebKit::WebFrame* frame,
                                           bool is_new_navigation) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->didNavigateWithinPage(frame, is_new_navigation);
}

void RenderViewTest::SendContentStateImmediately() {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->set_send_content_state_immediately(true);
}

WebKit::WebWidget* RenderViewTest::GetWebWidget() {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  return impl->webwidget();
}

}  // namespace content
