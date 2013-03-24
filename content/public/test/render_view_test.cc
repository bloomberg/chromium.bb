// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/render_view_test.h"

#include "base/run_loop.h"
#include "content/common/view_messages.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/renderer_preferences.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_main_platform_delegate.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "content/test/mock_render_process.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScriptSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/dom_storage/dom_storage_types.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebFrame;
using WebKit::WebInputEvent;
using WebKit::WebMouseEvent;
using WebKit::WebScriptController;
using WebKit::WebScriptSource;
using WebKit::WebString;
using WebKit::WebURLRequest;

namespace {
const int32 kOpenerId = -2;
const int32 kRouteId = 5;
const int32 kNewWindowRouteId = 6;
const int32 kSurfaceId = 42;

}  // namespace

namespace content {

class RendererWebKitPlatformSupportImplNoSandboxImpl
    : public RendererWebKitPlatformSupportImpl {
 public:
  virtual WebKit::WebSandboxSupport* sandboxSupport() {
    return NULL;
  }
};

RenderViewTest::RendererWebKitPlatformSupportImplNoSandbox::
    RendererWebKitPlatformSupportImplNoSandbox() {
  webkit_platform_support_.reset(
      new RendererWebKitPlatformSupportImplNoSandboxImpl());
}

RenderViewTest::RendererWebKitPlatformSupportImplNoSandbox::
    ~RendererWebKitPlatformSupportImplNoSandbox() {
}

WebKit::Platform*
    RenderViewTest::RendererWebKitPlatformSupportImplNoSandbox::Get() {
  return webkit_platform_support_.get();
}

RenderViewTest::RenderViewTest()
    : view_(NULL) {
}

RenderViewTest::~RenderViewTest() {
}

void RenderViewTest::ProcessPendingMessages() {
  msg_loop_.PostTask(FROM_HERE, MessageLoop::QuitClosure());
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
  v8::HandleScope handle_scope;
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

void RenderViewTest::GoBack(const WebKit::WebHistoryItem& item) {
  GoToOffset(-1, item);
}

void RenderViewTest::GoForward(const WebKit::WebHistoryItem& item) {
  GoToOffset(1, item);
}

void RenderViewTest::SetUp() {
  // Subclasses can set the ContentClient's renderer before calling
  // RenderViewTest::SetUp().
  if (!GetContentClient()->renderer())
    GetContentClient()->set_renderer_for_testing(&content_renderer_client_);

  // Subclasses can set render_thread_ with their own implementation before
  // calling RenderViewTest::SetUp().
  if (!render_thread_.get())
    render_thread_.reset(new MockRenderThread());
  render_thread_->set_routing_id(kRouteId);
  render_thread_->set_surface_id(kSurfaceId);
  render_thread_->set_new_window_routing_id(kNewWindowRouteId);

  command_line_.reset(new CommandLine(CommandLine::NO_PROGRAM));
  params_.reset(new MainFunctionParams(*command_line_));
  platform_.reset(new RendererMainPlatformDelegate(*params_));
  platform_->PlatformInitialize();

  // Setting flags and really doing anything with WebKit is fairly fragile and
  // hacky, but this is the world we live in...
  webkit_glue::SetJavaScriptFlags(" --expose-gc");
  WebKit::initialize(webkit_platform_support_.Get());

  // Ensure that we register any necessary schemes when initializing WebKit,
  // since we are using a MockRenderThread.
  RenderThreadImpl::RegisterSchemes();

  // This check is needed because when run under content_browsertests,
  // ResourceBundle isn't initialized (since we have to use a diferent test
  // suite implementation than for content_unittests). For browser_tests, this
  // is already initialized.
  if (!ResourceBundle::HasSharedInstance())
    ResourceBundle::InitSharedInstanceWithLocale("en-US", NULL);

  mock_process_.reset(new MockRenderProcess);

  // This needs to pass the mock render thread to the view.
  RenderViewImpl* view = RenderViewImpl::Create(
      kOpenerId,
      RendererPreferences(),
      webkit_glue::WebPreferences(),
      new SharedRenderViewCounter(0),
      kRouteId,
      kSurfaceId,
      dom_storage::kInvalidSessionStorageNamespaceId,
      string16(),
      false,
      false,
      1,
      WebKit::WebScreenInfo(),
      AccessibilityModeOff,
      true);
  view->AddRef();
  view_ = view;
}

void RenderViewTest::TearDown() {
  // Try very hard to collect garbage before shutting down.
  GetMainFrame()->collectGarbage();
  GetMainFrame()->collectGarbage();

  // Run the loop so the release task from the renderwidget executes.
  ProcessPendingMessages();

  render_thread_->SendCloseMessage();
  view_ = NULL;
  mock_process_.reset();

  // After telling the view to close and resetting mock_process_ we may get
  // some new tasks which need to be processed before shutting down WebKit
  // (http://crbug.com/21508).
  base::RunLoop().RunUntilIdle();

  WebKit::shutdown();

  platform_->PlatformUninitialize();
  platform_.reset();
  params_.reset();
  command_line_.reset();
}

void RenderViewTest::SendNativeKeyEvent(
    const NativeWebKeyboardEvent& key_event) {
  SendWebKeyboardEvent(key_event);
}

void RenderViewTest::SendWebKeyboardEvent(
    const WebKit::WebKeyboardEvent& key_event) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->OnMessageReceived(ViewMsg_HandleInputEvent(0, &key_event, false));
}

void RenderViewTest::SendWebMouseEvent(
    const WebKit::WebMouseEvent& mouse_event) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->OnMessageReceived(ViewMsg_HandleInputEvent(0, &mouse_event, false));
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
  scoped_ptr<IPC::Message> input_message(
      new ViewMsg_HandleInputEvent(0, &mouse_event, false));
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->OnMessageReceived(*input_message);
  return true;
}

void RenderViewTest::SetFocused(const WebKit::WebNode& node) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->focusedNodeChanged(node);
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

void RenderViewTest::Resize(gfx::Size new_size,
                            gfx::Rect resizer_rect,
                            bool is_fullscreen) {
  scoped_ptr<IPC::Message> resize_message(new ViewMsg_Resize(
      0, new_size, new_size, 0.f, resizer_rect, is_fullscreen));
  OnMessageReceived(*resize_message);
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

void RenderViewTest::GoToOffset(int offset,
                                const WebKit::WebHistoryItem& history_item) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);

  int history_list_length = impl->historyBackListCount() +
                            impl->historyForwardListCount() + 1;
  int pending_offset = offset + impl->history_list_offset();

  ViewMsg_Navigate_Params navigate_params;
  navigate_params.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  navigate_params.transition = PAGE_TRANSITION_FORWARD_BACK;
  navigate_params.current_history_list_length = history_list_length;
  navigate_params.current_history_list_offset = impl->history_list_offset();
  navigate_params.pending_history_list_offset = pending_offset;
  navigate_params.page_id = impl->GetPageId() + offset;
  navigate_params.state = webkit_glue::HistoryItemToString(history_item);
  navigate_params.request_time = base::Time::Now();

  ViewMsg_Navigate navigate_message(impl->GetRoutingID(), navigate_params);
  OnMessageReceived(navigate_message);

  // The load actually happens asynchronously, so we pump messages to process
  // the pending continuation.
  ProcessPendingMessages();
}

}  // namespace content
