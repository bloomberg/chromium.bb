// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/render_view_test.h"

#include "base/run_loop.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/common/content_client.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/history_controller.h"
#include "content/renderer/history_serialization.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/renderer_main_platform_delegate.h"
#include "content/renderer/renderer_webkitplatformsupport_impl.h"
#include "content/test/frame_load_waiter.h"
#include "content/test/mock_render_process.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebKit.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/resource/resource_bundle.h"
#include "v8/include/v8.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

using blink::WebInputEvent;
using blink::WebLocalFrame;
using blink::WebMouseEvent;
using blink::WebScriptSource;
using blink::WebString;
using blink::WebURLRequest;

namespace {
const int32 kOpenerId = -2;
const int32 kRouteId = 5;
const int32 kMainFrameRouteId = 6;
const int32 kNewWindowRouteId = 7;
const int32 kNewFrameRouteId = 10;
const int32 kSurfaceId = 42;

}  // namespace

namespace content {

class RendererWebKitPlatformSupportImplNoSandboxImpl
    : public RendererWebKitPlatformSupportImpl {
 public:
  virtual blink::WebSandboxSupport* sandboxSupport() {
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

blink::Platform*
    RenderViewTest::RendererWebKitPlatformSupportImplNoSandbox::Get() {
  return webkit_platform_support_.get();
}

RenderViewTest::RenderViewTest()
    : view_(NULL) {
}

RenderViewTest::~RenderViewTest() {
}

void RenderViewTest::ProcessPendingMessages() {
  msg_loop_.PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  msg_loop_.Run();
}

WebLocalFrame* RenderViewTest::GetMainFrame() {
  return view_->GetWebView()->mainFrame()->toWebLocalFrame();
}

void RenderViewTest::ExecuteJavaScript(const char* js) {
  GetMainFrame()->executeScript(WebScriptSource(WebString::fromUTF8(js)));
}

bool RenderViewTest::ExecuteJavaScriptAndReturnIntValue(
    const base::string16& script,
    int* int_result) {
  v8::HandleScope handle_scope(v8::Isolate::GetCurrent());
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
  FrameLoadWaiter(view_->GetMainRenderFrame()).Wait();
}

PageState RenderViewTest::GetCurrentPageState() {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  return HistoryEntryToPageState(impl->history_controller()->GetCurrentEntry());
}

void RenderViewTest::GoBack(const PageState& state) {
  GoToOffset(-1, state);
}

void RenderViewTest::GoForward(const PageState& state) {
  GoToOffset(1, state);
}

void RenderViewTest::SetUp() {
  content_client_.reset(CreateContentClient());
  content_browser_client_.reset(CreateContentBrowserClient());
  content_renderer_client_.reset(CreateContentRendererClient());
  SetContentClient(content_client_.get());
  SetBrowserClientForTesting(content_browser_client_.get());
  SetRendererClientForTesting(content_renderer_client_.get());

  // Subclasses can set render_thread_ with their own implementation before
  // calling RenderViewTest::SetUp().
  if (!render_thread_)
    render_thread_.reset(new MockRenderThread());
  render_thread_->set_routing_id(kRouteId);
  render_thread_->set_surface_id(kSurfaceId);
  render_thread_->set_new_window_routing_id(kNewWindowRouteId);
  render_thread_->set_new_frame_routing_id(kNewFrameRouteId);

#if defined(OS_MACOSX)
  autorelease_pool_.reset(new base::mac::ScopedNSAutoreleasePool());
#endif
  command_line_.reset(new CommandLine(CommandLine::NO_PROGRAM));
  params_.reset(new MainFunctionParams(*command_line_));
  platform_.reset(new RendererMainPlatformDelegate(*params_));
  platform_->PlatformInitialize();

  // Setting flags and really doing anything with WebKit is fairly fragile and
  // hacky, but this is the world we live in...
  std::string flags("--expose-gc");
  v8::V8::SetFlagsFromString(flags.c_str(), static_cast<int>(flags.size()));
  blink::initialize(webkit_platform_support_.Get());

  // Ensure that we register any necessary schemes when initializing WebKit,
  // since we are using a MockRenderThread.
  RenderThreadImpl::RegisterSchemes();

  // This check is needed because when run under content_browsertests,
  // ResourceBundle isn't initialized (since we have to use a diferent test
  // suite implementation than for content_unittests). For browser_tests, this
  // is already initialized.
  if (!ui::ResourceBundle::HasSharedInstance())
    ui::ResourceBundle::InitSharedInstanceWithLocale(
        "en-US", NULL, ui::ResourceBundle::LOAD_COMMON_RESOURCES);

  mock_process_.reset(new MockRenderProcess);

  // This needs to pass the mock render thread to the view.
  RenderViewImpl* view =
      RenderViewImpl::Create(kOpenerId,
                             false,  // window_was_created_with_opener
                             RendererPreferences(),
                             WebPreferences(),
                             kRouteId,
                             kMainFrameRouteId,
                             kSurfaceId,
                             kInvalidSessionStorageNamespaceId,
                             base::string16(),
                             false,  // is_renderer_created
                             false,  // swapped_out
                             MSG_ROUTING_NONE, // proxy_routing_id
                             false,  // hidden
                             false,  // never_visible
                             1,      // next_page_id
                             blink::WebScreenInfo());
  view->AddRef();
  view_ = view;
}

void RenderViewTest::TearDown() {
  // Try very hard to collect garbage before shutting down.
  // "5" was chosen following http://crbug.com/46571#c9
  const int kGCIterations = 5;
  for (int i = 0; i < kGCIterations; i++)
    GetMainFrame()->collectGarbage();

  // Run the loop so the release task from the renderwidget executes.
  ProcessPendingMessages();

  for (int i = 0; i < kGCIterations; i++)
    GetMainFrame()->collectGarbage();

  render_thread_->SendCloseMessage();
  view_ = NULL;
  mock_process_.reset();

  // After telling the view to close and resetting mock_process_ we may get
  // some new tasks which need to be processed before shutting down WebKit
  // (http://crbug.com/21508).
  base::RunLoop().RunUntilIdle();

#if defined(OS_MACOSX)
  // Needs to run before blink::shutdown().
  autorelease_pool_.reset(NULL);
#endif

  blink::shutdown();

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
    const blink::WebKeyboardEvent& key_event) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->OnMessageReceived(
      InputMsg_HandleInputEvent(0, &key_event, ui::LatencyInfo(), false));
}

void RenderViewTest::SendWebMouseEvent(
    const blink::WebMouseEvent& mouse_event) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->OnMessageReceived(
      InputMsg_HandleInputEvent(0, &mouse_event, ui::LatencyInfo(), false));
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
    "    return [ Math.round(coordinates[0]),"
    "             Math.round(coordinates[1])];"
    "  };"
    "  var elem = document.getElementById('$1');"
    "  if (!elem)"
    "    return null;"
    "  var bounds = GetCoordinates(elem);"
    "  bounds[2] = Math.round(elem.offsetWidth);"
    "  bounds[3] = Math.round(elem.offsetHeight);"
    "  return bounds;"
    "})();";
gfx::Rect RenderViewTest::GetElementBounds(const std::string& element_id) {
  std::vector<std::string> params;
  params.push_back(element_id);
  std::string script =
      ReplaceStringPlaceholders(kGetCoordinatesScript, params, NULL);

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::Handle<v8::Value>  value = GetMainFrame()->executeScriptAndReturnValue(
      WebScriptSource(WebString::fromUTF8(script)));
  if (value.IsEmpty() || !value->IsArray())
    return gfx::Rect();

  v8::Handle<v8::Array> array = value.As<v8::Array>();
  if (array->Length() != 4)
    return gfx::Rect();
  std::vector<int> coords;
  for (int i = 0; i < 4; ++i) {
    v8::Handle<v8::Number> index = v8::Number::New(isolate, i);
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
      new InputMsg_HandleInputEvent(0, &mouse_event, ui::LatencyInfo(), false));
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->OnMessageReceived(*input_message);
  return true;
}

void RenderViewTest::SetFocused(const blink::WebNode& node) {
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
  FrameMsg_Navigate_Params params;
  params.url = url;
  params.navigation_type = FrameMsg_Navigate_Type::RELOAD;
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->main_render_frame()->OnNavigate(params);
  FrameLoadWaiter(impl->main_render_frame()).Wait();
}

uint32 RenderViewTest::GetNavigationIPCType() {
  return FrameHostMsg_DidCommitProvisionalLoad::ID;
}

void RenderViewTest::Resize(gfx::Size new_size,
                            gfx::Rect resizer_rect,
                            bool is_fullscreen) {
  ViewMsg_Resize_Params params;
  params.screen_info = blink::WebScreenInfo();
  params.new_size = new_size;
  params.physical_backing_size = new_size;
  params.top_controls_layout_height = 0.f;
  params.resizer_rect = resizer_rect;
  params.is_fullscreen = is_fullscreen;
  scoped_ptr<IPC::Message> resize_message(new ViewMsg_Resize(0, params));
  OnMessageReceived(*resize_message);
}

bool RenderViewTest::OnMessageReceived(const IPC::Message& msg) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  return impl->OnMessageReceived(msg);
}

void RenderViewTest::DidNavigateWithinPage(blink::WebLocalFrame* frame,
                                           bool is_new_navigation) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  blink::WebHistoryItem item;
  item.initialize();
  impl->main_render_frame()->didNavigateWithinPage(
      frame,
      item,
      is_new_navigation ? blink::WebStandardCommit
                        : blink::WebHistoryInertCommit);
}

void RenderViewTest::SendContentStateImmediately() {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  impl->set_send_content_state_immediately(true);
}

blink::WebWidget* RenderViewTest::GetWebWidget() {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);
  return impl->webwidget();
}


ContentClient* RenderViewTest::CreateContentClient() {
  return new ContentClient;
}

ContentBrowserClient* RenderViewTest::CreateContentBrowserClient() {
  return new ContentBrowserClient;
}

ContentRendererClient* RenderViewTest::CreateContentRendererClient() {
  return new ContentRendererClient;
}

void RenderViewTest::GoToOffset(int offset, const PageState& state) {
  RenderViewImpl* impl = static_cast<RenderViewImpl*>(view_);

  int history_list_length = impl->historyBackListCount() +
                            impl->historyForwardListCount() + 1;
  int pending_offset = offset + impl->history_list_offset();

  FrameMsg_Navigate_Params navigate_params;
  navigate_params.navigation_type = FrameMsg_Navigate_Type::NORMAL;
  navigate_params.transition = PAGE_TRANSITION_FORWARD_BACK;
  navigate_params.current_history_list_length = history_list_length;
  navigate_params.current_history_list_offset = impl->history_list_offset();
  navigate_params.pending_history_list_offset = pending_offset;
  navigate_params.page_id = impl->page_id_ + offset;
  navigate_params.page_state = state;
  navigate_params.request_time = base::Time::Now();

  FrameMsg_Navigate navigate_message(impl->main_render_frame()->GetRoutingID(),
                                     navigate_params);
  impl->main_render_frame()->OnMessageReceived(navigate_message);

  // The load actually happens asynchronously, so we pump messages to process
  // the pending continuation.
  FrameLoadWaiter(view_->GetMainRenderFrame()).Wait();
}

}  // namespace content
