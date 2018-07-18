// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_WEB_VIEW_TEST_PROXY_H_
#define CONTENT_SHELL_TEST_RUNNER_WEB_VIEW_TEST_PROXY_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "content/shell/test_runner/web_view_test_client.h"
#include "content/shell/test_runner/web_widget_test_client.h"
#include "content/shell/test_runner/web_widget_test_proxy.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_screen_info.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_text_direction.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_widget_client.h"

namespace blink {
class WebDragData;
class WebLocalFrame;
class WebString;
class WebView;
class WebWidget;
struct WebPoint;
struct WebWindowFeatures;
}

namespace test_runner {

class AccessibilityController;
class TestInterfaces;
class TestRunnerForSpecificView;
class TextInputController;
class WebTestDelegate;
class WebTestInterfaces;

// A WebWidgetClient that manages calls to a client for testing, as well as a
// real client in the WebViewTestProxy base class.
// The |base_class_widget_client| is a real production WebWidgetClient, while
// the |test_widget_client| is a test-only WebWidgetClient. Mostly calls are
// forwarded to the |base_class_widget_client|, but some are intercepted and
// sent to the |test_widget_client| instead, or to both.
class TEST_RUNNER_EXPORT ProxyWebWidgetClient : public blink::WebWidgetClient {
 public:
  ProxyWebWidgetClient(blink::WebWidgetClient* base_class_widget_client,
                       blink::WebWidgetClient* widget_test_client);

  // blink::WebWidgetClient implementation.
  void DidInvalidateRect(const blink::WebRect&) override;
  blink::WebLayerTreeView* InitializeLayerTreeView() override;
  bool AllowsBrokenNullLayerTreeView() const override;
  void ScheduleAnimation() override;
  void IntrinsicSizingInfoChanged(
      const blink::WebIntrinsicSizingInfo&) override;
  void DidMeaningfulLayout(blink::WebMeaningfulLayout) override;
  void DidFirstLayoutAfterFinishedParsing() override;
  void DidChangeCursor(const blink::WebCursorInfo&) override;
  void AutoscrollStart(const blink::WebFloatPoint&) override;
  void AutoscrollFling(const blink::WebFloatSize& velocity) override;
  void AutoscrollEnd() override;
  void CloseWidgetSoon() override;
  void Show(blink::WebNavigationPolicy) override;
  blink::WebRect WindowRect() override;
  void SetWindowRect(const blink::WebRect&) override;
  blink::WebRect ViewRect() override;
  void SetToolTipText(const blink::WebString&,
                      blink::WebTextDirection hint) override;
  blink::WebScreenInfo GetScreenInfo() override;
  bool RequestPointerLock() override;
  void RequestPointerUnlock() override;
  bool IsPointerLocked() override;
  void DidHandleGestureEvent(const blink::WebGestureEvent& event,
                             bool event_cancelled) override;
  void DidOverscroll(const blink::WebFloatSize& overscroll_delta,
                     const blink::WebFloatSize& accumulated_overscroll,
                     const blink::WebFloatPoint& position_in_viewport,
                     const blink::WebFloatSize& velocity_in_viewport,
                     const cc::OverscrollBehavior& behavior) override;
  void HasTouchEventHandlers(bool) override;
  void SetNeedsLowLatencyInput(bool) override;
  void RequestUnbufferedInputEvents() override;
  void SetTouchAction(blink::WebTouchAction touch_action) override;
  void ShowVirtualKeyboardOnElementFocus() override;
  void ConvertViewportToWindow(blink::WebRect* rect) override;
  void ConvertWindowToViewport(blink::WebFloatRect* rect) override;
  void StartDragging(blink::WebReferrerPolicy,
                     const blink::WebDragData&,
                     blink::WebDragOperationsMask,
                     const SkBitmap& drag_image,
                     const blink::WebPoint& drag_image_offset) override;

 private:
  blink::WebWidgetClient* base_class_widget_client_;
  blink::WebWidgetClient* widget_test_client_;
};

// WebViewTestProxyBase is the "brain" of WebViewTestProxy in the sense that
// WebViewTestProxy does the bridge between RenderViewImpl and
// WebViewTestProxyBase and when it requires a behavior to be different from the
// usual, it will call WebViewTestProxyBase that implements the expected
// behavior. See WebViewTestProxy class comments for more information.
//
// Uses private inheritence to ensure that code doesn't assume WebViews are
// WebWidgets, and gets to the WebWidget through an accessor instead.
class TEST_RUNNER_EXPORT WebViewTestProxyBase : private WebWidgetTestProxyBase {
 public:
  blink::WebView* web_view() { return web_view_; }
  void set_web_view(blink::WebView* view) {
    DCHECK(view);
    DCHECK(!web_view_);
    web_view_ = view;
  }

  void set_view_test_client(
      std::unique_ptr<WebViewTestClient> view_test_client) {
    DCHECK(view_test_client);
    DCHECK(!view_test_client_);
    view_test_client_ = std::move(view_test_client);
  }

  // To be called once the WebWidgetTestClient has been set up, to build the
  // indirection used by this class.
  void SetUpWidgetClient() {
    DCHECK(widget_test_client());
    proxy_widget_client_ = std::make_unique<ProxyWebWidgetClient>(
        base_class_widget_client_, widget_test_client());
  }

  WebTestDelegate* delegate() { return delegate_; }
  void set_delegate(WebTestDelegate* delegate) {
    DCHECK(delegate);
    DCHECK(!delegate_);
    delegate_ = delegate;
  }

  TestInterfaces* test_interfaces() { return test_interfaces_; }
  void SetInterfaces(WebTestInterfaces* web_test_interfaces);

  AccessibilityController* accessibility_controller() {
    return accessibility_controller_.get();
  }

  TestRunnerForSpecificView* view_test_runner() {
    return view_test_runner_.get();
  }

  void Reset();
  void BindTo(blink::WebLocalFrame* frame);

  void GetScreenOrientationForTesting(blink::WebScreenInfo&);

  WebWidgetTestProxyBase* web_widget_test_proxy_base() { return this; }

 protected:
  explicit WebViewTestProxyBase(
      blink::WebWidgetClient* base_class_widget_client);
  ~WebViewTestProxyBase();

  blink::WebViewClient* view_test_client() { return view_test_client_.get(); }
  // Wraps the widget_test_client() and the base class' WidgetClient().
  blink::WebWidgetClient* proxy_widget_client() {
    return proxy_widget_client_.get();
  }

 private:
  // Hide widget_test_client(), the proxy_widget_client() should be used
  // instead. Which decides to redirect some calls to the widget_test_client()
  // as needed.
  using WebWidgetTestProxyBase::widget_test_client;

  TestInterfaces* test_interfaces_;
  WebTestDelegate* delegate_;
  blink::WebView* web_view_;
  blink::WebWidget* web_widget_;
  blink::WebWidgetClient* base_class_widget_client_;
  std::unique_ptr<ProxyWebWidgetClient> proxy_widget_client_;
  std::unique_ptr<WebViewTestClient> view_test_client_;
  std::unique_ptr<AccessibilityController> accessibility_controller_;
  std::unique_ptr<TextInputController> text_input_controller_;
  std::unique_ptr<TestRunnerForSpecificView> view_test_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebViewTestProxyBase);
};

// WebViewTestProxy is used during LayoutTests and always instantiated, at time
// of writing with Base=RenderViewImpl. It does not directly inherit from it for
// layering purposes.
// The intent of that class is to wrap RenderViewImpl for tests purposes in
// order to reduce the amount of test specific code in the production code.
// WebViewTestProxy is only doing the glue between RenderViewImpl and
// WebViewTestProxyBase, that means that there is no logic living in this class
// except deciding which base class should be called (could be both).
//
// Examples of usage:
//  * when a fooClient has a mock implementation, WebViewTestProxy can override
//    the fooClient() call and have WebViewTestProxyBase return the mock
//    implementation.
//  * when a value needs to be overridden by LayoutTests, WebViewTestProxy can
//    override RenderViewImpl's getter and call a getter from
//    WebViewTestProxyBase instead. In addition, WebViewTestProxyBase will have
//    a public setter that could be called from the TestRunner.
template <class Base>
class WebViewTestProxy : public Base, public WebViewTestProxyBase {
 public:
  template <typename... Args>
  explicit WebViewTestProxy(Args&&... args)
      : Base(std::forward<Args>(args)...),
        WebViewTestProxyBase(Base::WidgetClient()) {}

  // WebViewClient implementation.
  blink::WebView* CreateView(blink::WebLocalFrame* creator,
                             const blink::WebURLRequest& request,
                             const blink::WebWindowFeatures& features,
                             const blink::WebString& frame_name,
                             blink::WebNavigationPolicy policy,
                             bool suppress_opener,
                             blink::WebSandboxFlags sandbox_flags) override {
    if (!view_test_client()->CreateView(creator, request, features, frame_name,
                                        policy, suppress_opener, sandbox_flags))
      return nullptr;
    return Base::CreateView(creator, request, features, frame_name, policy,
                            suppress_opener, sandbox_flags);
  }
  void PrintPage(blink::WebLocalFrame* frame) override {
    view_test_client()->PrintPage(frame);
  }
  blink::WebString AcceptLanguages() override {
    return view_test_client()->AcceptLanguages();
  }
  void DidFocus(blink::WebLocalFrame* calling_frame) override {
    view_test_client()->DidFocus(calling_frame);
    Base::DidFocus(calling_frame);
  }
  blink::WebWidgetClient* WidgetClient() override {
    return proxy_widget_client();
  }

 private:
  virtual ~WebViewTestProxy() {}

  DISALLOW_COPY_AND_ASSIGN(WebViewTestProxy);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_WEB_VIEW_TEST_PROXY_H_
