// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_per_process_browsertest.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/pattern.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "cc/input/touch_action.h"
#include "content/browser/frame_host/cross_process_frame_connector.h"
#include "content/browser/frame_host/frame_navigation_entry.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/frame_host/render_frame_proxy_host.h"
#include "content/browser/frame_host/render_widget_host_view_child_frame.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/renderer_host/input/input_router_impl.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/child_process_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/common/input_messages.h"
#include "content/common/renderer.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/interstitial_page_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/navigation_handle_observer.h"
#include "content/public/test/test_frame_navigation_observer.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_security_test_util.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebFeaturePolicy.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebInsecureRequestPolicy.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "ui/display/display_switches.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/geometry/point.h"
#include "ui/latency/latency_info.h"
#include "ui/native_theme/native_theme_features.h"

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#endif

#if defined(OS_MACOSX)
#include "ui/base/test/scoped_preferred_scroller_style_mac.h"
#endif

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/android/ime_adapter_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#endif

using ::testing::SizeIs;

namespace content {

namespace {

// Helper function to send a postMessage and wait for a reply message.  The
// |post_message_script| is executed on the |sender_ftn| frame, and the sender
// frame is expected to post |reply_status| from the DOMAutomationController
// when it receives a reply.
void PostMessageAndWaitForReply(FrameTreeNode* sender_ftn,
                                const std::string& post_message_script,
                                const std::string& reply_status) {
  // Subtle: msg_queue needs to be declared before the ExecuteScript below, or
  // else it might miss the message of interest.  See https://crbug.com/518729.
  DOMMessageQueue msg_queue;

  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      sender_ftn,
      "window.domAutomationController.send(" + post_message_script + ");",
      &success));
  EXPECT_TRUE(success);

  std::string status;
  while (msg_queue.WaitForMessage(&status)) {
    if (status == reply_status)
      break;
  }
}

// Helper function to extract and return "window.receivedMessages" from the
// |sender_ftn| frame.  This variable is used in post_message.html to count the
// number of messages received via postMessage by the current window.
int GetReceivedMessages(FrameTreeNode* ftn) {
  int received_messages = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      ftn, "window.domAutomationController.send(window.receivedMessages);",
      &received_messages));
  return received_messages;
}

// Helper function to perform a window.open from the |caller_frame| targeting a
// frame with the specified name.
void NavigateNamedFrame(const ToRenderFrameHost& caller_frame,
                        const GURL& url,
                        const std::string& name) {
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      caller_frame,
      "window.domAutomationController.send("
      "    !!window.open('" + url.spec() + "', '" + name + "'));",
      &success));
  EXPECT_TRUE(success);
}

// Helper function to generate a click on the given RenderWidgetHost.  The
// mouse event is forwarded directly to the RenderWidgetHost without any
// hit-testing.
void SimulateMouseClick(RenderWidgetHost* rwh, int x, int y) {
  blink::WebMouseEvent mouse_event(blink::WebInputEvent::kMouseDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.SetPositionInWidget(x, y);
  rwh->ForwardMouseEvent(mouse_event);
}

// Retrieve document.origin for the frame |ftn|.
std::string GetDocumentOrigin(FrameTreeNode* ftn) {
  std::string origin;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      ftn, "domAutomationController.send(document.origin)", &origin));
  return origin;
}

class RenderWidgetHostMouseEventMonitor {
 public:
  explicit RenderWidgetHostMouseEventMonitor(RenderWidgetHost* host)
      : host_(host), event_received_(false) {
    host_->AddMouseEventCallback(
        base::Bind(&RenderWidgetHostMouseEventMonitor::MouseEventCallback,
                   base::Unretained(this)));
  }
  ~RenderWidgetHostMouseEventMonitor() {
    host_->RemoveMouseEventCallback(
        base::Bind(&RenderWidgetHostMouseEventMonitor::MouseEventCallback,
                   base::Unretained(this)));
  }
  bool EventWasReceived() const { return event_received_; }
  void ResetEventReceived() { event_received_ = false; }
  const blink::WebMouseEvent& event() const { return event_; }

 private:
  bool MouseEventCallback(const blink::WebMouseEvent& event) {
    event_received_ = true;
    event_ = event;
    return false;
  }
  RenderWidgetHost* host_;
  bool event_received_;
  blink::WebMouseEvent event_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostMouseEventMonitor);
};

class TestInputEventObserver : public RenderWidgetHost::InputEventObserver {
 public:
  explicit TestInputEventObserver(RenderWidgetHost* host) : host_(host) {
    host_->AddInputEventObserver(this);
  }

  ~TestInputEventObserver() override { host_->RemoveInputEventObserver(this); }

  bool EventWasReceived() const { return !events_received_.empty(); }
  void ResetEventsReceived() { events_received_.clear(); }
  blink::WebInputEvent::Type EventType() const {
    DCHECK(EventWasReceived());
    return events_received_.front();
  }
  const std::vector<blink::WebInputEvent::Type>& events_received() {
    return events_received_;
  }

  void OnInputEvent(const blink::WebInputEvent& event) override {
    events_received_.push_back(event.GetType());
  };

 private:
  RenderWidgetHost* host_;
  std::vector<blink::WebInputEvent::Type> events_received_;

  DISALLOW_COPY_AND_ASSIGN(TestInputEventObserver);
};

double GetFrameDeviceScaleFactor(const ToRenderFrameHost& adapter) {
  double device_scale_factor;
  const char kGetFrameDeviceScaleFactor[] =
      "window.domAutomationController.send(window.devicePixelRatio);";
  EXPECT_TRUE(ExecuteScriptAndExtractDouble(adapter, kGetFrameDeviceScaleFactor,
                                            &device_scale_factor));
  return device_scale_factor;
}

// This helper accounts for Android devices which use page scale factor
// different from 1.0. Coordinate targeting needs to be adjusted before
// hit testing.
double GetPageScaleFactor(Shell* shell) {
  return RenderWidgetHostImpl::From(
             shell->web_contents()->GetRenderViewHost()->GetWidget())
      ->last_frame_metadata()
      .page_scale_factor;
}

// Helper function that performs a surface hittest.
void SurfaceHitTestTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell->web_contents())
          ->GetInputEventRouter();

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForChildFrameSurfaceReady(child_node->current_frame_host());

  float scale_factor = GetPageScaleFactor(shell);

  // Get the view bounds of the child iframe, which should account for the
  // relative offset of its direct parent within the root frame, for use in
  // targeting the input event.
  gfx::Rect bounds = rwhv_child->GetViewBounds();

  // Target input event to child frame.
  blink::WebMouseEvent child_event(blink::WebInputEvent::kMouseDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  child_event.button = blink::WebPointerProperties::Button::kLeft;
  child_event.SetPositionInWidget(
      gfx::ToCeiledInt((bounds.x() - root_view->GetViewBounds().x() + 5) *
                       scale_factor),
      gfx::ToCeiledInt((bounds.y() - root_view->GetViewBounds().y() + 5) *
                       scale_factor));
  child_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  router->RouteMouseEvent(root_view, &child_event, ui::LatencyInfo());

  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  // The expected result coordinates are (5, 5), but can get slightly
  // different results due to rounding error with some page scale factors.
  EXPECT_NEAR(5, child_frame_monitor.event().PositionInWidget().x, 2);
  EXPECT_NEAR(5, child_frame_monitor.event().PositionInWidget().y, 2);
  EXPECT_FALSE(main_frame_monitor.EventWasReceived());

  child_frame_monitor.ResetEventReceived();
  main_frame_monitor.ResetEventReceived();

  // Target input event to main frame.
  blink::WebMouseEvent main_event(blink::WebInputEvent::kMouseDown,
                                  blink::WebInputEvent::kNoModifiers,
                                  blink::WebInputEvent::kTimeStampForTesting);
  main_event.button = blink::WebPointerProperties::Button::kLeft;
  main_event.SetPositionInWidget(2, 2);
  main_event.click_count = 1;
  // Ladies and gentlemen, THIS is the main_event!
  router->RouteMouseEvent(root_view, &main_event, ui::LatencyInfo());

  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_NEAR(2, main_frame_monitor.event().PositionInWidget().x, 2);
  EXPECT_NEAR(2, main_frame_monitor.event().PositionInWidget().y, 2);
}

class RedirectNotificationObserver : public NotificationObserver {
 public:
  // Register to listen for notifications of the given type from either a
  // specific source, or from all sources if |source| is
  // NotificationService::AllSources().
  RedirectNotificationObserver(int notification_type,
                               const NotificationSource& source);
  ~RedirectNotificationObserver() override;

  // Wait until the specified notification occurs.  If the notification was
  // emitted between the construction of this object and this call then it
  // returns immediately.
  void Wait();

  // Returns NotificationService::AllSources() if we haven't observed a
  // notification yet.
  const NotificationSource& source() const {
    return source_;
  }

  const NotificationDetails& details() const {
    return details_;
  }

  // NotificationObserver:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override;

 private:
  bool seen_;
  bool seen_twice_;
  bool running_;
  NotificationRegistrar registrar_;

  NotificationSource source_;
  NotificationDetails details_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(RedirectNotificationObserver);
};

RedirectNotificationObserver::RedirectNotificationObserver(
    int notification_type,
    const NotificationSource& source)
    : seen_(false),
      running_(false),
      source_(NotificationService::AllSources()) {
  registrar_.Add(this, notification_type, source);
}

RedirectNotificationObserver::~RedirectNotificationObserver() {}

void RedirectNotificationObserver::Wait() {
  if (seen_ && seen_twice_)
    return;

  running_ = true;
  message_loop_runner_ = new MessageLoopRunner;
  message_loop_runner_->Run();
  EXPECT_TRUE(seen_);
}

void RedirectNotificationObserver::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  source_ = source;
  details_ = details;
  seen_twice_ = seen_;
  seen_ = true;
  if (!running_)
    return;

  message_loop_runner_->Quit();
  running_ = false;
}

// This observer keeps track of the number of created RenderFrameHosts.  Tests
// can use this to ensure that a certain number of child frames has been
// created after navigating.
class RenderFrameHostCreatedObserver : public WebContentsObserver {
 public:
  RenderFrameHostCreatedObserver(WebContents* web_contents,
                                 int expected_frame_count)
      : WebContentsObserver(web_contents),
        expected_frame_count_(expected_frame_count),
        frames_created_(0),
        message_loop_runner_(new MessageLoopRunner) {}

  ~RenderFrameHostCreatedObserver() override;

  // Runs a nested run loop and blocks until the expected number of
  // RenderFrameHosts is created.
  void Wait();

 private:
  // WebContentsObserver
  void RenderFrameCreated(RenderFrameHost* render_frame_host) override;

  // The number of RenderFrameHosts to wait for.
  int expected_frame_count_;

  // The number of RenderFrameHosts that have been created.
  int frames_created_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(RenderFrameHostCreatedObserver);
};

RenderFrameHostCreatedObserver::~RenderFrameHostCreatedObserver() {
}

void RenderFrameHostCreatedObserver::Wait() {
  message_loop_runner_->Run();
}

void RenderFrameHostCreatedObserver::RenderFrameCreated(
    RenderFrameHost* render_frame_host) {
  frames_created_++;
  if (frames_created_ == expected_frame_count_) {
    message_loop_runner_->Quit();
  }
}

// This observer detects when WebContents receives notification of a user
// gesture having occurred, following a user input event targeted to
// a RenderWidgetHost under that WebContents.
class UserInteractionObserver : public WebContentsObserver {
 public:
  explicit UserInteractionObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents), user_interaction_received_(false) {}

  ~UserInteractionObserver() override {}

  // Retrieve the flag. There is no need to wait on a loop since
  // DidGetUserInteraction() should be called synchronously with the input
  // event processing in the browser process.
  bool WasUserInteractionReceived() { return user_interaction_received_; }

  void Reset() { user_interaction_received_ = false; }

 private:
  // WebContentsObserver
  void DidGetUserInteraction(const blink::WebInputEvent::Type type) override {
    user_interaction_received_ = true;
  }

  bool user_interaction_received_;

  DISALLOW_COPY_AND_ASSIGN(UserInteractionObserver);
};

// This observer is used to wait for its owner FrameTreeNode to become deleted.
class FrameDeletedObserver : public FrameTreeNode::Observer {
 public:
  FrameDeletedObserver(FrameTreeNode* owner)
      : owner_(owner), message_loop_runner_(new MessageLoopRunner) {
    owner->AddObserver(this);
  }

  void Wait() { message_loop_runner_->Run(); }

 private:
  // FrameTreeNode::Observer
  void OnFrameTreeNodeDestroyed(FrameTreeNode* node) override {
    if (node == owner_)
      message_loop_runner_->Quit();
  }

  FrameTreeNode* owner_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(FrameDeletedObserver);
};

// Helper function to focus a frame by sending it a mouse click and then
// waiting for it to become focused.
void FocusFrame(FrameTreeNode* frame) {
  FrameFocusedObserver focus_observer(frame->current_frame_host());
  SimulateMouseClick(frame->current_frame_host()->GetRenderWidgetHost(), 1, 1);
  focus_observer.Wait();
}

// A BrowserMessageFilter that drops SwapOut ACK messages.
class SwapoutACKMessageFilter : public BrowserMessageFilter {
 public:
  SwapoutACKMessageFilter() : BrowserMessageFilter(FrameMsgStart) {}

 protected:
  ~SwapoutACKMessageFilter() override {}

 private:
  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    return message.type() == FrameHostMsg_SwapOut_ACK::ID;
  }

  DISALLOW_COPY_AND_ASSIGN(SwapoutACKMessageFilter);
};

class RenderWidgetHostVisibilityObserver : public NotificationObserver {
 public:
  explicit RenderWidgetHostVisibilityObserver(RenderWidgetHostImpl* rwhi,
                                              bool expected_visibility_state)
      : expected_visibility_state_(expected_visibility_state),
        was_observed_(false),
        did_fail_(false),
        source_(rwhi) {
    registrar_.Add(this, NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                   source_);
    message_loop_runner_ = new MessageLoopRunner;
  }

  bool WaitUntilSatisfied() {
    if (!was_observed_)
      message_loop_runner_->Run();
    registrar_.Remove(this, NOTIFICATION_RENDER_WIDGET_VISIBILITY_CHANGED,
                      source_);
    return !did_fail_;
  }

 private:
  void Observe(int type,
               const NotificationSource& source,
               const NotificationDetails& details) override {
    was_observed_ = true;
    did_fail_ = expected_visibility_state_ !=
                (*static_cast<const Details<bool>&>(details).ptr());
    if (message_loop_runner_->loop_running())
      message_loop_runner_->Quit();
  }

  bool expected_visibility_state_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;
  NotificationRegistrar registrar_;
  bool was_observed_;
  bool did_fail_;
  Source<RenderWidgetHost> source_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostVisibilityObserver);
};

class TestInterstitialDelegate : public InterstitialPageDelegate {
 private:
  // InterstitialPageDelegate:
  std::string GetHTMLContents() override { return "<p>Interstitial</p>"; }
};

}  // namespace

//
// SitePerProcessBrowserTest
//

SitePerProcessBrowserTest::SitePerProcessBrowserTest() {
};

std::string SitePerProcessBrowserTest::DepictFrameTree(FrameTreeNode* node) {
  return visualizer_.DepictFrameTree(node);
}

void SitePerProcessBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  IsolateAllSitesForTesting(command_line);
#if !defined(OS_ANDROID)
  // TODO(bokan): Needed for scrollability check in
  // FrameOwnerPropertiesPropagationScrolling. crbug.com/662196.
  feature_list_.InitAndDisableFeature(features::kOverlayScrollbar);
#endif
}

void SitePerProcessBrowserTest::SetUpOnMainThread() {
  host_resolver()->AddRule("*", "127.0.0.1");
  SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());
}

//
// SitePerProcessHighDPIBrowserTest
//


class SitePerProcessHighDPIBrowserTest : public SitePerProcessBrowserTest {
 public:
  const double kDeviceScaleFactor = 2.0;

  SitePerProcessHighDPIBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }
};

//
// SitePerProcessNonIntegerScaleFactorBrowserTest
//

class SitePerProcessNonIntegerScaleFactorBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  const double kDeviceScaleFactor = 1.5;

  SitePerProcessNonIntegerScaleFactorBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kForceDeviceScaleFactor,
        base::StringPrintf("%f", kDeviceScaleFactor));
  }
};

// SitePerProcessIgnoreCertErrorsBrowserTest

class SitePerProcessIgnoreCertErrorsBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessIgnoreCertErrorsBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }
};

// SitePerProcessEmbedderCSPEnforcementBrowserTest

class SitePerProcessEmbedderCSPEnforcementBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessEmbedderCSPEnforcementBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTest::SetUpCommandLine(command_line);
    // TODO(amalika): Remove this switch when the EmbedderCSPEnforcement becomes
    // stable
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "EmbedderCSPEnforcement");
  }
};

// SitePerProcessFeaturePolicyBrowserTest

class SitePerProcessFeaturePolicyBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  SitePerProcessFeaturePolicyBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SitePerProcessBrowserTest::SetUpCommandLine(command_line);
    // TODO(iclelland): Remove this switch when Feature Policy ships.
    // https://crbug.com/623682
    command_line->AppendSwitchASCII(
        switches::kEnableBlinkFeatures,
        "FeaturePolicy,FeaturePolicyExperimentalFeatures");
  }

  ParsedFeaturePolicyHeader CreateFPHeader(
      blink::WebFeaturePolicyFeature feature,
      const std::vector<GURL>& origins) {
    ParsedFeaturePolicyHeader result(1);
    result[0].feature = feature;
    result[0].matches_all_origins = false;
    DCHECK(!origins.empty());
    for (const GURL& origin : origins)
      result[0].origins.push_back(url::Origin(origin));
    return result;
  }

  ParsedFeaturePolicyHeader CreateFPHeaderMatchesAll(
      blink::WebFeaturePolicyFeature feature) {
    ParsedFeaturePolicyHeader result(1);
    result[0].feature = feature;
    result[0].matches_all_origins = true;
    return result;
  }
};

IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIBrowserTest,
                       SubframeLoadsWithCorrectDeviceScaleFactor) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // On Android forcing device scale factor does not work for tests, therefore
  // we ensure that make frame and iframe have the same DIP scale there, but
  // not necessarily kDeviceScaleFactor.
  const double expected_dip_scale =
#if defined(OS_ANDROID)
      GetFrameDeviceScaleFactor(web_contents());
#else
      SitePerProcessHighDPIBrowserTest::kDeviceScaleFactor;
#endif

  EXPECT_EQ(expected_dip_scale, GetFrameDeviceScaleFactor(web_contents()));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(expected_dip_scale, GetFrameDeviceScaleFactor(child));
}

// Ensure that navigating subframes in --site-per-process mode works and the
// correct documents are committed.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrossSiteIframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  {
    // There should be only one RenderWidgetHost when there are no
    // cross-process iframes.
    std::set<RenderWidgetHostView*> views_set =
        web_contents()->GetRenderWidgetHostViewsInTree();
    EXPECT_EQ(1U, views_set.size());
  }

  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        |--Site A\n"
      "        +--Site A\n"
      "             +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  // Load cross-site page into iframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateFrameToURL(root->child_at(0), url);
    deleted_observer.WaitUntilDeleted();
  }
  // Verify that the navigation succeeded and the expected URL was loaded.
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Ensure that we have created a new process for the subframe.
  ASSERT_EQ(2U, root->child_count());
  SiteInstance* site_instance = child->current_frame_host()->GetSiteInstance();
  RenderViewHost* rvh = child->current_frame_host()->render_view_host();
  RenderProcessHost* rph = child->current_frame_host()->GetProcess();
  EXPECT_NE(shell()->web_contents()->GetRenderViewHost(), rvh);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);
  EXPECT_NE(shell()->web_contents()->GetRenderProcessHost(), rph);
  {
    // There should be now two RenderWidgetHosts, one for each process
    // rendering a frame.
    std::set<RenderWidgetHostView*> views_set =
        web_contents()->GetRenderWidgetHostViewsInTree();
    EXPECT_EQ(2U, views_set.size());
  }
  RenderFrameProxyHost* proxy_to_parent =
      child->render_manager()->GetProxyToParent();
  EXPECT_TRUE(proxy_to_parent);
  EXPECT_TRUE(proxy_to_parent->cross_process_frame_connector());
  // The out-of-process iframe should have its own RenderWidgetHost,
  // independent of any RenderViewHost.
  EXPECT_NE(
      rvh->GetWidget()->GetView(),
      proxy_to_parent->cross_process_frame_connector()->get_view_for_testing());
  EXPECT_TRUE(child->current_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        |--Site A -- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "             +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Load another cross-site page into the same iframe.
  url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateFrameToURL(root->child_at(0), url);
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Check again that a new process is created and is different from the
  // top level one and the previous one.
  ASSERT_EQ(2U, root->child_count());
  child = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetRenderViewHost(),
            child->current_frame_host()->render_view_host());
  EXPECT_NE(rvh, child->current_frame_host()->render_view_host());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(site_instance,
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(shell()->web_contents()->GetRenderProcessHost(),
            child->current_frame_host()->GetProcess());
  EXPECT_NE(rph, child->current_frame_host()->GetProcess());
  {
    std::set<RenderWidgetHostView*> views_set =
        web_contents()->GetRenderWidgetHostViewsInTree();
    EXPECT_EQ(2U, views_set.size());
  }
  EXPECT_EQ(proxy_to_parent, child->render_manager()->GetProxyToParent());
  EXPECT_TRUE(proxy_to_parent->cross_process_frame_connector());
  EXPECT_NE(
      child->current_frame_host()->render_view_host()->GetWidget()->GetView(),
      proxy_to_parent->cross_process_frame_connector()->get_view_for_testing());
  EXPECT_TRUE(child->current_frame_host()->GetRenderWidgetHost());

  EXPECT_EQ(
      " Site A ------------ proxies for C\n"
      "   |--Site C ------- proxies for A\n"
      "   +--Site A ------- proxies for C\n"
      "        |--Site A -- proxies for C\n"
      "        +--Site A -- proxies for C\n"
      "             +--Site A -- proxies for C\n"
      "Where A = http://a.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));
}

// Ensure that title updates affect the correct NavigationEntry after a new
// subframe navigation with an out-of-process iframe.  https://crbug.com/616609.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, TitleAfterCrossSiteIframe) {
  // Start at an initial page.
  GURL initial_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  // Navigate to a same-site page with a same-site iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Make the main frame update its title after the subframe loads.
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(),
                            "document.querySelector('iframe').onload = "
                            "    function() { document.title = 'loaded'; };"));
  EXPECT_TRUE(
      ExecuteScript(shell()->web_contents(), "document.title = 'not loaded';"));
  base::string16 expected_title(base::UTF8ToUTF16("loaded"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Navigate the iframe cross-site.
  TestNavigationObserver load_observer(shell()->web_contents());
  GURL frame_url = embedded_test_server()->GetURL("b.com", "/title2.html");
  EXPECT_TRUE(
      ExecuteScript(root->child_at(0)->current_frame_host(),
                    "window.location.href = '" + frame_url.spec() + "';"));
  load_observer.Wait();

  // Wait for the title to update and ensure it affects the right NavEntry.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  EXPECT_EQ(expected_title, entry->GetTitle());
}

// Class to detect incoming GestureScrollEnd acks for bubbling tests.
class InputEventAckWaiter
    : public content::RenderWidgetHost::InputEventObserver {
 public:
  InputEventAckWaiter(blink::WebInputEvent::Type ack_type_waiting_for)
      : message_loop_runner_(new content::MessageLoopRunner),
        ack_type_waiting_for_(ack_type_waiting_for),
        desired_ack_type_received_(false) {}
  ~InputEventAckWaiter() override {}

  void OnInputEventAck(const blink::WebInputEvent& event) override {
    if (event.GetType() == ack_type_waiting_for_) {
      desired_ack_type_received_ = true;
      if (message_loop_runner_->loop_running())
        message_loop_runner_->Quit();
    }
  }

  void Wait() {
    if (!desired_ack_type_received_) {
      message_loop_runner_->Run();
    }
  }

  void Reset() {
    desired_ack_type_received_ = false;
    message_loop_runner_ = new content::MessageLoopRunner;
  }

 private:
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  blink::WebInputEvent::Type ack_type_waiting_for_;
  bool desired_ack_type_received_;

  DISALLOW_COPY_AND_ASSIGN(InputEventAckWaiter);
};

// Class to sniff incoming IPCs for FrameHostMsg_FrameRectChanged messages.
class FrameRectChangedMessageFilter : public content::BrowserMessageFilter {
 public:
  FrameRectChangedMessageFilter()
      : content::BrowserMessageFilter(FrameMsgStart),
        message_loop_runner_(new content::MessageLoopRunner),
        frame_rect_received_(false) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(FrameRectChangedMessageFilter, message)
      IPC_MESSAGE_HANDLER(FrameHostMsg_FrameRectChanged, OnFrameRectChanged)
    IPC_END_MESSAGE_MAP()
    return false;
  }

  gfx::Rect last_rect() const { return last_rect_; }

  void Wait() {
    message_loop_runner_->Run();
  }

  void Reset() {
    last_rect_ = gfx::Rect();
    message_loop_runner_ = new content::MessageLoopRunner;
    frame_rect_received_ = false;
  }

 private:
  ~FrameRectChangedMessageFilter() override {}

  void OnFrameRectChanged(const gfx::Rect& rect) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&FrameRectChangedMessageFilter::OnFrameRectChangedOnUI, this,
                   rect));
  }

  void OnFrameRectChangedOnUI(const gfx::Rect& rect) {
    last_rect_ = rect;
    if (!frame_rect_received_) {
      frame_rect_received_ = true;
      message_loop_runner_->Quit();
    }
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  bool frame_rect_received_;
  gfx::Rect last_rect_;

  DISALLOW_COPY_AND_ASSIGN(FrameRectChangedMessageFilter);
};

// Test that the view bounds for an out-of-process iframe are set and updated
// correctly, including accounting for local frame offsets in the parent and
// scroll positions.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, ViewBoundsInNestedFrameTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_positioned_frame.html"));
  NavigateFrameToURL(parent_iframe_node, site_url);

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForChildFrameSurfaceReady(nested_iframe_node->current_frame_host());

  float scale_factor = GetPageScaleFactor(shell());

  // Get the view bounds of the nested iframe, which should account for the
  // relative offset of its direct parent within the root frame.
  gfx::Rect bounds = rwhv_nested->GetViewBounds();

  scoped_refptr<FrameRectChangedMessageFilter> filter =
      new FrameRectChangedMessageFilter();
  root->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Scroll the parent frame downward to verify that the child rect gets updated
  // correctly.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);

  scroll_event.SetPositionInWidget(
      gfx::ToFlooredInt((bounds.x() - rwhv_root->GetViewBounds().x() - 5) *
                        scale_factor),
      gfx::ToFlooredInt((bounds.y() - rwhv_root->GetViewBounds().y() - 5) *
                        scale_factor));
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -30.0f;
  rwhv_root->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  filter->Wait();

  // The precise amount of scroll for the first view position update is not
  // deterministic, so this simply verifies that the OOPIF moved from its
  // earlier position.
  gfx::Rect update_rect = filter->last_rect();
  EXPECT_LT(update_rect.y(), bounds.y() - rwhv_root->GetViewBounds().y());
}

// This test verifies that scroll bubbling from an OOPIF properly forwards
// GestureFlingStart events from the child frame to the parent frame. This
// test times out on failure.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       GestureFlingStartEventsBubble) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_iframe_node = root->child_at(0);

  std::unique_ptr<InputEventAckWaiter> gesture_fling_start_ack_observer =
      base::MakeUnique<InputEventAckWaiter>(
          blink::WebInputEvent::kGestureFlingStart);
  root->current_frame_host()->GetRenderWidgetHost()->AddInputEventObserver(
      gesture_fling_start_ack_observer.get());

  RenderWidgetHost* child_rwh =
      child_iframe_node->current_frame_host()->GetRenderWidgetHost();

  WaitForChildFrameSurfaceReady(child_iframe_node->current_frame_host());

  gesture_fling_start_ack_observer->Reset();
  // Send a GSB, GSU, GFS sequence and verify that the GFS bubbles.
  blink::WebGestureEvent gesture_scroll_begin(
      blink::WebGestureEvent::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  gesture_scroll_begin.source_device = blink::kWebGestureDeviceTouchscreen;
  gesture_scroll_begin.data.scroll_begin.delta_hint_units =
      blink::WebGestureEvent::ScrollUnits::kPrecisePixels;
  gesture_scroll_begin.data.scroll_begin.delta_x_hint = 0.f;
  gesture_scroll_begin.data.scroll_begin.delta_y_hint = 5.f;

  child_rwh->ForwardGestureEvent(gesture_scroll_begin);

  blink::WebGestureEvent gesture_scroll_update(
      blink::WebGestureEvent::kGestureScrollUpdate,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  gesture_scroll_update.source_device = blink::kWebGestureDeviceTouchscreen;
  gesture_scroll_update.data.scroll_update.delta_units =
      blink::WebGestureEvent::ScrollUnits::kPrecisePixels;
  gesture_scroll_update.data.scroll_update.delta_x = 0.f;
  gesture_scroll_update.data.scroll_update.delta_y = 5.f;
  gesture_scroll_update.data.scroll_update.velocity_y = 5.f;

  child_rwh->ForwardGestureEvent(gesture_scroll_update);

  blink::WebGestureEvent gesture_fling_start(
      blink::WebGestureEvent::kGestureFlingStart,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  gesture_fling_start.source_device = blink::kWebGestureDeviceTouchscreen;
  gesture_fling_start.data.fling_start.velocity_x = 0.f;
  gesture_fling_start.data.fling_start.velocity_y = 5.f;

  child_rwh->ForwardGestureEvent(gesture_fling_start);

  // We now wait for the fling start event to be acked by the parent
  // frame. If the test fails, then the test times out.
  gesture_fling_start_ack_observer->Wait();
}

// Test that scrolling a nested out-of-process iframe bubbles unused scroll
// delta to a parent frame.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, ScrollBubblingFromOOPIFTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);

  // This test uses the position of the nested iframe within the parent iframe
  // to infer the scroll position of the parent. FrameRectChangedMessageFilter
  // catches updates to the position in order to avoid busy waiting.
  // It gets created early to catch the initial rects from the navigation.
  scoped_refptr<FrameRectChangedMessageFilter> filter =
      new FrameRectChangedMessageFilter();
  parent_iframe_node->current_frame_host()->GetProcess()->AddFilter(
      filter.get());

  std::unique_ptr<InputEventAckWaiter> ack_observer =
      base::MakeUnique<InputEventAckWaiter>(
          blink::WebInputEvent::kGestureScrollEnd);
  parent_iframe_node->current_frame_host()
      ->GetRenderWidgetHost()
      ->AddInputEventObserver(ack_observer.get());

  GURL site_url(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_positioned_frame.html"));
  NavigateFrameToURL(parent_iframe_node, site_url);

  // Navigate the nested frame to a page large enough to have scrollbars.
  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  GURL nested_site_url(embedded_test_server()->GetURL(
      "baz.com", "/tall_page.html"));
  NavigateFrameToURL(nested_iframe_node, nested_site_url);

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  RenderWidgetHostViewBase* rwhv_parent =
      static_cast<RenderWidgetHostViewBase*>(
          parent_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForChildFrameSurfaceReady(nested_iframe_node->current_frame_host());

  // Save the original offset as a point of reference.
  filter->Wait();
  gfx::Rect update_rect = filter->last_rect();
  int initial_y = update_rect.y();
  filter->Reset();

  // Scroll the parent frame downward.
  blink::WebMouseWheelEvent scroll_event(
      blink::WebInputEvent::kMouseWheel, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  scroll_event.SetPositionInWidget(1, 1);
  scroll_event.delta_x = 0.0f;
  scroll_event.delta_y = -5.0f;
  // Set has_precise_scroll_deltas to keep these events off the animated scroll
  // pathways, which currently break this test.
  // https://bugs.chromium.org/p/chromium/issues/detail?id=710513
  scroll_event.has_precise_scrolling_deltas = true;
  rwhv_parent->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // Ensure that the view position is propagated to the child properly.
  filter->Wait();
  update_rect = filter->last_rect();
  EXPECT_LT(update_rect.y(), initial_y);
  filter->Reset();
  ack_observer->Reset();

  // Now scroll the nested frame upward, which should bubble to the parent.
  // The upscroll exceeds the amount that the frame was initially scrolled
  // down to account for rounding.
  scroll_event.delta_y = 6.0f;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  filter->Wait();
  // This loop isn't great, but it accounts for the possibility of multiple
  // incremental updates happening as a result of the scroll animation.
  // A failure condition of this test is that the loop might not terminate
  // due to bubbling not working properly. If the overscroll bubbles to the
  // parent iframe then the nested frame's y coord will return to its
  // initial position.
  update_rect = filter->last_rect();
  while (update_rect.y() > initial_y) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
    update_rect = filter->last_rect();
  }

  filter->Reset();
  // Once we've sent a wheel to the nested iframe that we expect to turn into
  // a bubbling scroll, we need to delay to make sure the GestureScrollBegin
  // from this new scroll doesn't hit the RenderWidgetHostImpl before the
  // GestureScrollEnd bubbled from the child.
  // This timing only seems to be needed for CrOS, but we'll enable it on
  // all platforms just to lessen the possibility of tests being flakey
  // on non-CrOS platforms.
  ack_observer->Wait();

  // Scroll the parent down again in order to test scroll bubbling from
  // gestures.
  scroll_event.delta_y = -5.0f;
  rwhv_parent->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());

  // Ensure ensuing offset change is received, and then reset the filter.
  filter->Wait();
  filter->Reset();

  // Scroll down the nested iframe via gesture. This requires 3 separate input
  // events.
  blink::WebGestureEvent gesture_event(
      blink::WebGestureEvent::kGestureScrollBegin,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::kTimeStampForTesting);
  gesture_event.source_device = blink::kWebGestureDeviceTouchpad;
  gesture_event.x = 1;
  gesture_event.y = 1;
  rwhv_nested->GetRenderWidgetHost()->ForwardGestureEvent(gesture_event);

  gesture_event.SetType(blink::WebGestureEvent::kGestureScrollUpdate);
  gesture_event.data.scroll_update.delta_x = 0.0f;
  gesture_event.data.scroll_update.delta_y = 6.0f;
  gesture_event.data.scroll_update.velocity_x = 0;
  gesture_event.data.scroll_update.velocity_y = 0;
  rwhv_nested->GetRenderWidgetHost()->ForwardGestureEvent(gesture_event);

  gesture_event.SetType(blink::WebGestureEvent::kGestureScrollEnd);
  rwhv_nested->GetRenderWidgetHost()->ForwardGestureEvent(gesture_event);

  filter->Wait();
  update_rect = filter->last_rect();
  // As above, if this loop does not terminate then it indicates an issue
  // with scroll bubbling.
  while (update_rect.y() > initial_y) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
    update_rect = filter->last_rect();
  }

  // Test that when the child frame absorbs all of the scroll delta, it does
  // not propagate to the parent (see https://crbug.com/621624).
  filter->Reset();
  scroll_event.delta_y = -5.0f;
  rwhv_nested->ProcessMouseWheelEvent(scroll_event, ui::LatencyInfo());
  // It isn't possible to busy loop waiting on the renderer here because we
  // are explicitly testing that something does *not* happen. This creates a
  // small chance of false positives but shouldn't result in false negatives,
  // so flakiness implies this test is failing.
  {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::action_timeout());
    run_loop.Run();
  }
  DCHECK_EQ(filter->last_rect().x(), 0);
  DCHECK_EQ(filter->last_rect().y(), 0);
}

// Test that an ET_SCROLL event sent to an out-of-process iframe correctly
// results in a scroll. This is only handled by RenderWidgetHostViewAura
// and is needed for trackpad scrolling on Chromebooks.
#if !defined(USE_AURA)
#define MAYBE_ScrollEventToOOPIF DISABLED_ScrollEventToOOPIF
#else
#define MAYBE_ScrollEventToOOPIF ScrollEventToOOPIF
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, MAYBE_ScrollEventToOOPIF) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewAura* rwhv_parent =
      static_cast<RenderWidgetHostViewAura*>(
          root->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForChildFrameSurfaceReady(child_node->current_frame_host());

  // Create listener for input events.
  TestInputEventObserver child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  // Send a ui::ScrollEvent that will hit test to the child frame.
  ui::ScrollEvent scroll_event(ui::ET_SCROLL, gfx::Point(75, 75),
                               ui::EventTimeForNow(), ui::EF_NONE,
                               0, 10,     // Offsets
                               0, 10,  // Offset ordinals
                               2);
  rwhv_parent->OnScrollEvent(&scroll_event);

  // Verify that this a mouse wheel event was sent to the child frame renderer.
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_EQ(child_frame_monitor.EventType(), blink::WebInputEvent::kMouseWheel);
}

// Test that mouse events are being routed to the correct RenderWidgetHostView
// based on coordinates.
#if defined(THREAD_SANITIZER)
// The test times out often on TSAN bot.
// https://crbug.com/591170.
#define MAYBE_SurfaceHitTestTest DISABLED_SurfaceHitTestTest
#else
#define MAYBE_SurfaceHitTestTest SurfaceHitTestTest
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, MAYBE_SurfaceHitTestTest) {
  SurfaceHitTestTestHelper(shell(), embedded_test_server());
}

// Same test as above, but runs in high-dpi mode.
#if defined(OS_ANDROID) || defined(OS_WIN)
// High DPI browser tests are not needed on Android, and confuse some of the
// coordinate calculations. Android uses fixed device scale factor.
// Windows is disabled because of https://crbug.com/545547.
#define MAYBE_HighDPISurfaceHitTestTest DISABLED_SurfaceHitTestTest
#else
#define MAYBE_HighDPISurfaceHitTestTest SurfaceHitTestTest
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIBrowserTest,
                       MAYBE_HighDPISurfaceHitTestTest) {
  SurfaceHitTestTestHelper(shell(), embedded_test_server());
}

// Test that mouse events are being routed to the correct RenderWidgetHostView
// when there are nested out-of-process iframes.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, NestedSurfaceHitTestTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* parent_iframe_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_EQ(site_url, parent_iframe_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            parent_iframe_node->current_frame_host()->GetSiteInstance());

  FrameTreeNode* nested_iframe_node = parent_iframe_node->child_at(0);
  GURL nested_site_url(
      embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(nested_site_url, nested_iframe_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            nested_iframe_node->current_frame_host()->GetSiteInstance());
  EXPECT_NE(parent_iframe_node->current_frame_host()->GetSiteInstance(),
            nested_iframe_node->current_frame_host()->GetSiteInstance());

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor nested_frame_monitor(
      nested_iframe_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_nested =
      static_cast<RenderWidgetHostViewBase*>(
          nested_iframe_node->current_frame_host()
              ->GetRenderWidgetHost()
              ->GetView());

  WaitForChildFrameSurfaceReady(nested_iframe_node->current_frame_host());

  float scale_factor = GetPageScaleFactor(shell());

  // Get the view bounds of the nested iframe, which should account for the
  // relative offset of its direct parent within the root frame, for use in
  // targeting the input event.
  gfx::Rect bounds = rwhv_nested->GetViewBounds();

  // Target input event to nested frame.
  blink::WebMouseEvent nested_event(blink::WebInputEvent::kMouseDown,
                                    blink::WebInputEvent::kNoModifiers,
                                    blink::WebInputEvent::kTimeStampForTesting);
  nested_event.button = blink::WebPointerProperties::Button::kLeft;
  nested_event.SetPositionInWidget(
      gfx::ToCeiledInt((bounds.x() - root_view->GetViewBounds().x() + 10) *
                       scale_factor),
      gfx::ToCeiledInt((bounds.y() - root_view->GetViewBounds().y() + 10) *
                       scale_factor));
  nested_event.click_count = 1;
  nested_frame_monitor.ResetEventReceived();
  main_frame_monitor.ResetEventReceived();
  router->RouteMouseEvent(root_view, &nested_event, ui::LatencyInfo());

  EXPECT_TRUE(nested_frame_monitor.EventWasReceived());
  EXPECT_NEAR(10, nested_frame_monitor.event().PositionInWidget().x, 2);
  EXPECT_NEAR(10, nested_frame_monitor.event().PositionInWidget().y, 2);
  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
}

// This test tests that browser process hittesting ignores frames with
// pointer-events: none.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       SurfaceHitTestPointerEventsNone) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame_pointer-events_none.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForChildFrameSurfaceReady(child_node->current_frame_host());

  // Target input event to child frame.
  blink::WebMouseEvent child_event(blink::WebInputEvent::kMouseDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  child_event.button = blink::WebPointerProperties::Button::kLeft;
  child_event.SetPositionInWidget(75, 75);
  child_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  router->RouteMouseEvent(root_view, &child_event, ui::LatencyInfo());

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().x, 2);
  EXPECT_NEAR(75, main_frame_monitor.event().PositionInWidget().y, 2);
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
}

// This test verifies that MouseEnter and MouseLeave events fire correctly
// when the mouse cursor moves between processes.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CrossProcessMouseEnterAndLeaveTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c(d))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site B ------- proxies for A C D\n"
      "   +--Site C ------- proxies for A B D\n"
      "        +--Site D -- proxies for A B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/\n"
      "      D = http://d.com/",
      DepictFrameTree(root));

  FrameTreeNode* b_node = root->child_at(0);
  FrameTreeNode* c_node = root->child_at(1);
  FrameTreeNode* d_node = c_node->child_at(0);

  RenderWidgetHostViewBase* rwhv_a = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_b = static_cast<RenderWidgetHostViewBase*>(
      b_node->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_d = static_cast<RenderWidgetHostViewBase*>(
      d_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Verifying surfaces are ready in B and D are sufficient, since other
  // surfaces contain at least one of them.
  WaitForChildFrameSurfaceReady(b_node->current_frame_host());
  WaitForChildFrameSurfaceReady(d_node->current_frame_host());

  // Create listeners for mouse events. These are used to verify that the
  // RenderWidgetHostInputEventRouter is generating MouseLeave, etc for
  // the right renderers.
  RenderWidgetHostMouseEventMonitor root_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor a_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor b_frame_monitor(
      b_node->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor c_frame_monitor(
      c_node->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor d_frame_monitor(
      d_node->current_frame_host()->GetRenderWidgetHost());

  float scale_factor = GetPageScaleFactor(shell());

  // Get the view bounds of the child iframe, which should account for the
  // relative offset of its direct parent within the root frame, for use in
  // targeting the input event.
  gfx::Rect a_bounds = rwhv_a->GetViewBounds();
  gfx::Rect b_bounds = rwhv_b->GetViewBounds();
  gfx::Rect d_bounds = rwhv_d->GetViewBounds();

  gfx::Point point_in_a_frame(2, 2);
  gfx::Point point_in_b_frame(
      gfx::ToCeiledInt((b_bounds.x() - a_bounds.x() + 25) * scale_factor),
      gfx::ToCeiledInt((b_bounds.y() - a_bounds.y() + 25) * scale_factor));
  gfx::Point point_in_d_frame(
      gfx::ToCeiledInt((d_bounds.x() - a_bounds.x() + 25) * scale_factor),
      gfx::ToCeiledInt((d_bounds.y() - a_bounds.y() + 25) * scale_factor));

  blink::WebMouseEvent mouse_event(blink::WebInputEvent::kMouseMove,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  mouse_event.SetPositionInWidget(point_in_a_frame.x(), point_in_a_frame.y());

  // Send an initial MouseMove to the root view, which shouldn't affect the
  // other renderers.
  web_contents()->GetInputEventRouter()->RouteMouseEvent(rwhv_a, &mouse_event,
                                                         ui::LatencyInfo());
  EXPECT_TRUE(a_frame_monitor.EventWasReceived());
  a_frame_monitor.ResetEventReceived();
  EXPECT_FALSE(b_frame_monitor.EventWasReceived());
  EXPECT_FALSE(c_frame_monitor.EventWasReceived());
  EXPECT_FALSE(d_frame_monitor.EventWasReceived());

  // Next send a MouseMove to B frame, which shouldn't affect C or D but
  // A should receive a MouseMove event.
  mouse_event.SetPositionInWidget(point_in_b_frame.x(), point_in_b_frame.y());
  web_contents()->GetInputEventRouter()->RouteMouseEvent(rwhv_a, &mouse_event,
                                                         ui::LatencyInfo());
  EXPECT_TRUE(a_frame_monitor.EventWasReceived());
  EXPECT_EQ(a_frame_monitor.event().GetType(),
            blink::WebInputEvent::kMouseMove);
  a_frame_monitor.ResetEventReceived();
  EXPECT_TRUE(b_frame_monitor.EventWasReceived());
  b_frame_monitor.ResetEventReceived();
  EXPECT_FALSE(c_frame_monitor.EventWasReceived());
  EXPECT_FALSE(d_frame_monitor.EventWasReceived());

  // Next send a MouseMove to D frame, which should have side effects in every
  // other RenderWidgetHostView.
  mouse_event.SetPositionInWidget(point_in_d_frame.x(), point_in_d_frame.y());
  web_contents()->GetInputEventRouter()->RouteMouseEvent(rwhv_a, &mouse_event,
                                                         ui::LatencyInfo());
  EXPECT_TRUE(a_frame_monitor.EventWasReceived());
  EXPECT_EQ(a_frame_monitor.event().GetType(),
            blink::WebInputEvent::kMouseMove);
  EXPECT_TRUE(b_frame_monitor.EventWasReceived());
  EXPECT_EQ(b_frame_monitor.event().GetType(),
            blink::WebInputEvent::kMouseLeave);
  EXPECT_TRUE(c_frame_monitor.EventWasReceived());
  EXPECT_EQ(c_frame_monitor.event().GetType(),
            blink::WebInputEvent::kMouseMove);
  EXPECT_TRUE(d_frame_monitor.EventWasReceived());
}

// Verify that mouse capture works on a RenderWidgetHostView level, so that
// dragging scroll bars and selecting text continues even when the mouse
// cursor crosses over cross-process frame boundaries.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrossProcessMouseCapture) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  // Create listeners for mouse events.
  RenderWidgetHostMouseEventMonitor main_frame_monitor(
      root->current_frame_host()->GetRenderWidgetHost());
  RenderWidgetHostMouseEventMonitor child_frame_monitor(
      child_node->current_frame_host()->GetRenderWidgetHost());

  RenderWidgetHostInputEventRouter* router =
      web_contents()->GetInputEventRouter();

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  WaitForChildFrameSurfaceReady(child_node->current_frame_host());

  float scale_factor = GetPageScaleFactor(shell());

  // Get the view bounds of the child iframe, which should account for the
  // relative offset of its direct parent within the root frame, for use in
  // targeting the input event.
  gfx::Rect bounds = rwhv_child->GetViewBounds();
  int child_frame_target_x = gfx::ToCeiledInt(
      (bounds.x() - root_view->GetViewBounds().x() + 5) * scale_factor);
  int child_frame_target_y = gfx::ToCeiledInt(
      (bounds.y() - root_view->GetViewBounds().y() + 5) * scale_factor);

  // Target MouseDown to child frame.
  blink::WebMouseEvent mouse_event(blink::WebInputEvent::kMouseDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.SetPositionInWidget(child_frame_target_x, child_frame_target_y);
  mouse_event.click_count = 1;
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());

  // Target MouseMove to main frame. This should still be routed to the
  // child frame because it is now capturing mouse input.
  mouse_event.SetType(blink::WebInputEvent::kMouseMove);
  mouse_event.SetModifiers(blink::WebInputEvent::kLeftButtonDown);
  mouse_event.SetPositionInWidget(1, 1);
  // Note that this event is sent twice, with the monitors cleared after
  // the first time, because the first MouseMove to the child frame
  // causes a MouseMove to be sent to the main frame also, which we
  // need to ignore.
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  mouse_event.SetPositionInWidget(1, 5);
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());

  // A MouseUp to the child frame should cancel the mouse capture.
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  mouse_event.SetModifiers(blink::WebInputEvent::kNoModifiers);
  mouse_event.SetPositionInWidget(child_frame_target_x, child_frame_target_y);
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  EXPECT_FALSE(main_frame_monitor.EventWasReceived());
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());

  // Subsequent MouseMove events targeted to the main frame should be routed
  // to that frame.
  mouse_event.SetType(blink::WebInputEvent::kMouseMove);
  mouse_event.SetPositionInWidget(1, 10);
  // Sending the MouseMove twice for the same reason as above.
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  mouse_event.SetPositionInWidget(1, 15);
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());

  // Target MouseDown to the main frame to cause it to capture input.
  mouse_event.SetType(blink::WebInputEvent::kMouseDown);
  mouse_event.SetPositionInWidget(1, 20);
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());

  // Sending a MouseMove to the child frame should still result in the main
  // frame receiving the event.
  mouse_event.SetType(blink::WebInputEvent::kMouseMove);
  mouse_event.SetModifiers(blink::WebInputEvent::kLeftButtonDown);
  mouse_event.SetPositionInWidget(child_frame_target_x, child_frame_target_y);
  main_frame_monitor.ResetEventReceived();
  child_frame_monitor.ResetEventReceived();
  router->RouteMouseEvent(root_view, &mouse_event, ui::LatencyInfo());

  EXPECT_TRUE(main_frame_monitor.EventWasReceived());
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
}

// Tests OOPIF rendering by checking that the RWH of the iframe generates
// OnSwapCompositorFrame message.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CompositorFrameSwapped) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(baz)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "baz.com", "/cross_site_iframe_factory.html?baz()"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());
  RenderWidgetHostViewBase* rwhv_base = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Wait for OnSwapCompositorFrame message.
  while (rwhv_base->RendererFrameNumber() <= 0) {
    // TODO(lazyboy): Find a better way to avoid sleeping like this. See
    // http://crbug.com/405282 for details.
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(10));
    run_loop.Run();
  }
}

// Ensure that OOPIFs are deleted after navigating to a new main frame.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CleanupCrossSiteIframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load a cross-site page into both iframes.
  GURL foo_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), foo_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(foo_url, observer.last_navigation_url());
  NavigateFrameToURL(root->child_at(1), foo_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(foo_url, observer.last_navigation_url());

  // Ensure that we have created a new process for the subframes.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  int subframe_process_id = root->child_at(0)
                                ->current_frame_host()
                                ->GetSiteInstance()
                                ->GetProcess()
                                ->GetID();
  int subframe_rvh_id = root->child_at(0)
                            ->current_frame_host()
                            ->render_view_host()
                            ->GetRoutingID();
  EXPECT_TRUE(RenderViewHost::FromID(subframe_process_id, subframe_rvh_id));

  // Use Javascript in the parent to remove one of the frames and ensure that
  // the subframe goes away.
  EXPECT_TRUE(ExecuteScript(shell(),
                            "document.body.removeChild("
                            "document.querySelectorAll('iframe')[0])"));
  ASSERT_EQ(1U, root->child_count());

  // Load a new same-site page in the top-level frame and ensure the other
  // subframe goes away.
  GURL new_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), new_url));
  ASSERT_EQ(0U, root->child_count());

  // Ensure the RVH for the subframe gets cleaned up when the frame goes away.
  EXPECT_FALSE(RenderViewHost::FromID(subframe_process_id, subframe_rvh_id));
}

// Ensure that root frames cannot be detached.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, RestrictFrameDetach) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site pages into both iframes.
  GURL foo_url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(root->child_at(0), foo_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(foo_url, observer.last_navigation_url());
  GURL bar_url = embedded_test_server()->GetURL("bar.com", "/title2.html");
  NavigateFrameToURL(root->child_at(1), bar_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(bar_url, observer.last_navigation_url());

  // Ensure that we have created new processes for the subframes.
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* foo_child = root->child_at(0);
  SiteInstance* foo_site_instance =
      foo_child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), foo_site_instance);
  FrameTreeNode* bar_child = root->child_at(1);
  SiteInstance* bar_site_instance =
      bar_child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), bar_site_instance);

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  // Simulate an attempt to detach the root frame from foo_site_instance.  This
  // should kill foo_site_instance's process.
  RenderFrameProxyHost* foo_mainframe_rfph =
      root->render_manager()->GetRenderFrameProxyHost(foo_site_instance);
  content::RenderProcessHostWatcher foo_terminated(
      foo_mainframe_rfph->GetProcess(),
      content::RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  FrameHostMsg_Detach evil_msg2(foo_mainframe_rfph->GetRoutingID());
  IPC::IpcSecurityTestUtil::PwnMessageReceived(
      foo_mainframe_rfph->GetProcess()->GetChannel(), evil_msg2);
  foo_terminated.Wait();

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/ (no process)\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, NavigateRemoteFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Load cross-site page into iframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateFrameToURL(root->child_at(0), url);
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Ensure that we have created a new process for the subframe.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        |--Site A -- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "             +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));
  SiteInstance* site_instance = child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);

  // Emulate the main frame changing the src of the iframe such that it
  // navigates cross-site.
  url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateIframeToURL(shell()->web_contents(), "child-0", url);
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  // Check again that a new process is created and is different from the
  // top level one and the previous one.
  EXPECT_EQ(
      " Site A ------------ proxies for C\n"
      "   |--Site C ------- proxies for A\n"
      "   +--Site A ------- proxies for C\n"
      "        |--Site A -- proxies for C\n"
      "        +--Site A -- proxies for C\n"
      "             +--Site A -- proxies for C\n"
      "Where A = http://a.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  // Navigate back to the parent's origin and ensure we return to the
  // parent's process.
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateFrameToURL(child, http_url);
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateRemoteFrameToBlankAndDataURLs) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  // Load cross-site page into iframe.
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigateFrameToURL(child, url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Navigate iframe to a data URL. The navigation happens from a script in the
  // parent frame, so the data URL should be committed in the same SiteInstance
  // as the parent frame.
  RenderFrameDeletedObserver deleted_observer1(
      root->child_at(0)->current_frame_host());
  GURL data_url("data:text/html,dataurl");
  NavigateIframeToURL(shell()->web_contents(), "child-0", data_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(data_url, observer.last_navigation_url());

  // Wait for the old process to exit, to verify that the proxies go away.
  deleted_observer1.WaitUntilDeleted();

  // Ensure that we have navigated using the top level process.
  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  // Load cross-site page into iframe.
  url = embedded_test_server()->GetURL("bar.com", "/title2.html");
  NavigateFrameToURL(child, url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for C\n"
      "   |--Site C ------- proxies for A\n"
      "   +--Site A ------- proxies for C\n"
      "        +--Site A -- proxies for C\n"
      "Where A = http://a.com/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  // Navigate iframe to about:blank. The navigation happens from a script in the
  // parent frame, so it should be committed in the same SiteInstance as the
  // parent frame.
  RenderFrameDeletedObserver deleted_observer2(
      root->child_at(0)->current_frame_host());
  GURL about_blank_url("about:blank");
  NavigateIframeToURL(shell()->web_contents(), "child-0", about_blank_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(about_blank_url, observer.last_navigation_url());

  // Wait for the old process to exit, to verify that the proxies go away.
  deleted_observer2.WaitUntilDeleted();

  // Ensure that we have navigated using the top level process.
  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));

  // Load cross-site page into iframe again.
  url = embedded_test_server()->GetURL("f00.com", "/title3.html");
  NavigateFrameToURL(child, url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for D\n"
      "   |--Site D ------- proxies for A\n"
      "   +--Site A ------- proxies for D\n"
      "        +--Site A -- proxies for D\n"
      "Where A = http://a.com/\n"
      "      D = http://f00.com/",
      DepictFrameTree(root));

  // Navigate the iframe itself to about:blank using a script executing in its
  // own context. It should stay in the same SiteInstance as before, not the
  // parent one.
  std::string script(
      "window.domAutomationController.send("
      "window.location.href = 'about:blank');");
  TestFrameNavigationObserver frame_observer(child);
  EXPECT_TRUE(ExecuteScript(child, script));
  frame_observer.Wait();
  EXPECT_EQ(about_blank_url, child->current_url());

  // Ensure that we have navigated using the top level process.
  EXPECT_EQ(
      " Site A ------------ proxies for D\n"
      "   |--Site D ------- proxies for A\n"
      "   +--Site A ------- proxies for D\n"
      "        +--Site A -- proxies for D\n"
      "Where A = http://a.com/\n"
      "      D = http://f00.com/",
      DepictFrameTree(root));
}

// This test checks that killing a renderer process of a remote frame
// and then navigating some other frame to the same SiteInstance of the killed
// process works properly.
// This can be illustrated as follows,
// where 1/2/3 are FrameTreeNode-s and A/B are processes and B* is the killed
// B process:
//
//     1        A                  A                           A
//    / \  ->  / \  -> Kill B ->  / \  -> Navigate 3 to B ->  / \  .
//   2   3    B   A              B*  A                       B*  B
//
// Initially, node1.proxy_hosts_ = {B}
// After we kill B, we make sure B stays in node1.proxy_hosts_, then we navigate
// 3 to B and we expect that to complete normally.
// See http://crbug.com/432107.
//
// Note that due to http://crbug.com/450681, node2 cannot be re-navigated to
// site B and stays in not rendered state.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateRemoteFrameToKilledProcess) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(bar.com, foo.com)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure node2 points to the correct cross-site page.
  GURL site_b_url = embedded_test_server()->GetURL(
      "bar.com", "/cross_site_iframe_factory.html?bar.com()");
  FrameTreeNode* node2 = root->child_at(0);
  EXPECT_EQ(site_b_url, node2->current_url());

  // Kill that cross-site renderer.
  RenderProcessHost* child_process =
      node2->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0, false);
  crash_observer.Wait();

  // Now navigate the second iframe (node3) to the same site as the node2.
  FrameTreeNode* node3 = root->child_at(1);
  NavigateFrameToURL(node3, site_b_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(site_b_url, observer.last_navigation_url());
}

// This test ensures that WebContentsImpl::FocusOwningWebContents does not crash
// the browser if the currently focused frame's renderer has disappeared.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, RemoveFocusFromKilledFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/cross_site_iframe_factory.html?foo.com(bar.com)"));
  NavigateToURL(shell(), main_url);

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(1U, root->child_count());

  // Make sure node2 points to the correct cross-site page.
  GURL site_b_url = embedded_test_server()->GetURL(
      "bar.com", "/cross_site_iframe_factory.html?bar.com()");
  FrameTreeNode* node2 = root->child_at(0);
  EXPECT_EQ(site_b_url, node2->current_url());

  web_contents()->SetFocusedFrame(
      node2, node2->current_frame_host()->GetSiteInstance());

  // Kill that cross-site renderer.
  RenderProcessHost* child_process = node2->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0, false);
  crash_observer.Wait();

  // Try to focus the root's owning WebContents.
  web_contents()->FocusOwningWebContents(
      root->current_frame_host()->GetRenderWidgetHost());
}

// This test is similar to
// SitePerProcessBrowserTest.NavigateRemoteFrameToKilledProcess with
// addition that node2 also has a cross-origin frame to site C.
//
//     1          A                  A                       A
//    / \        / \                / \                     / \  .
//   2   3 ->   B   A -> Kill B -> B*   A -> Navigate 3 -> B*  B
//  /          /
// 4          C
//
// Initially, node1.proxy_hosts_ = {B, C}
// After we kill B, we make sure B stays in node1.proxy_hosts_, but
// C gets cleared from node1.proxy_hosts_.
//
// Note that due to http://crbug.com/450681, node2 cannot be re-navigated to
// site B and stays in not rendered state.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateRemoteFrameToKilledProcessWithSubtree) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(bar(baz), a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  ASSERT_EQ(2U, root->child_count());

  GURL site_b_url(embedded_test_server()->GetURL(
      "bar.com", "/cross_site_iframe_factory.html?bar(baz())"));
  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(site_b_url, root->child_at(0)->current_url());

  // Ensure that a new process is created for node2.
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());
  // Ensure that a new process is *not* created for node3.
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            root->child_at(1)->current_frame_host()->GetSiteInstance());

  ASSERT_EQ(1U, root->child_at(0)->child_count());

  // Make sure node4 points to the correct cross-site page.
  FrameTreeNode* node4 = root->child_at(0)->child_at(0);
  GURL site_c_url(embedded_test_server()->GetURL(
      "baz.com", "/cross_site_iframe_factory.html?baz()"));
  EXPECT_EQ(site_c_url, node4->current_url());

  // |site_instance_c| is expected to go away once we kill |child_process_b|
  // below, so create a local scope so we can extend the lifetime of
  // |site_instance_c| with a refptr.
  {
    // Initially each frame has proxies for the other sites.
    EXPECT_EQ(
        " Site A ------------ proxies for B C\n"
        "   |--Site B ------- proxies for A C\n"
        "   |    +--Site C -- proxies for A B\n"
        "   +--Site A ------- proxies for B C\n"
        "Where A = http://a.com/\n"
        "      B = http://bar.com/\n"
        "      C = http://baz.com/",
        DepictFrameTree(root));

    // Kill the render process for Site B.
    RenderProcessHost* child_process_b =
        root->child_at(0)->current_frame_host()->GetProcess();
    RenderProcessHostWatcher crash_observer(
        child_process_b, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process_b->Shutdown(0, false);
    crash_observer.Wait();

    // The Site C frame (a child of the crashed Site B frame) should go away,
    // and there should be no remaining proxies for site C anywhere.
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site B ------- proxies for A\n"
        "   +--Site A ------- proxies for B\n"
        "Where A = http://a.com/\n"
        "      B = http://bar.com/ (no process)",
        DepictFrameTree(root));
  }

  // Now navigate the second iframe (node3) to Site B also.
  FrameTreeNode* node3 = root->child_at(1);
  GURL url = embedded_test_server()->GetURL("bar.com", "/title1.html");
  NavigateFrameToURL(node3, url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));
}

// Ensure that the renderer process doesn't crash when the main frame navigates
// a remote child to a page that results in a network error.
// See https://crbug.com/558016.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, NavigateRemoteAfterError) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Load same-site page into iframe.
  {
    TestNavigationObserver observer(shell()->web_contents());
    FrameTreeNode* child = root->child_at(0);
    GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    NavigateFrameToURL(child, http_url);
    EXPECT_EQ(http_url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    observer.Wait();
  }

  // Load cross-site page into iframe.
  {
    TestNavigationObserver observer(shell()->web_contents());
    FrameTreeNode* child = root->child_at(0);
    GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
    NavigateFrameToURL(root->child_at(0), url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(url, observer.last_navigation_url());
    observer.Wait();

    // Ensure that we have created a new process for the subframe.
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://foo.com/",
        DepictFrameTree(root));
    SiteInstance* site_instance =
        child->current_frame_host()->GetSiteInstance();
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site_instance);
  }

  // Stop the test server and try to navigate the remote frame.
  {
    GURL url = embedded_test_server()->GetURL("bar.com", "/title3.html");
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    NavigateIframeToURL(shell()->web_contents(), "child-0", url);
  }
}

namespace {
class FailingURLLoaderImpl : public mojom::URLLoader {
 public:
  explicit FailingURLLoaderImpl(mojom::URLLoaderClientPtr client) {
    ResourceRequestCompletionStatus status;
    status.error_code = net::ERR_NOT_IMPLEMENTED;
    client->OnComplete(status);
  }

  void FollowRedirect() override {}
  void SetPriority(net::RequestPriority priority,
                   int32_t intra_priority_value) override {}
};

class FailingLoadFactory : public mojom::URLLoaderFactory {
 public:
  FailingLoadFactory() {}
  ~FailingLoadFactory() override {}

  void CreateLoaderAndStart(mojom::URLLoaderAssociatedRequest loader,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const ResourceRequest& request,
                            mojom::URLLoaderClientPtr client) override {
    new FailingURLLoaderImpl(std::move(client));
  }
  void SyncLoad(int32_t routing_id,
                int32_t request_id,
                const ResourceRequest& request,
                SyncLoadCallback callback) override {}
};
}

// Ensure that a cross-site page ends up in the correct process when it
// successfully loads after earlier encountering a network error for it.
// See https://crbug.com/560511.
// TODO(creis): Make the net error page show in the correct process as well,
// per https://crbug.com/588314.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, ProcessTransferAfterError) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);
  GURL url_a = child->current_url();

  // Disable host resolution in the test server and try to navigate the subframe
  // cross-site, which will lead to a committed net error.
  GURL url_b = embedded_test_server()->GetURL("b.com", "/title3.html");
  bool network_service = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableNetworkService);
  mojom::URLLoaderFactoryPtr failing_factory;
  mojo::MakeStrongBinding(base::MakeUnique<FailingLoadFactory>(),
                          mojo::MakeRequest(&failing_factory));
  StoragePartitionImpl* storage_partition = nullptr;
  if (network_service) {
    storage_partition = static_cast<StoragePartitionImpl*>(
        BrowserContext::GetDefaultStoragePartition(
            shell()->web_contents()->GetBrowserContext()));
    storage_partition->url_loader_factory_getter()->SetNetworkFactoryForTesting(
        std::move(failing_factory));
  } else {
    host_resolver()->ClearRules();
  }

  TestNavigationObserver observer(shell()->web_contents());
  NavigateIframeToURL(shell()->web_contents(), "child-0", url_b);
  EXPECT_FALSE(observer.last_navigation_succeeded());
  EXPECT_EQ(url_b, observer.last_navigation_url());
  EXPECT_EQ(2, shell()->web_contents()->GetController().GetEntryCount());

  // PlzNavigate: Ensure that we have created a new process for the subframe.
  if (IsBrowserSideNavigationEnabled()) {
    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://b.com/",
        DepictFrameTree(root));
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
              child->current_frame_host()->GetSiteInstance());
  }

  // The FrameTreeNode should update its URL (so that we don't affect other uses
  // of the API), but the frame's last_successful_url shouldn't change and the
  // origin should be empty.
  // PlzNavigate: We have switched RenderFrameHosts for the subframe, so the
  // last successful url should be empty (since the frame only loaded an error
  // page).
  if (IsBrowserSideNavigationEnabled())
    EXPECT_EQ(GURL(), child->current_frame_host()->last_successful_url());
  else
    EXPECT_EQ(url_a, child->current_frame_host()->last_successful_url());
  EXPECT_EQ(url_b, child->current_url());
  EXPECT_EQ("null", child->current_origin().Serialize());

  // Try again after re-enabling host resolution.
  if (network_service) {
    storage_partition->url_loader_factory_getter()->SetNetworkFactoryForTesting(
        nullptr);
  } else {
    host_resolver()->AddRule("*", "127.0.0.1");
  }

  NavigateIframeToURL(shell()->web_contents(), "child-0", url_b);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url_b, observer.last_navigation_url());

  // The FrameTreeNode should have updated its URL and origin.
  EXPECT_EQ(url_b, child->current_frame_host()->last_successful_url());
  EXPECT_EQ(url_b, child->current_url());
  EXPECT_EQ(url_b.GetOrigin().spec(),
            child->current_origin().Serialize() + '/');

  // Ensure that we have created a new process for the subframe.
  // PlzNavigate: the subframe should still be in its separate process.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Make sure that the navigation replaced the error page and that going back
  // ends up on the original site.
  EXPECT_EQ(2, shell()->web_contents()->GetController().GetEntryCount());
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    TestNavigationObserver back_load_observer(shell()->web_contents());
    shell()->web_contents()->GetController().GoBack();
    back_load_observer.Wait();

    // Wait for the old process to exit, to verify that the proxies go away.
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_EQ(
      " Site A\n"
      "   +--Site A\n"
      "Where A = http://a.com/",
      DepictFrameTree(root));
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(url_a, child->current_frame_host()->last_successful_url());
  EXPECT_EQ(url_a, child->current_url());
  EXPECT_EQ(url_a.GetOrigin().spec(),
            child->current_origin().Serialize() + '/');
}

// Verify that killing a cross-site frame's process B and then navigating a
// frame to B correctly recreates all proxies in B.
//
//      1           A                    A          A
//    / | \       / | \                / | \      / | \  .
//   2  3  4 ->  B  A  A -> Kill B -> B* A  A -> B* B  A
//
// After the last step, the test sends a postMessage from node 3 to node 4,
// verifying that a proxy for node 4 has been recreated in process B.  This
// verifies the fix for https://crbug.com/478892.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigatingToKilledProcessRestoresAllProxies) {
  // Navigate to a page with three frames: one cross-site and two same-site.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_three_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Kill the first subframe's b.com renderer.
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0, false);
  crash_observer.Wait();

  // Navigate the second subframe to b.com to recreate the b.com process.
  GURL b_url = embedded_test_server()->GetURL("b.com", "/post_message.html");
  NavigateFrameToURL(root->child_at(1), b_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(b_url, observer.last_navigation_url());
  EXPECT_TRUE(root->child_at(1)->current_frame_host()->IsRenderFrameLive());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Check that third subframe's proxy is available in the b.com process by
  // sending it a postMessage from second subframe, and waiting for a reply.
  PostMessageAndWaitForReply(root->child_at(1),
                             "postToSibling('subframe-msg','frame3')",
                             "\"done-frame2\"");
}

// Verify that proxy creation doesn't recreate a crashed process if no frame
// will be created in it.
//
//      1           A                    A          A
//    / | \       / | \                / | \      / | \    .
//   2  3  4 ->  B  A  A -> Kill B -> B* A  A -> B* A  A
//                                                      \  .
//                                                       A
//
// The test kills process B (node 2), creates a child frame of node 4 in
// process A, and then checks that process B isn't resurrected to create a
// proxy for the new child frame.  See https://crbug.com/476846.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CreateChildFrameAfterKillingProcess) {
  // Navigate to a page with three frames: one cross-site and two same-site.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_three_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  SiteInstance* b_site_instance =
      root->child_at(0)->current_frame_host()->GetSiteInstance();

  // Kill the first subframe's renderer (B).
  RenderProcessHost* child_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0, false);
  crash_observer.Wait();

  // Add a new child frame to the third subframe.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(
      root->child_at(2),
      "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();

  // The new frame should have a RenderFrameProxyHost for B, but it should not
  // be alive, and B should still not have a process (verified by last line of
  // expected DepictFrameTree output).
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/ (no process)",
      DepictFrameTree(root));
  FrameTreeNode* grandchild = root->child_at(2)->child_at(0);
  RenderFrameProxyHost* grandchild_rfph =
      grandchild->render_manager()->GetRenderFrameProxyHost(b_site_instance);
  EXPECT_FALSE(grandchild_rfph->is_render_frame_proxy_live());

  // Navigate the second subframe to b.com to recreate process B.
  TestNavigationObserver observer(shell()->web_contents());
  GURL b_url = embedded_test_server()->GetURL("b.com", "/title1.html");
  NavigateFrameToURL(root->child_at(1), b_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(b_url, observer.last_navigation_url());

  // Ensure that the grandchild RenderFrameProxy in B was created when process
  // B was restored.
  EXPECT_TRUE(grandchild_rfph->is_render_frame_proxy_live());
}

// Verify that creating a child frame after killing and reloading an opener
// process doesn't crash. See https://crbug.com/501152.
//   1. Navigate to site A.
//   2. Open a popup with window.open and navigate it cross-process to site B.
//   3. Kill process A for the original tab.
//   4. Reload the original tab to resurrect process A.
//   5. Add a child frame to the top-level frame in the popup tab B.
// In step 5, we try to create proxies for the child frame in all SiteInstances
// for which its parent has proxies.  This includes A.  However, even though
// process A is live (step 4), the parent proxy in A is not live (which was
// incorrectly assumed previously).  This is because step 4 does not resurrect
// proxies for popups opened before the crash.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CreateChildFrameAfterKillingOpener) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  SiteInstance* site_instance_a = root->current_frame_host()->GetSiteInstance();

  // Open a popup and navigate it cross-process to b.com.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(root, "popup = window.open('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(NavigateToURL(popup, popup_url));

  // Verify that each top-level frame has proxies in the other's SiteInstance.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  EXPECT_EQ(
      " Site B ------------ proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(popup_root));

  // Kill the first window's renderer (a.com).
  RenderProcessHost* child_process = root->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0, false);
  crash_observer.Wait();
  EXPECT_FALSE(root->current_frame_host()->IsRenderFrameLive());

  // The proxy for the popup in a.com should've died.
  RenderFrameProxyHost* rfph =
      popup_root->render_manager()->GetRenderFrameProxyHost(site_instance_a);
  EXPECT_FALSE(rfph->is_render_frame_proxy_live());

  // Recreate the a.com renderer.
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_TRUE(root->current_frame_host()->IsRenderFrameLive());

  // The popup's proxy in a.com should still not be live. Re-navigating the
  // main window to a.com doesn't reinitialize a.com proxies for popups
  // previously opened from the main window.
  EXPECT_FALSE(rfph->is_render_frame_proxy_live());

  // Add a new child frame on the popup.
  RenderFrameHostCreatedObserver frame_observer(popup->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(
      popup, "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();

  // Both the child frame's and its parent's proxies should still not be live.
  // The main page can't reach them since it lost reference to the popup after
  // it crashed, so there is no need to create them.
  EXPECT_FALSE(rfph->is_render_frame_proxy_live());
  RenderFrameProxyHost* child_rfph =
      popup_root->child_at(0)->render_manager()->GetRenderFrameProxyHost(
          site_instance_a);
  EXPECT_TRUE(child_rfph);
  EXPECT_FALSE(child_rfph->is_render_frame_proxy_live());
}

// In A-embed-B-embed-C scenario, verify that killing process B clears proxies
// of C from the tree.
//
//     1          A                  A
//    / \        / \                / \    .
//   2   3 ->   B   A -> Kill B -> B*  A
//  /          /
// 4          C
//
// node1 is the root.
// Initially, both node1.proxy_hosts_ and node3.proxy_hosts_ contain C.
// After we kill B, make sure proxies for C are cleared.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       KillingRendererClearsDescendantProxies) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_two_frames_nested.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(2U, root->child_count());

  GURL site_b_url(
      embedded_test_server()->GetURL(
          "bar.com", "/frame_tree/page_with_one_frame.html"));
  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(site_b_url, root->child_at(0)->current_url());

  // Ensure that a new process is created for node2.
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());
  // Ensure that a new process is *not* created for node3.
  EXPECT_EQ(shell()->web_contents()->GetSiteInstance(),
            root->child_at(1)->current_frame_host()->GetSiteInstance());

  ASSERT_EQ(1U, root->child_at(0)->child_count());

  // Make sure node4 points to the correct cross-site-page.
  FrameTreeNode* node4 = root->child_at(0)->child_at(0);
  GURL site_c_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_c_url, node4->current_url());

  // |site_instance_c|'s frames and proxies are expected to go away once we kill
  // |child_process_b| below.
  scoped_refptr<SiteInstanceImpl> site_instance_c =
      node4->current_frame_host()->GetSiteInstance();

  // Initially proxies for both B and C will be present in the root.
  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   |    +--Site C -- proxies for A B\n"
      "   +--Site A ------- proxies for B C\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  EXPECT_GT(site_instance_c->active_frame_count(), 0U);

  // Kill process B.
  RenderProcessHost* child_process_b =
      root->child_at(0)->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process_b, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process_b->Shutdown(0, false);
  crash_observer.Wait();

  // Make sure proxy C has gone from root.
  // Make sure proxy C has gone from node3 as well.
  // Make sure proxy B stays around in root and node3.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://a.com/\n"
      "      B = http://bar.com/ (no process)",
      DepictFrameTree(root));

  EXPECT_EQ(0U, site_instance_c->active_frame_count());
}

// Crash a subframe and ensures its children are cleared from the FrameTree.
// See http://crbug.com/338508.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrashSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Check the subframe process.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
  FrameTreeNode* child = root->child_at(0);
  EXPECT_TRUE(
      child->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_TRUE(child->current_frame_host()->IsRenderFrameLive());

  // Crash the subframe process.
  RenderProcessHost* root_process = root->current_frame_host()->GetProcess();
  RenderProcessHost* child_process = child->current_frame_host()->GetProcess();
  {
    RenderProcessHostWatcher crash_observer(
        child_process,
        RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process->Shutdown(0, false);
    crash_observer.Wait();
  }

  // Ensure that the child frame still exists but has been cleared.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/ (no process)",
      DepictFrameTree(root));
  EXPECT_EQ(1U, root->child_count());
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(GURL(), child->current_url());

  EXPECT_FALSE(
      child->current_frame_host()->render_view_host()->IsRenderViewLive());
  EXPECT_FALSE(child->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(child->current_frame_host()->render_frame_created_);

  // Now crash the top-level page to clear the child frame.
  {
    RenderProcessHostWatcher crash_observer(
        root_process,
        RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    root_process->Shutdown(0, false);
    crash_observer.Wait();
  }
  EXPECT_EQ(0U, root->child_count());
  EXPECT_EQ(GURL(), root->current_url());
}

// When a new subframe is added, related SiteInstances that can reach the
// subframe should create proxies for it (https://crbug.com/423587).  This test
// checks that if A embeds B and later adds a new subframe A2, A2 gets a proxy
// in B's process.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CreateProxiesForNewFrames) {
  GURL main_url(embedded_test_server()->GetURL(
      "b.com", "/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  // Make sure the frame starts out at the correct cross-site URL.
  EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
            root->child_at(0)->current_url());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://b.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Add a new child frame to the top-level frame.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(shell(), "addFrame('data:text/html,foo');"));
  frame_observer.Wait();

  // The new frame should have a proxy in Site B, for use by the old frame.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://b.com/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));
}

// TODO(nasko): Disable this test until out-of-process iframes is ready and the
// security checks are back in place.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DISABLED_CrossSiteIframeRedirectOnce) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server.Start());

  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  GURL https_url(https_server.GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  TestNavigationObserver observer(shell()->web_contents());
  {
    // Load cross-site client-redirect page into Iframe.
    // Should be blocked.
    GURL client_redirect_https_url(
        https_server.GetURL("/client-redirect?/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_https_url));
    // DidFailProvisionalLoad when navigating to client_redirect_https_url.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load cross-site server-redirect page into Iframe,
    // which redirects to same-site page.
    GURL server_redirect_http_url(
        https_server.GetURL("/server-redirect?" + http_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  {
    // Load cross-site server-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL server_redirect_http_url(
        https_server.GetURL("/server-redirect?/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    // DidFailProvisionalLoad when navigating to https_url.
    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load same-site server-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL server_redirect_http_url(
        embedded_test_server()->GetURL("/server-redirect?" + https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));

    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
   }

  {
    // Load same-site client-redirect page into Iframe,
    // which redirects to cross-site page.
    GURL client_redirect_http_url(
        embedded_test_server()->GetURL("/client-redirect?" + https_url.spec()));

    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(
            &shell()->web_contents()->GetController()));

    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_http_url));

    // Same-site Client-Redirect Page should be loaded successfully.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());

    // Redirecting to Cross-site Page should be blocked.
    load_observer2.Wait();
    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load same-site server-redirect page into Iframe,
    // which redirects to same-site page.
    GURL server_redirect_http_url(
        embedded_test_server()->GetURL("/server-redirect?/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
   }

  {
    // Load same-site client-redirect page into Iframe,
    // which redirects to same-site page.
    GURL client_redirect_http_url(
        embedded_test_server()->GetURL("/client-redirect?" + http_url.spec()));
    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(
            &shell()->web_contents()->GetController()));

    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_http_url));

    // Same-site Client-Redirect Page should be loaded successfully.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());

    // Redirecting to Same-site Page should be loaded successfully.
    load_observer2.Wait();
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }
}

// TODO(nasko): Disable this test until out-of-process iframes is ready and the
// security checks are back in place.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DISABLED_CrossSiteIframeRedirectTwice) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  ASSERT_TRUE(https_server.Start());

  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  GURL https_url(https_server.GetURL("/title1.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  TestNavigationObserver observer(shell()->web_contents());
  {
    // Load client-redirect page pointing to a cross-site client-redirect page,
    // which eventually redirects back to same-site page.
    GURL client_redirect_https_url(
        https_server.GetURL("/client-redirect?" + http_url.spec()));
    GURL client_redirect_http_url(embedded_test_server()->GetURL(
        "/client-redirect?" + client_redirect_https_url.spec()));

    // We should wait until second client redirect get cancelled.
    RedirectNotificationObserver load_observer2(
        NOTIFICATION_LOAD_STOP,
        Source<NavigationController>(
            &shell()->web_contents()->GetController()));

    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    client_redirect_http_url));

    // DidFailProvisionalLoad when navigating to client_redirect_https_url.
    load_observer2.Wait();
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site server-redirect page,
    // which eventually redirect back to same-site page.
    GURL server_redirect_https_url(
        https_server.GetURL("/server-redirect?" + http_url.spec()));
    GURL server_redirect_http_url(embedded_test_server()->GetURL(
        "/server-redirect?" + server_redirect_https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));
    EXPECT_EQ(observer.last_navigation_url(), http_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site server-redirect page,
    // which eventually redirects back to cross-site page.
    GURL server_redirect_https_url(
        https_server.GetURL("/server-redirect?" + https_url.spec()));
    GURL server_redirect_http_url(embedded_test_server()->GetURL(
        "/server-redirect?" + server_redirect_https_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));

    // DidFailProvisionalLoad when navigating to https_url.
    EXPECT_EQ(observer.last_navigation_url(), https_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }

  {
    // Load server-redirect page pointing to a cross-site client-redirect page,
    // which eventually redirects back to same-site page.
    GURL client_redirect_http_url(
        https_server.GetURL("/client-redirect?" + http_url.spec()));
    GURL server_redirect_http_url(embedded_test_server()->GetURL(
        "/server-redirect?" + client_redirect_http_url.spec()));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "test",
                                    server_redirect_http_url));

    // DidFailProvisionalLoad when navigating to client_redirect_http_url.
    EXPECT_EQ(observer.last_navigation_url(), client_redirect_http_url);
    EXPECT_FALSE(observer.last_navigation_succeeded());
  }
}

// Ensure that when navigating a frame cross-process RenderFrameProxyHosts are
// created in the FrameTree skipping the subtree of the navigating frame.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       ProxyCreationSkipsSubtree) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a(a,a(a)))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_TRUE(root->child_at(1) != NULL);
  EXPECT_EQ(2U, root->child_at(1)->child_count());

  {
    // Load same-site page into iframe.
    TestNavigationObserver observer(shell()->web_contents());
    GURL http_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    NavigateFrameToURL(root->child_at(0), http_url);
    EXPECT_EQ(http_url, observer.last_navigation_url());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(
        " Site A\n"
        "   |--Site A\n"
        "   +--Site A\n"
        "        |--Site A\n"
        "        +--Site A\n"
        "             +--Site A\n"
        "Where A = http://a.com/",
        DepictFrameTree(root));
  }

  // Create the cross-site URL to navigate to.
  GURL cross_site_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/title2.html");

  // Load cross-site page into the second iframe without waiting for the
  // navigation to complete. Once LoadURLWithParams returns, we would expect
  // proxies to have been created in the frame tree, but children of the
  // navigating frame to still be present. The reason is that we don't run the
  // message loop, so no IPCs that alter the frame tree can be processed.
  FrameTreeNode* child = root->child_at(1);
  SiteInstance* site = NULL;
  bool browser_side_navigation = IsBrowserSideNavigationEnabled();
  std::string cross_site_rfh_type =
      browser_side_navigation ? "speculative" : "pending";
  {
    TestNavigationObserver observer(shell()->web_contents());
    TestFrameNavigationObserver navigation_observer(child);
    NavigationController::LoadURLParams params(cross_site_url);
    params.transition_type = PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = child->frame_tree_node_id();
    child->navigator()->GetController()->LoadURLWithParams(params);

    if (browser_side_navigation) {
      site = child->render_manager()
                 ->speculative_frame_host()
                 ->GetSiteInstance();
    } else {
      site = child->render_manager()->pending_frame_host()->GetSiteInstance();
    }
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site);

    std::string tree = base::StringPrintf(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   +--Site A (B %s)\n"
        "        |--Site A\n"
        "        +--Site A\n"
        "             +--Site A\n"
        "Where A = http://a.com/\n"
        "      B = http://foo.com/",
        cross_site_rfh_type.c_str());
    EXPECT_EQ(tree, DepictFrameTree(root));

    // Now that the verification is done, run the message loop and wait for the
    // navigation to complete.
    navigation_observer.Wait();
    EXPECT_FALSE(child->render_manager()->pending_frame_host());
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(cross_site_url, observer.last_navigation_url());

    EXPECT_EQ(
        " Site A ------------ proxies for B\n"
        "   |--Site A ------- proxies for B\n"
        "   +--Site B ------- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://foo.com/",
        DepictFrameTree(root));
  }

  // Load another cross-site page into the same iframe.
  cross_site_url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  {
    // Perform the same checks as the first cross-site navigation, since
    // there have been issues in subsequent cross-site navigations. Also ensure
    // that the SiteInstance has properly changed.
    // TODO(nasko): Once we have proper cleanup of resources, add code to
    // verify that the intermediate SiteInstance/RenderFrameHost have been
    // properly cleaned up.
    TestNavigationObserver observer(shell()->web_contents());
    TestFrameNavigationObserver navigation_observer(child);
    NavigationController::LoadURLParams params(cross_site_url);
    params.transition_type = PageTransitionFromInt(ui::PAGE_TRANSITION_LINK);
    params.frame_tree_node_id = child->frame_tree_node_id();
    child->navigator()->GetController()->LoadURLWithParams(params);

    SiteInstance* site2;
    if (browser_side_navigation) {
      site2 = child->render_manager()
                  ->speculative_frame_host()
                  ->GetSiteInstance();
    } else {
      site2 = child->render_manager()->pending_frame_host()->GetSiteInstance();
    }
    EXPECT_NE(shell()->web_contents()->GetSiteInstance(), site2);
    EXPECT_NE(site, site2);

    std::string tree = base::StringPrintf(
        " Site A ------------ proxies for B C\n"
        "   |--Site A ------- proxies for B C\n"
        "   +--Site B (C %s) -- proxies for A\n"
        "Where A = http://a.com/\n"
        "      B = http://foo.com/\n"
        "      C = http://bar.com/",
        cross_site_rfh_type.c_str());
    EXPECT_EQ(tree, DepictFrameTree(root));

    navigation_observer.Wait();
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(cross_site_url, observer.last_navigation_url());
    EXPECT_EQ(0U, child->child_count());
  }
}

// Verify that "scrolling" property on frame elements propagates to child frames
// correctly.
// Does not work on android since android has scrollbars overlayed.
// TODO(bokan): Pretty soon most/all platforms will use overlay scrollbars. This
// test should find a better way to check for scrollability. crbug.com/662196.
#if defined(OS_ANDROID)
#define MAYBE_FrameOwnerPropertiesPropagationScrolling \
        DISABLED_FrameOwnerPropertiesPropagationScrolling
#else
#define MAYBE_FrameOwnerPropertiesPropagationScrolling \
        FrameOwnerPropertiesPropagationScrolling
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       MAYBE_FrameOwnerPropertiesPropagationScrolling) {
#if defined(OS_MACOSX)
  ui::test::ScopedPreferredScrollerStyle scroller_style_override(false);
#endif
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_owner_properties_scrolling.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);

  // If the available client width within the iframe is smaller than the
  // frame element's width, we assume there's a scrollbar.
  // Also note that just comparing clientHeight and scrollHeight of the frame's
  // document will not work.
  auto has_scrollbar = [](RenderFrameHostImpl* rfh) {
    int client_width;
    EXPECT_TRUE(ExecuteScriptAndExtractInt(rfh,
        "window.domAutomationController.send(document.body.clientWidth);",
        &client_width));
    const int kFrameElementWidth = 200;
    return client_width < kFrameElementWidth;
  };

  auto set_scrolling_property = [](RenderFrameHostImpl* parent_rfh,
                                   const std::string& value) {
    EXPECT_TRUE(ExecuteScript(
        parent_rfh,
        base::StringPrintf(
            "document.getElementById('child-1').setAttribute("
            "    'scrolling', '%s');", value.c_str())));
  };

  // Run the test over variety of parent/child cases.
  GURL urls[] = {
    // Remote to remote.
    embedded_test_server()->GetURL("c.com", "/tall_page.html"),
    // Remote to local.
    embedded_test_server()->GetURL("a.com", "/tall_page.html"),
    // Local to remote.
    embedded_test_server()->GetURL("b.com", "/tall_page.html")
  };
  const std::string scrolling_values[] = {
    "yes", "auto", "no"
  };

  for (size_t i = 0; i < arraysize(scrolling_values); ++i) {
    bool expect_scrollbar = scrolling_values[i] != "no";
    set_scrolling_property(root->current_frame_host(), scrolling_values[i]);
    for (size_t j = 0; j < arraysize(urls); ++j) {
      NavigateFrameToURL(child, urls[j]);
      EXPECT_EQ(expect_scrollbar, has_scrollbar(child->current_frame_host()));
    }
  }
}

// Verify that "marginwidth" and "marginheight" properties on frame elements
// propagate to child frames correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       FrameOwnerPropertiesPropagationMargin) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_owner_properties_margin.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);

  std::string margin_width;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child,
      "window.domAutomationController.send("
      "document.body.getAttribute('marginwidth'));",
      &margin_width));
  EXPECT_EQ("10", margin_width);

  std::string margin_height;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      child,
      "window.domAutomationController.send("
      "document.body.getAttribute('marginheight'));",
      &margin_height));
  EXPECT_EQ("50", margin_height);

  // Run the test over variety of parent/child cases.
  GURL urls[] = {
    // Remote to remote.
    embedded_test_server()->GetURL("c.com", "/title2.html"),
    // Remote to local.
    embedded_test_server()->GetURL("a.com", "/title1.html"),
    // Local to remote.
    embedded_test_server()->GetURL("b.com", "/title2.html")
  };

  int current_margin_width = 15;
  int current_margin_height = 25;

  // Before each navigation, we change the marginwidth and marginheight
  // properties of the frame. We then check whether those properties are applied
  // correctly after the navigation has completed.
  for (size_t i = 0; i < arraysize(urls); ++i) {
    // Change marginwidth and marginheight before navigating.
    EXPECT_TRUE(ExecuteScript(
        root,
        base::StringPrintf("document.getElementById('child-1').setAttribute("
                           "    'marginwidth', '%d');",
                           current_margin_width)));
    EXPECT_TRUE(ExecuteScript(
        root,
        base::StringPrintf("document.getElementById('child-1').setAttribute("
                           "    'marginheight', '%d');",
                           current_margin_height)));

    NavigateFrameToURL(child, urls[i]);

    std::string actual_margin_width;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        child,
        "window.domAutomationController.send("
        "document.body.getAttribute('marginwidth'));",
        &actual_margin_width));
    EXPECT_EQ(base::IntToString(current_margin_width), actual_margin_width);

    std::string actual_margin_height;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        child,
        "window.domAutomationController.send("
        "document.body.getAttribute('marginheight'));",
        &actual_margin_height));
    EXPECT_EQ(base::IntToString(current_margin_height), actual_margin_height);

    current_margin_width += 5;
    current_margin_height += 10;
  }
}

// Verify that "csp" property on frame elements propagates to child frames
// correctly. See  https://crbug.com/647588
IN_PROC_BROWSER_TEST_F(SitePerProcessEmbedderCSPEnforcementBrowserTest,
                       FrameOwnerPropertiesPropagationCSP) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_owner_properties_csp.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);

  std::string csp;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root,
      "window.domAutomationController.send("
      "document.getElementById('child-1').getAttribute('csp'));",
      &csp));
  EXPECT_EQ("object-src \'none\'", csp);

  // Run the test over variety of parent/child cases.
  GURL urls[] = {// Remote to remote.
                 embedded_test_server()->GetURL("c.com", "/title2.html"),
                 // Remote to local.
                 embedded_test_server()->GetURL("a.com", "/title1.html"),
                 // Local to remote.
                 embedded_test_server()->GetURL("b.com", "/title2.html")};

  std::vector<std::string> csp_values = {"default-src a.com",
                                         "default-src b.com", "img-src c.com"};

  // Before each navigation, we change the csp property of the frame.
  // We then check whether that property is applied
  // correctly after the navigation has completed.
  for (size_t i = 0; i < arraysize(urls); ++i) {
    // Change csp before navigating.
    EXPECT_TRUE(ExecuteScript(
        root,
        base::StringPrintf("document.getElementById('child-1').setAttribute("
                           "    'csp', '%s');",
                           csp_values[i].c_str())));

    NavigateFrameToURL(child, urls[i]);
    EXPECT_EQ(csp_values[i], child->frame_owner_properties().required_csp);
    // TODO(amalika): add checks that the CSP replication takes effect
  }
}

// Verify origin replication with an A-embed-B-embed-C-embed-A hierarchy.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, OriginReplication) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c(a),b), a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"       // tiptop_child
      "   |    |--Site C -- proxies for A B\n"       // middle_child
      "   |    |    +--Site A -- proxies for B C\n"  // lowest_child
      "   |    +--Site B -- proxies for A C\n"
      "   +--Site A ------- proxies for B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));

  std::string a_origin = embedded_test_server()->GetURL("a.com", "/").spec();
  std::string b_origin = embedded_test_server()->GetURL("b.com", "/").spec();
  std::string c_origin = embedded_test_server()->GetURL("c.com", "/").spec();
  FrameTreeNode* tiptop_child = root->child_at(0);
  FrameTreeNode* middle_child = root->child_at(0)->child_at(0);
  FrameTreeNode* lowest_child = root->child_at(0)->child_at(0)->child_at(0);

  // Check that b.com frame's location.ancestorOrigins contains the correct
  // origin for the parent.  The origin should have been replicated as part of
  // the mojom::Renderer::CreateView message that created the parent's
  // RenderFrameProxy in b.com's process.
  int ancestor_origins_length = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      tiptop_child,
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(1, ancestor_origins_length);
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      tiptop_child,
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &result));
  EXPECT_EQ(a_origin, result + "/");

  // Check that c.com frame's location.ancestorOrigins contains the correct
  // origin for its two ancestors. The topmost parent origin should be
  // replicated as part of mojom::Renderer::CreateView, and the middle frame
  // (b.com's) origin should be replicated as part of
  // mojom::Renderer::CreateFrameProxy sent for b.com's frame in c.com's
  // process.
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      middle_child,
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(2, ancestor_origins_length);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      middle_child,
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &result));
  EXPECT_EQ(b_origin, result + "/");
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      middle_child,
      "window.domAutomationController.send(location.ancestorOrigins[1]);",
      &result));
  EXPECT_EQ(a_origin, result + "/");

  // Check that the nested a.com frame's location.ancestorOrigins contains the
  // correct origin for its three ancestors.
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      lowest_child,
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(3, ancestor_origins_length);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      lowest_child,
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &result));
  EXPECT_EQ(c_origin, result + "/");
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      lowest_child,
      "window.domAutomationController.send(location.ancestorOrigins[1]);",
      &result));
  EXPECT_EQ(b_origin, result + "/");
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      lowest_child,
      "window.domAutomationController.send(location.ancestorOrigins[2]);",
      &result));
  EXPECT_EQ(a_origin, result + "/");
}

// Check that iframe sandbox flags are replicated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, SandboxFlagsReplication) {
  GURL main_url(embedded_test_server()->GetURL("/sandboxed_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Navigate the second (sandboxed) subframe to a cross-site page with a
  // subframe.
  GURL foo_url(
      embedded_test_server()->GetURL("foo.com", "/frame_tree/1-1.html"));
  NavigateFrameToURL(root->child_at(1), foo_url);
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // We can't use a TestNavigationObserver to verify the URL here,
  // since the frame has children that may have clobbered it in the observer.
  EXPECT_EQ(foo_url, root->child_at(1)->current_url());

  // Load cross-site page into subframe's subframe.
  ASSERT_EQ(2U, root->child_at(1)->child_count());
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(1)->child_at(0), bar_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(bar_url, observer.last_navigation_url());

  // Opening a popup in the sandboxed foo.com iframe should fail.
  bool success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(root->child_at(1),
                                  "window.domAutomationController.send("
                                  "!window.open('data:text/html,dataurl'));",
                                  &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Opening a popup in a frame whose parent is sandboxed should also fail.
  // Here, bar.com frame's sandboxed parent frame is a remote frame in
  // bar.com's process.
  success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(root->child_at(1)->child_at(0),
                                  "window.domAutomationController.send("
                                  "!window.open('data:text/html,dataurl'));",
                                  &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Same, but now try the case where bar.com frame's sandboxed parent is a
  // local frame in bar.com's process.
  success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(root->child_at(2)->child_at(0),
                                  "window.domAutomationController.send("
                                  "!window.open('data:text/html,dataurl'));",
                                  &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Check that foo.com frame's location.ancestorOrigins contains the correct
  // origin for the parent, which should be unaffected by sandboxing.
  int ancestor_origins_length = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root->child_at(1),
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(1, ancestor_origins_length);
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root->child_at(1),
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &result));
  EXPECT_EQ(result + "/", main_url.GetOrigin().spec());

  // Now check location.ancestorOrigins for the bar.com frame. The middle frame
  // (foo.com's) origin should be unique, since that frame is sandboxed, and
  // the top frame should match |main_url|.
  FrameTreeNode* bottom_child = root->child_at(1)->child_at(0);
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      bottom_child,
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(2, ancestor_origins_length);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      bottom_child,
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &result));
  EXPECT_EQ("null", result);
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      bottom_child,
      "window.domAutomationController.send(location.ancestorOrigins[1]);",
      &result));
  EXPECT_EQ(main_url.GetOrigin().spec(), result + "/");
}

// Check that dynamic updates to iframe sandbox flags are propagated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, DynamicSandboxFlags) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure first frame starts out at the correct cross-site page.
  EXPECT_EQ(embedded_test_server()->GetURL("bar.com", "/title1.html"),
            root->child_at(0)->current_url());

  // Navigate second frame to another cross-site page.
  GURL baz_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(1), baz_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(baz_url, observer.last_navigation_url());

  // Both frames should not be sandboxed to start with.
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(0)->pending_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(0)->effective_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(1)->pending_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(1)->effective_sandbox_flags());

  // Dynamically update sandbox flags for the first frame.
  EXPECT_TRUE(ExecuteScript(
      shell(),
      "document.querySelector('iframe').sandbox='allow-scripts';"));

  // Check that updated sandbox flags are propagated to browser process.
  // The new flags should be reflected in pending_sandbox_flags(), while
  // effective_sandbox_flags() should still reflect the old flags, because
  // sandbox flag updates take place only after navigations. "allow-scripts"
  // resets both SandboxFlags::Scripts and SandboxFlags::AutomaticFeatures bits
  // per blink::parseSandboxPolicy().
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
      ~blink::WebSandboxFlags::kAutomaticFeatures;
  EXPECT_EQ(expected_flags, root->child_at(0)->pending_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(0)->effective_sandbox_flags());

  // Navigate the first frame to a page on the same site.  The new sandbox
  // flags should take effect.
  GURL bar_url(
      embedded_test_server()->GetURL("bar.com", "/frame_tree/2-4.html"));
  NavigateFrameToURL(root->child_at(0), bar_url);
  // (The new page has a subframe; wait for it to load as well.)
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(bar_url, root->child_at(0)->current_url());
  ASSERT_EQ(1U, root->child_at(0)->child_count());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   |    +--Site B -- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  // Confirm that the browser process has updated the frame's current sandbox
  // flags.
  EXPECT_EQ(expected_flags, root->child_at(0)->pending_sandbox_flags());
  EXPECT_EQ(expected_flags, root->child_at(0)->effective_sandbox_flags());

  // Opening a popup in the now-sandboxed frame should fail.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0),
      "window.domAutomationController.send("
      "    !window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Navigate the child of the now-sandboxed frame to a page on baz.com.  The
  // child should inherit the latest sandbox flags from its parent frame, which
  // is currently a proxy in baz.com's renderer process.  This checks that the
  // proxies of |root->child_at(0)| were also updated with the latest sandbox
  // flags.
  GURL baz_child_url(embedded_test_server()->GetURL("baz.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0)->child_at(0), baz_child_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(baz_child_url, observer.last_navigation_url());

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   |    +--Site C -- proxies for A B\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/\n"
      "      C = http://baz.com/",
      DepictFrameTree(root));

  // Opening a popup in the child of a sandboxed frame should fail.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0)->child_at(0),
      "window.domAutomationController.send("
      "    !window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Child of a sandboxed frame should also be sandboxed on the browser side.
  EXPECT_EQ(expected_flags,
            root->child_at(0)->child_at(0)->effective_sandbox_flags());
}

// Check that dynamic updates to iframe sandbox flags are propagated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DynamicSandboxFlagsRemoteToLocal) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(2U, root->child_count());

  // Make sure the two frames starts out at correct URLs.
  EXPECT_EQ(embedded_test_server()->GetURL("bar.com", "/title1.html"),
            root->child_at(0)->current_url());
  EXPECT_EQ(embedded_test_server()->GetURL("/title1.html"),
            root->child_at(1)->current_url());

  // Update the second frame's sandbox flags.
  EXPECT_TRUE(ExecuteScript(
      shell(),
      "document.querySelectorAll('iframe')[1].sandbox='allow-scripts'"));

  // Check that the current sandbox flags are updated but the effective
  // sandbox flags are not.
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
      ~blink::WebSandboxFlags::kAutomaticFeatures;
  EXPECT_EQ(expected_flags, root->child_at(1)->pending_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(1)->effective_sandbox_flags());

  // Navigate the second subframe to a page on bar.com.  This will trigger a
  // remote-to-local frame swap in bar.com's process.
  GURL bar_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_one_frame.html"));
  NavigateFrameToURL(root->child_at(1), bar_url);
  EXPECT_EQ(bar_url, root->child_at(1)->current_url());
  ASSERT_EQ(1U, root->child_at(1)->child_count());

  // Confirm that the browser process has updated the current sandbox flags.
  EXPECT_EQ(expected_flags, root->child_at(1)->pending_sandbox_flags());
  EXPECT_EQ(expected_flags, root->child_at(1)->effective_sandbox_flags());

  // Opening a popup in the sandboxed second frame should fail.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(1),
      "window.domAutomationController.send("
      "    !window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());

  // Make sure that the child frame inherits the sandbox flags of its
  // now-sandboxed parent frame.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(1)->child_at(0),
      "window.domAutomationController.send("
      "    !window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());
}

// Check that dynamic updates to iframe sandbox flags are propagated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DynamicSandboxFlagsRendererInitiatedNavigation) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());
  ASSERT_EQ(1U, root->child_count());

  // Make sure the frame starts out at the correct cross-site page.
  EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title1.html"),
            root->child_at(0)->current_url());

  // The frame should not be sandboxed to start with.
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(0)->pending_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(0)->effective_sandbox_flags());

  // Dynamically update the frame's sandbox flags.
  EXPECT_TRUE(ExecuteScript(
      shell(), "document.querySelector('iframe').sandbox='allow-scripts';"));

  // Check that updated sandbox flags are propagated to browser process.
  // The new flags should be set in pending_sandbox_flags(), while
  // effective_sandbox_flags() should still reflect the old flags, because
  // sandbox flag updates take place only after navigations. "allow-scripts"
  // resets both SandboxFlags::Scripts and SandboxFlags::AutomaticFeatures bits
  // per blink::parseSandboxPolicy().
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
      ~blink::WebSandboxFlags::kAutomaticFeatures;
  EXPECT_EQ(expected_flags, root->child_at(0)->pending_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(0)->effective_sandbox_flags());

  // Perform a renderer-initiated same-site navigation in the first frame. The
  // new sandbox flags should take effect.
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  ASSERT_TRUE(
      ExecuteScript(root->child_at(0), "window.location.href='/title2.html'"));
  frame_observer.Wait();
  EXPECT_EQ(embedded_test_server()->GetURL("baz.com", "/title2.html"),
            root->child_at(0)->current_url());

  // Confirm that the browser process has updated the frame's current sandbox
  // flags.
  EXPECT_EQ(expected_flags, root->child_at(0)->pending_sandbox_flags());
  EXPECT_EQ(expected_flags, root->child_at(0)->effective_sandbox_flags());

  // Opening a popup in the now-sandboxed frame should fail.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0),
      "window.domAutomationController.send("
      "    !window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());
}

// Verify that when a new child frame is added, the proxies created for it in
// other SiteInstances have correct sandbox flags and origin.
//
//     A         A           A
//    /         / \         / \    .
//   B    ->   B   A   ->  B   A
//                              \  .
//                               B
//
// The test checks sandbox flags and origin for the proxy added in step 2, by
// checking whether the grandchild frame added in step 3 sees proper sandbox
// flags and origin for its (remote) parent.  This wasn't addressed when
// https://crbug.com/423587 was fixed.
// TODO(alexmos): Re-enable when https://crbug.com/610893 is fixed.
IN_PROC_BROWSER_TEST_F(
    SitePerProcessBrowserTest,
    DISABLED_ProxiesForNewChildFramesHaveCorrectReplicationState) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // In the root frame, add a new sandboxed local frame, which itself has a
  // child frame on baz.com.  Wait for three RenderFrameHosts to be created:
  // the new sandboxed local frame, its child (while it's still local), and a
  // pending RFH when starting the cross-site navigation to baz.com.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 3);
  EXPECT_TRUE(ExecuteScript(root,
                            "addFrame('/frame_tree/page_with_one_frame.html',"
                            "         'allow-scripts allow-same-origin'))"));
  frame_observer.Wait();

  // Wait for the cross-site navigation to baz.com in the grandchild to finish.
  FrameTreeNode* bottom_child = root->child_at(1)->child_at(0);
  TestFrameNavigationObserver navigation_observer(bottom_child);
  navigation_observer.Wait();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://baz.com/",
      DepictFrameTree(root));

  // Use location.ancestorOrigins to check that the grandchild on baz.com sees
  // correct origin for its parent.
  int ancestor_origins_length = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      bottom_child,
      "window.domAutomationController.send(location.ancestorOrigins.length);",
      &ancestor_origins_length));
  EXPECT_EQ(2, ancestor_origins_length);
  std::string parent_origin;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      bottom_child,
      "window.domAutomationController.send(location.ancestorOrigins[0]);",
      &parent_origin));
  EXPECT_EQ(main_url.GetOrigin().spec(), parent_origin + "/");

  // Check that the sandbox flags in the browser process are correct.
  // "allow-scripts" resets both WebSandboxFlags::Scripts and
  // WebSandboxFlags::AutomaticFeatures bits per blink::parseSandboxPolicy().
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
      ~blink::WebSandboxFlags::kAutomaticFeatures &
      ~blink::WebSandboxFlags::kOrigin;
  EXPECT_EQ(expected_flags, root->child_at(1)->effective_sandbox_flags());

  // The child of the sandboxed frame should've inherited sandbox flags, so it
  // should not be able to create popups.
  EXPECT_EQ(expected_flags, bottom_child->effective_sandbox_flags());
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      bottom_child,
      "window.domAutomationController.send("
      "    !window.open('data:text/html,dataurl'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(1u, Shell::windows().size());
}

// Verify that a child frame can retrieve the name property set by its parent.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, WindowNameReplication) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/2-4.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  GURL frame_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/3-1.html");
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // Ensure that a new process is created for the subframe.
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Check that the window.name seen by the frame matches the name attribute
  // specified by its parent in the iframe tag.
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root->child_at(0), "window.domAutomationController.send(window.name);",
      &result));
  EXPECT_EQ("3-1-name", result);
}

// Verify that dynamic updates to a frame's window.name propagate to the
// frame's proxies, so that the latest frame names can be used in navigations.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, DynamicWindowName) {
  GURL main_url(embedded_test_server()->GetURL("/frame_tree/2-4.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  GURL frame_url =
      embedded_test_server()->GetURL("foo.com", "/frame_tree/3-1.html");
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // Browser process should know the child frame's original window.name
  // specified in the iframe element.
  EXPECT_EQ(root->child_at(0)->frame_name(), "3-1-name");

  // Update the child frame's window.name.
  EXPECT_TRUE(
      ExecuteScript(root->child_at(0), "window.name = 'updated-name';"));

  // The change should propagate to the browser process.
  EXPECT_EQ(root->child_at(0)->frame_name(), "updated-name");

  // The proxy in the parent process should also receive the updated name.
  // Now iframe's name and the content window's name differ, so it shouldn't
  // be possible to access to the content window with the updated name.
  bool success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(shell(),
                                  "window.domAutomationController.send("
                                  "    frames['updated-name'] === undefined);",
                                  &success));
  // TODO(yukishiino): The following expectation should be TRUE, but we're
  // intentionally disabling the name and origin check of the named access on
  // window.  See also crbug.com/538562 and crbug.com/701489.
  EXPECT_FALSE(success);
  // Change iframe's name to match the content window's name so that it can
  // reference the child frame by its new name in case of cross origin.
  EXPECT_TRUE(
      ExecuteScript(root, "window['3-1-id'].name = 'updated-name';"));
  success = false;
  EXPECT_TRUE(
      ExecuteScriptAndExtractBool(shell(),
                                  "window.domAutomationController.send("
                                  "    frames['updated-name'] == frames[0]);",
                                  &success));
  EXPECT_TRUE(success);

  // Issue a renderer-initiated navigation from the root frame to the child
  // frame using the frame's name. Make sure correct frame is navigated.
  //
  // TODO(alexmos): When blink::createWindow is refactored to handle
  // RemoteFrames, this should also be tested via window.open(url, frame_name)
  // and a more complicated frame hierarchy (https://crbug.com/463742)
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  EXPECT_TRUE(ExecuteScript(
      shell(),
      base::StringPrintf("frames['updated-name'].location.href = '%s';",
                         foo_url.spec().c_str())));
  frame_observer.Wait();
  EXPECT_EQ(foo_url, root->child_at(0)->current_url());
}

// Verify that when a frame is navigated to a new origin, the origin update
// propagates to the frame's proxies.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, OriginUpdatesReachProxies) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));

  // Navigate second subframe to a baz.com.  This should send an origin update
  // to the frame's proxy in the bar.com (first frame's) process.
  GURL frame_url = embedded_test_server()->GetURL("baz.com", "/title2.html");
  NavigateFrameToURL(root->child_at(1), frame_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(frame_url, observer.last_navigation_url());

  // The first frame can't directly observe the second frame's origin with
  // JavaScript.  Instead, try to navigate the second frame from the first
  // frame.  This should fail with a console error message, which should
  // contain the second frame's updated origin (see blink::Frame::canNavigate).
  std::unique_ptr<ConsoleObserverDelegate> console_delegate(
      new ConsoleObserverDelegate(
          shell()->web_contents(),
          "Unsafe JavaScript attempt to initiate navigation*"));
  shell()->web_contents()->SetDelegate(console_delegate.get());

  // frames[1] can't be used due to a bug where RemoteFrames are created out of
  // order (https://crbug.com/478792).  Instead, target second frame by name.
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "try { parent.frames['frame2'].location.href = "
                            "'data:text/html,foo'; } catch (e) {}"));
  console_delegate->Wait();

  std::string frame_origin = root->child_at(1)->current_origin().Serialize();
  EXPECT_EQ(frame_origin + "/", frame_url.GetOrigin().spec());
  EXPECT_TRUE(
      base::MatchPattern(console_delegate->message(), "*" + frame_origin + "*"))
      << "Error message does not contain the frame's latest origin ("
      << frame_origin << ")";
}

// Ensure that navigating subframes in --site-per-process mode properly fires
// the DidStopLoading event on WebContentsObserver.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrossSiteDidStopLoading) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load same-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL http_url(embedded_test_server()->GetURL("/title1.html"));
  NavigateFrameToURL(child, http_url);
  EXPECT_EQ(http_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Load cross-site page into iframe.
  TestNavigationObserver nav_observer(shell()->web_contents(), 1);
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = child->frame_tree_node_id();
  child->navigator()->GetController()->LoadURLWithParams(params);
  nav_observer.Wait();

  // Verify that the navigation succeeded and the expected URL was loaded.
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
}

// Ensure that the renderer does not crash when navigating a frame that has a
// sibling RemoteFrame.  See https://crbug.com/426953.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateWithSiblingRemoteFrame) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  TestNavigationObserver observer(shell()->web_contents());

  // Make sure the first frame is out of process.
  ASSERT_EQ(2U, root->child_count());
  FrameTreeNode* node2 = root->child_at(0);
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            node2->current_frame_host()->GetSiteInstance());

  // Make sure the second frame is in the parent's process.
  FrameTreeNode* node3 = root->child_at(1);
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());

  // Navigate the second iframe (node3) to a URL in its own process.
  GURL title_url = embedded_test_server()->GetURL("/title2.html");
  NavigateFrameToURL(node3, title_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(title_url, observer.last_navigation_url());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(node3->current_frame_host()->IsRenderFrameLive());
}

// Ensure that the renderer does not crash when a local frame with a remote
// parent frame is swapped from local to remote, then back to local again.
// See https://crbug.com/585654.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateSiblingsToSameProcess) {
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  FrameTreeNode* node2 = root->child_at(0);
  FrameTreeNode* node3 = root->child_at(1);

  // Navigate the second iframe to the same process as the first.
  GURL frame_url = embedded_test_server()->GetURL("bar.com", "/title1.html");
  NavigateFrameToURL(node3, frame_url);

  // Verify that they are in the same process.
  EXPECT_EQ(node2->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());

  // Navigate the first iframe into its parent's process.
  GURL title_url = embedded_test_server()->GetURL("/title2.html");
  NavigateFrameToURL(node2, title_url);
  EXPECT_NE(node2->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());

  // Return the first iframe to the same process as its sibling, and ensure
  // that it does not crash.
  NavigateFrameToURL(node2, frame_url);
  EXPECT_EQ(node2->current_frame_host()->GetSiteInstance(),
            node3->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(node2->current_frame_host()->IsRenderFrameLive());
}

// Verify that load events for iframe elements work when the child frame is
// out-of-process.  In such cases, the load event is forwarded from the child
// frame to the parent frame via the browser process.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, LoadEventForwarding) {
  // Load a page with a cross-site frame.  The parent page has an onload
  // handler in the iframe element that appends "LOADED" to the document title.
  {
    GURL main_url(
        embedded_test_server()->GetURL("/frame_with_load_event.html"));
    base::string16 expected_title(base::UTF8ToUTF16("LOADED"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    EXPECT_TRUE(NavigateToURL(shell(), main_url));
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
  }

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Load another cross-site page into the iframe and check that the load event
  // is fired.
  {
    GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
    base::string16 expected_title(base::UTF8ToUTF16("LOADEDLOADED"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);
    TestNavigationObserver observer(shell()->web_contents());
    NavigateFrameToURL(root->child_at(0), foo_url);
    EXPECT_TRUE(observer.last_navigation_succeeded());
    EXPECT_EQ(foo_url, observer.last_navigation_url());
    EXPECT_EQ(title_watcher.WaitAndGetTitle(), expected_title);
  }
}

// Check that postMessage can be routed between cross-site iframes.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, SubframePostMessage) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_post_message_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  ASSERT_EQ(2U, root->child_count());

  // Verify the frames start at correct URLs.  First frame should be
  // same-site; second frame should be cross-site.
  GURL same_site_url(embedded_test_server()->GetURL("/post_message.html"));
  EXPECT_EQ(same_site_url, root->child_at(0)->current_url());
  GURL foo_url(embedded_test_server()->GetURL("foo.com",
                                              "/post_message.html"));
  EXPECT_EQ(foo_url, root->child_at(1)->current_url());
  EXPECT_NE(root->child_at(0)->current_frame_host()->GetSiteInstance(),
            root->child_at(1)->current_frame_host()->GetSiteInstance());

  // Send a message from first, same-site frame to second, cross-site frame.
  // Expect the second frame to reply back to the first frame.
  PostMessageAndWaitForReply(root->child_at(0),
                             "postToSibling('subframe-msg','subframe2')",
                             "\"done-subframe1\"");

  // Send a postMessage from second, cross-site frame to its parent.  Expect
  // parent to send a reply to the frame.
  base::string16 expected_title(base::ASCIIToUTF16("subframe-msg"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  PostMessageAndWaitForReply(root->child_at(1), "postToParent('subframe-msg')",
                             "\"done-subframe2\"");
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Verify the total number of received messages for each subframe.  First
  // frame should have one message (reply from second frame).  Second frame
  // should have two messages (message from first frame and reply from parent).
  // Parent should have one message (from second frame).
  EXPECT_EQ(1, GetReceivedMessages(root->child_at(0)));
  EXPECT_EQ(2, GetReceivedMessages(root->child_at(1)));
  EXPECT_EQ(1, GetReceivedMessages(root));
}

// Check that renderer initiated navigations which commit a new RenderFrameHost
// do not crash if the original RenderFrameHost was being covered by an
// interstitial. See crbug.com/607964.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateOpenerWithInterstitial) {
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Open a popup and navigate it to bar.com.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(web_contents(), "window.open('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();
  EXPECT_TRUE(NavigateToURL(popup, embedded_test_server()->GetURL(
                                       "bar.com", "/navigate_opener.html")));

  // Show an interstitial in the opener.
  TestInterstitialDelegate* delegate = new TestInterstitialDelegate;
  WebContentsImpl* opener_contents =
      static_cast<WebContentsImpl*>(web_contents());
  GURL interstitial_url("http://interstitial");
  InterstitialPageImpl* interstitial = new InterstitialPageImpl(
      opener_contents, static_cast<RenderWidgetHostDelegate*>(opener_contents),
      true, interstitial_url, delegate);
  interstitial->Show();
  WaitForInterstitialAttach(opener_contents);

  // Now, navigate the opener cross-process using the popup while it still has
  // an interstitial. This should not crash.
  TestNavigationObserver navigation_observer(opener_contents);
  EXPECT_TRUE(ExecuteScript(popup, "navigateOpener();"));
  navigation_observer.Wait();
}

// Check that postMessage can be sent from a subframe on a cross-process opener
// tab, and that its event.source points to a valid proxy.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       PostMessageWithSubframeOnOpenerChain) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_post_message_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  ASSERT_EQ(2U, root->child_count());

  // Verify the initial state of the world.  First frame should be same-site;
  // second frame should be cross-site.
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Open a popup from the first subframe (so that popup's window.opener points
  // to the subframe) and navigate it to bar.com.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecuteScript(root->child_at(0), "openPopup('about:blank');"));
  Shell* popup = new_shell_observer.GetShell();
  GURL popup_url(
      embedded_test_server()->GetURL("bar.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURL(popup, popup_url));

  // From the popup, open another popup for baz.com.  This will be used to
  // check that the whole opener chain is processed when creating proxies and
  // not just an immediate opener.
  ShellAddedObserver new_shell_observer2;
  EXPECT_TRUE(ExecuteScript(popup, "openPopup('about:blank');"));
  Shell* popup2 = new_shell_observer2.GetShell();
  GURL popup2_url(
      embedded_test_server()->GetURL("baz.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURL(popup2, popup2_url));

  // Ensure that we've created proxies for SiteInstances of both popups (C, D)
  // in the main window's frame tree.
  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site A ------- proxies for B C D\n"
      "   +--Site B ------- proxies for A C D\n"
      "Where A = http://a.com/\n"
      "      B = http://foo.com/\n"
      "      C = http://bar.com/\n"
      "      D = http://baz.com/",
      DepictFrameTree(root));

  // Check the first popup's frame tree as well.  Note that it doesn't have a
  // proxy for foo.com, since foo.com can't reach the popup.  It does have a
  // proxy for its opener a.com (which can reach it via the window.open
  // reference) and second popup (which can reach it via window.opener).
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(
      " Site C ------------ proxies for A D\n"
      "Where A = http://a.com/\n"
      "      C = http://bar.com/\n"
      "      D = http://baz.com/",
      DepictFrameTree(popup_root));

  // Send a message from first subframe on main page to the first popup and
  // wait for a reply back. The reply verifies that the proxy for the opener
  // tab's subframe is targeted properly.
  PostMessageAndWaitForReply(root->child_at(0), "postToPopup('subframe-msg')",
                             "\"done-subframe1\"");

  // Send a postMessage from the popup to window.opener and ensure that it
  // reaches subframe1.  This verifies that the subframe opener information
  // propagated to the popup's RenderFrame.  Wait for subframe1 to send a reply
  // message to the popup.
  EXPECT_TRUE(ExecuteScript(popup, "window.name = 'popup';"));
  PostMessageAndWaitForReply(popup_root, "postToOpener('subframe-msg', '*')",
                             "\"done-popup\"");

  // Second a postMessage from popup2 to window.opener.opener, which should
  // resolve to subframe1.  This tests opener chains of length greater than 1.
  // As before, subframe1 will send a reply to popup2.
  FrameTreeNode* popup2_root =
      static_cast<WebContentsImpl*>(popup2->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_TRUE(ExecuteScript(popup2, "window.name = 'popup2';"));
  PostMessageAndWaitForReply(popup2_root,
                             "postToOpenerOfOpener('subframe-msg', '*')",
                             "\"done-popup2\"");

  // Verify the total number of received messages for each subframe:
  //  - 3 for first subframe (two from first popup, one from second popup)
  //  - 2 for popup (both from first subframe)
  //  - 1 for popup2 (reply from first subframe)
  //  - 0 for other frames
  EXPECT_EQ(0, GetReceivedMessages(root));
  EXPECT_EQ(3, GetReceivedMessages(root->child_at(0)));
  EXPECT_EQ(0, GetReceivedMessages(root->child_at(1)));
  EXPECT_EQ(2, GetReceivedMessages(popup_root));
  EXPECT_EQ(1, GetReceivedMessages(popup2_root));
}

// Check that parent.frames[num] references correct sibling frames when the
// parent is remote.  See https://crbug.com/478792.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, IndexedFrameAccess) {
  // Start on a page with three same-site subframes.
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/frame_tree/top.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(3U, root->child_count());
  FrameTreeNode* child0 = root->child_at(0);
  FrameTreeNode* child1 = root->child_at(1);
  FrameTreeNode* child2 = root->child_at(2);

  // Send each of the frames to a different site.  Each new renderer will first
  // create proxies for the parent and two sibling subframes and then create
  // and insert the new RenderFrame into the frame tree.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/post_message.html"));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/post_message.html"));
  GURL d_url(embedded_test_server()->GetURL("d.com", "/post_message.html"));
  NavigateFrameToURL(child0, b_url);
  NavigateFrameToURL(child1, c_url);
  NavigateFrameToURL(child2, d_url);

  EXPECT_EQ(
      " Site A ------------ proxies for B C D\n"
      "   |--Site B ------- proxies for A C D\n"
      "   |--Site C ------- proxies for A B D\n"
      "   +--Site D ------- proxies for A B C\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/\n"
      "      D = http://d.com/",
      DepictFrameTree(root));

  // Check that each subframe sees itself at correct index in parent.frames.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child0,
      "window.domAutomationController.send(window === parent.frames[0]);",
      &success));
  EXPECT_TRUE(success);

  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child1,
      "window.domAutomationController.send(window === parent.frames[1]);",
      &success));
  EXPECT_TRUE(success);

  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      child2,
      "window.domAutomationController.send(window === parent.frames[2]);",
      &success));
  EXPECT_TRUE(success);

  // Send a postMessage from B to parent.frames[1], which should go to C, and
  // wait for reply.
  PostMessageAndWaitForReply(child0, "postToSibling('subframe-msg', 1)",
                             "\"done-1-1-name\"");

  // Send a postMessage from C to parent.frames[2], which should go to D, and
  // wait for reply.
  PostMessageAndWaitForReply(child1, "postToSibling('subframe-msg', 2)",
                             "\"done-1-2-name\"");

  // Verify the total number of received messages for each subframe.
  EXPECT_EQ(1, GetReceivedMessages(child0));
  EXPECT_EQ(2, GetReceivedMessages(child1));
  EXPECT_EQ(1, GetReceivedMessages(child2));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, RFPHDestruction) {
  GURL main_url(embedded_test_server()->GetURL("/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  // Load cross-site page into iframe.
  FrameTreeNode* child = root->child_at(0);
  GURL url = embedded_test_server()->GetURL("foo.com", "/title2.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateFrameToURL(root->child_at(0), url);
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "        |--Site A -- proxies for B\n"
      "        +--Site A -- proxies for B\n"
      "             +--Site A -- proxies for B\n"
      "Where A = http://127.0.0.1/\n"
      "      B = http://foo.com/",
      DepictFrameTree(root));

  // Load another cross-site page.
  url = embedded_test_server()->GetURL("bar.com", "/title3.html");
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    NavigateIframeToURL(shell()->web_contents(), "test", url);
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_EQ(
      " Site A ------------ proxies for C\n"
      "   |--Site C ------- proxies for A\n"
      "   +--Site A ------- proxies for C\n"
      "        |--Site A -- proxies for C\n"
      "        +--Site A -- proxies for C\n"
      "             +--Site A -- proxies for C\n"
      "Where A = http://127.0.0.1/\n"
      "      C = http://bar.com/",
      DepictFrameTree(root));

  // Navigate back to the parent's origin.
  {
    RenderFrameDeletedObserver deleted_observer(child->current_frame_host());
    url = embedded_test_server()->GetURL("/title1.html");
    NavigateFrameToURL(child, url);
    // Wait for the old process to exit, to verify that the proxies go away.
    deleted_observer.WaitUntilDeleted();
  }
  EXPECT_EQ(url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  EXPECT_EQ(
      " Site A\n"
      "   |--Site A\n"
      "   +--Site A\n"
      "        |--Site A\n"
      "        +--Site A\n"
      "             +--Site A\n"
      "Where A = http://127.0.0.1/",
      DepictFrameTree(root));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, OpenPopupWithRemoteParent) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Navigate first child cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Open a popup from the first child.
  Shell* new_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "");
  EXPECT_TRUE(new_shell);

  // Check that the popup's opener is correct on both the browser and renderer
  // sides.
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(new_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(root->child_at(0), popup_root->opener());

  std::string opener_url;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      popup_root,
      "window.domAutomationController.send(window.opener.location.href);",
      &opener_url));
  EXPECT_EQ(frame_url.spec(), opener_url);

  // Now try the same with a cross-site popup and make sure it ends up in a new
  // process and with a correct opener.
  GURL popup_url(embedded_test_server()->GetURL("c.com", "/title2.html"));
  Shell* cross_site_popup = OpenPopup(root->child_at(0), popup_url, "");
  EXPECT_TRUE(cross_site_popup);

  FrameTreeNode* cross_site_popup_root =
      static_cast<WebContentsImpl*>(cross_site_popup->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(cross_site_popup_root->current_url(), popup_url);

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            cross_site_popup->web_contents()->GetSiteInstance());
  EXPECT_NE(root->child_at(0)->current_frame_host()->GetSiteInstance(),
            cross_site_popup->web_contents()->GetSiteInstance());

  EXPECT_EQ(root->child_at(0), cross_site_popup_root->opener());

  // Ensure the popup's window.opener points to the right subframe.  Note that
  // we can't check the opener's location as above since it's cross-origin.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      cross_site_popup_root,
      "window.domAutomationController.send("
      "    window.opener === window.opener.top.frames[0]);",
      &success));
  EXPECT_TRUE(success);
}

// Test that cross-process popups can't be navigated to disallowed URLs by
// their opener.  This ensures that proper URL validation is performed when
// RenderFrameProxyHosts are navigated.  See https://crbug.com/595339.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, NavigatePopupToIllegalURL) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Open a cross-site popup.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  Shell* popup = OpenPopup(shell(), popup_url, "foo");
  EXPECT_TRUE(popup);
  EXPECT_NE(popup->web_contents()->GetSiteInstance(),
            shell()->web_contents()->GetSiteInstance());

  // From the opener, navigate the popup to a file:/// URL.  This should be
  // disallowed and result in an about:blank navigation.
  GURL file_url("file:///");
  NavigateNamedFrame(shell(), file_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(popup->web_contents()));
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            popup->web_contents()->GetLastCommittedURL());

  // Navigate popup back to a cross-site URL.
  EXPECT_TRUE(NavigateToURL(popup, popup_url));
  EXPECT_NE(popup->web_contents()->GetSiteInstance(),
            shell()->web_contents()->GetSiteInstance());

  // Now try the same test with a chrome:// URL.
  GURL chrome_url(std::string(kChromeUIScheme) + "://" +
                  std::string(kChromeUIGpuHost));
  NavigateNamedFrame(shell(), chrome_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(popup->web_contents()));
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            popup->web_contents()->GetLastCommittedURL());
}

// Verify that named frames are discoverable from their opener's ancestors.
// See https://crbug.com/511474.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DiscoverNamedFrameFromAncestorOfOpener) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Navigate first child cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Open a popup named "foo" from the first child.
  Shell* foo_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(foo_shell);

  // Check that a proxy was created for the "foo" popup in a.com.
  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetFrameTree()
          ->root();
  SiteInstance* site_instance_a = root->current_frame_host()->GetSiteInstance();
  RenderFrameProxyHost* popup_rfph_for_a =
      foo_root->render_manager()->GetRenderFrameProxyHost(site_instance_a);
  EXPECT_TRUE(popup_rfph_for_a);

  // Verify that the main frame can find the "foo" popup by name.  If
  // window.open targets the correct frame, the "foo" popup's current URL
  // should be updated to |named_frame_url|.
  GURL named_frame_url(embedded_test_server()->GetURL("c.com", "/title2.html"));
  NavigateNamedFrame(shell(), named_frame_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(foo_shell->web_contents()));
  EXPECT_EQ(named_frame_url, foo_root->current_url());

  // Navigate the popup cross-site and ensure it's still reachable via
  // window.open from the main frame.
  GURL d_url(embedded_test_server()->GetURL("d.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURL(foo_shell, d_url));
  EXPECT_EQ(d_url, foo_root->current_url());
  NavigateNamedFrame(shell(), named_frame_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(foo_shell->web_contents()));
  EXPECT_EQ(named_frame_url, foo_root->current_url());
}

// Similar to DiscoverNamedFrameFromAncestorOfOpener, but check that if a
// window is created without a name and acquires window.name later, it will
// still be discoverable from its opener's ancestors.  Also, instead of using
// an opener's ancestor, this test uses a popup with same origin as that
// ancestor. See https://crbug.com/511474.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DiscoverFrameAfterSettingWindowName) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/site_per_process_main.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a same-site popup from the main frame.
  GURL a_com_url(embedded_test_server()->GetURL("a.com", "/title3.html"));
  Shell* a_com_shell = OpenPopup(root->child_at(0), a_com_url, "");
  EXPECT_TRUE(a_com_shell);

  // Navigate first child on main frame cross-site.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Open an unnamed popup from the first child frame.
  Shell* foo_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "");
  EXPECT_TRUE(foo_shell);

  // There should be no proxy created for the "foo" popup in a.com, since
  // there's no way for the two a.com frames to access it yet.
  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetFrameTree()
          ->root();
  SiteInstance* site_instance_a = root->current_frame_host()->GetSiteInstance();
  EXPECT_FALSE(
      foo_root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // Set window.name in the popup's frame.
  EXPECT_TRUE(ExecuteScript(foo_shell, "window.name = 'foo'"));

  // A proxy for the popup should now exist in a.com.
  EXPECT_TRUE(
      foo_root->render_manager()->GetRenderFrameProxyHost(site_instance_a));

  // Verify that the a.com popup can now find the "foo" popup by name.
  GURL named_frame_url(embedded_test_server()->GetURL("c.com", "/title2.html"));
  NavigateNamedFrame(a_com_shell, named_frame_url, "foo");
  EXPECT_TRUE(WaitForLoadStop(foo_shell->web_contents()));
  EXPECT_EQ(named_frame_url, foo_root->current_url());
}

// Check that frame opener updates work with subframes.  Set up a window with a
// popup and update openers for the popup's main frame and subframe to
// subframes on first window, as follows:
//
//    foo      +---- bar
//    / \      |     / \      .
// bar   foo <-+  bar   foo
//  ^                    |
//  +--------------------+
//
// The sites are carefully set up so that both opener updates are cross-process
// but still allowed by Blink's navigation checks.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, UpdateSubframeOpener) {
  GURL main_url = embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/page_with_two_frames.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(2U, root->child_count());

  // From the top frame, open a popup and navigate it to a cross-site page with
  // two subframes.
  Shell* popup_shell = OpenPopup(shell(), GURL(url::kAboutBlankURL), "popup");
  EXPECT_TRUE(popup_shell);
  GURL popup_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_post_message_frames.html"));
  EXPECT_TRUE(NavigateToURL(popup_shell, popup_url));

  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(2U, popup_root->child_count());

  // Popup's opener should point to main frame to start with.
  EXPECT_EQ(root, popup_root->opener());

  // Update the popup's opener to the second subframe on the main page (which
  // is same-origin with the top frame, i.e., foo.com).
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(1),
      "window.domAutomationController.send(!!window.open('','popup'));",
      &success));
  EXPECT_TRUE(success);

  // Check that updated opener propagated to the browser process and the
  // popup's bar.com process.
  EXPECT_EQ(root->child_at(1), popup_root->opener());

  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      popup_shell,
      "window.domAutomationController.send("
      "    window.opener === window.opener.parent.frames['frame2']);",
      &success));
  EXPECT_TRUE(success);

  // Now update opener on the popup's second subframe (foo.com) to the main
  // page's first subframe (bar.com).
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0),
      "window.domAutomationController.send(!!window.open('','subframe2'));",
      &success));
  EXPECT_TRUE(success);

  // Check that updated opener propagated to the browser process and the
  // foo.com process.
  EXPECT_EQ(root->child_at(0), popup_root->child_at(1)->opener());

  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      popup_root->child_at(1),
      "window.domAutomationController.send("
      "    window.opener === window.opener.parent.frames['frame1']);",
      &success));
  EXPECT_TRUE(success);
}

// Check that when a subframe navigates to a new SiteInstance, the new
// SiteInstance will get a proxy for the opener of subframe's parent.  I.e.,
// accessing parent.opener from the subframe should still work after a
// cross-process navigation.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigatingSubframePreservesOpenerInParent) {
  GURL main_url = embedded_test_server()->GetURL("a.com", "/post_message.html");
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a popup with a cross-site page that has a subframe.
  GURL popup_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(b)"));
  Shell* popup_shell = OpenPopup(shell(), popup_url, "popup");
  EXPECT_TRUE(popup_shell);
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(1U, popup_root->child_count());

  // Check that the popup's opener is correct in the browser process.
  EXPECT_EQ(root, popup_root->opener());

  // Navigate popup's subframe to another site.
  GURL frame_url(embedded_test_server()->GetURL("c.com", "/post_message.html"));
  NavigateFrameToURL(popup_root->child_at(0), frame_url);

  // Check that the new subframe process still sees correct opener for its
  // parent by sending a postMessage to subframe's parent.opener.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      popup_root->child_at(0),
      "window.domAutomationController.send(!!parent.opener);", &success));
  EXPECT_TRUE(success);

  base::string16 expected_title = base::ASCIIToUTF16("msg");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      popup_root->child_at(0),
      "window.domAutomationController.send(postToOpenerOfParent('msg','*'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Check that if a subframe has an opener, that opener is preserved when the
// subframe navigates cross-site.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, NavigateSubframeWithOpener) {
  GURL main_url(embedded_test_server()->GetURL(
      "foo.com", "/frame_tree/page_with_two_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site A ------- proxies for B\n"
      "Where A = http://foo.com/\n"
      "      B = http://bar.com/",
      DepictFrameTree(root));

  // Update the first (cross-site) subframe's opener to root frame.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root, "window.domAutomationController.send(!!window.open('','frame1'));",
      &success));
  EXPECT_TRUE(success);

  // Check that updated opener propagated to the browser process and subframe's
  // process.
  EXPECT_EQ(root, root->child_at(0)->opener());

  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0),
      "window.domAutomationController.send(window.opener === window.parent);",
      &success));
  EXPECT_TRUE(success);

  // Navigate the subframe with opener to another site.
  GURL frame_url(embedded_test_server()->GetURL("baz.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Check that the subframe still sees correct opener in its new process.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(0),
      "window.domAutomationController.send(window.opener === window.parent);",
      &success));
  EXPECT_TRUE(success);

  // Navigate second subframe to a new site.  Check that the proxy that's
  // created for the first subframe in the new SiteInstance has correct opener.
  GURL frame2_url(embedded_test_server()->GetURL("qux.com", "/title1.html"));
  NavigateFrameToURL(root->child_at(1), frame2_url);

  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root->child_at(1),
      "window.domAutomationController.send("
      "    parent.frames['frame1'].opener === parent);",
      &success));
  EXPECT_TRUE(success);
}

// Check that if a subframe has an opener, that opener is preserved when a new
// RenderFrameProxy is created for that subframe in another renderer process.
// Similar to NavigateSubframeWithOpener, but this test verifies the subframe
// opener plumbing for mojom::Renderer::CreateFrameProxy(), whereas
// NavigateSubframeWithOpener targets mojom::Renderer::CreateFrame().
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NewRenderFrameProxyPreservesOpener) {
  GURL main_url(
      embedded_test_server()->GetURL("foo.com", "/post_message.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a popup with a cross-site page that has two subframes.
  GURL popup_url(embedded_test_server()->GetURL(
      "bar.com", "/frame_tree/page_with_post_message_frames.html"));
  Shell* popup_shell = OpenPopup(shell(), popup_url, "popup");
  EXPECT_TRUE(popup_shell);
  FrameTreeNode* popup_root =
      static_cast<WebContentsImpl*>(popup_shell->web_contents())
          ->GetFrameTree()
          ->root();
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site A ------- proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://bar.com/\n"
      "      B = http://foo.com/",
      DepictFrameTree(popup_root));

  // Update the popup's second subframe's opener to root frame.  This is
  // allowed because that subframe is in the same foo.com SiteInstance as the
  // root frame.
  bool success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      root,
      "window.domAutomationController.send(!!window.open('','subframe2'));",
      &success));
  EXPECT_TRUE(success);

  // Check that the opener update propagated to the browser process and bar.com
  // process.
  EXPECT_EQ(root, popup_root->child_at(1)->opener());
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      popup_root->child_at(0),
      "window.domAutomationController.send("
      "    parent.frames['subframe2'].opener && "
      "        parent.frames['subframe2'].opener === parent.opener);",
      &success));
  EXPECT_TRUE(success);

  // Navigate the popup's first subframe to another site.
  GURL frame_url(
      embedded_test_server()->GetURL("baz.com", "/post_message.html"));
  NavigateFrameToURL(popup_root->child_at(0), frame_url);

  // Check that the second subframe's opener is still correct in the first
  // subframe's new process.  Verify it both in JS and with a postMessage.
  success = false;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      popup_root->child_at(0),
      "window.domAutomationController.send("
      "    parent.frames['subframe2'].opener && "
      "        parent.frames['subframe2'].opener === parent.opener);",
      &success));
  EXPECT_TRUE(success);

  base::string16 expected_title = base::ASCIIToUTF16("msg");
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_TRUE(ExecuteScriptAndExtractBool(
      popup_root->child_at(0),
      "window.domAutomationController.send("
      "    postToOpenerOfSibling('subframe2', 'msg', '*'));",
      &success));
  EXPECT_TRUE(success);
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Test for https://crbug.com/515302.  Perform two navigations, A->B->A, and
// drop the SwapOut ACK from the A->B navigation, so that the second B->A
// navigation is initiated before the first page receives the SwapOut ACK.
// Ensure that this doesn't crash and that the RVH(A) is not reused in that
// case.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       RenderViewHostIsNotReusedAfterDelayedSwapOutACK) {
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderFrameHostImpl* rfh = root->current_frame_host();
  RenderViewHostImpl* rvh = rfh->render_view_host();
  int rvh_routing_id = rvh->GetRoutingID();
  SiteInstanceImpl* site_instance = rfh->GetSiteInstance();
  RenderFrameDeletedObserver deleted_observer(rfh);

  // Install a BrowserMessageFilter to drop SwapOut ACK messages in A's
  // process.
  scoped_refptr<SwapoutACKMessageFilter> filter = new SwapoutACKMessageFilter();
  rfh->GetProcess()->AddFilter(filter.get());
  rfh->DisableSwapOutTimerForTesting();

  // Navigate to B.  This must wait for DidCommitProvisionalLoad and not
  // DidStopLoading, so that the SwapOut timer doesn't call OnSwappedOut and
  // destroy |rfh| and |rvh| before they are checked in the test.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestFrameNavigationObserver commit_observer(root);
  shell()->LoadURL(b_url);
  commit_observer.WaitForCommit();
  EXPECT_FALSE(deleted_observer.deleted());

  // Since the SwapOut ACK for A->B is dropped, the first page's
  // RenderFrameHost should be pending deletion after the last navigation.
  EXPECT_FALSE(rfh->is_active());

  // Wait for process A to exit so we can reinitialize it cleanly for the next
  // navigation.  Since process A doesn't have any active views, it will
  // initiate shutdown via ChildProcessHostMsg_ShutdownRequest.  After process
  // A shuts down, the |rfh| and |rvh| should get destroyed via
  // OnRenderProcessGone.
  //
  // Not waiting for process shutdown here could lead to the |rvh| being
  // reused, now that there is no notion of pending deletion RenderViewHosts.
  // This would also be fine; however, the race in https://crbug.com/535246
  // still needs to be addressed and tested in that case.
  RenderProcessHostWatcher process_exit_observer(
      rvh->GetProcess(),
      RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  process_exit_observer.Wait();

  // Verify that the RVH and RFH for A were cleaned up.
  EXPECT_FALSE(root->frame_tree()->GetRenderViewHost(site_instance));
  EXPECT_TRUE(deleted_observer.deleted());

  // Start a navigation back to A and check that the RenderViewHost wasn't
  // reused.
  TestNavigationObserver navigation_observer(shell()->web_contents());
  shell()->LoadURL(a_url);
  RenderViewHostImpl* pending_rvh =
      IsBrowserSideNavigationEnabled()
          ? root->render_manager()->speculative_frame_host()->render_view_host()
          : root->render_manager()->pending_render_view_host();
  EXPECT_EQ(site_instance, pending_rvh->GetSiteInstance());
  EXPECT_NE(rvh_routing_id, pending_rvh->GetRoutingID());

  // Make sure the last navigation finishes without crashing.
  navigation_observer.Wait();
}

// Test for https://crbug.com/591478, where navigating to a cross-site page with
// a subframe on the old site caused a crash while trying to reuse the old
// RenderViewHost.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       ReusePendingDeleteRenderViewHostForSubframe) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  std::string script =
      "window.onunload = function() { "
      "  var start = Date.now();"
      "  while (Date.now() - start < 1000);"
      "}";
  EXPECT_TRUE(ExecuteScript(shell(), script));

  // Navigating cross-site with an iframe to the original site shouldn't crash.
  GURL second_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), second_url));

  // If the subframe is created while the main frame is pending deletion, then
  // the RVH will be reused.  The main frame should've been swapped with a
  // proxy despite being the last active frame in the progress (see
  // https://crbug.com/568836), and this proxy should also be reused by the new
  // page.
  //
  // TODO(creis, alexmos): Find a way to assert this that isn't flaky. For now,
  // the test is just likely (not certain) to catch regressions by crashing.
}

// Check that when a cross-process frame acquires focus, the old focused frame
// loses focus and fires blur events.  Starting on a page with a cross-site
// subframe, simulate mouse clicks to switch focus from root frame to subframe
// and then back to root frame.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CrossProcessFocusChangeFiresBlurEvents) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_input_field.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  // Focus the main frame's text field.  The return value "input-focus"
  // indicates that the focus event was fired correctly.
  std::string result;
  EXPECT_TRUE(
      ExecuteScriptAndExtractString(shell(), "focusInputField()", &result));
  EXPECT_EQ(result, "input-focus");

  // The main frame should be focused.
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());

  DOMMessageQueue msg_queue;

  // Click on the cross-process subframe.
  SimulateMouseClick(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), 1, 1);

  // Check that the main frame lost focus and fired blur event on the input
  // text field.
  std::string status;
  while (msg_queue.WaitForMessage(&status)) {
    if (status == "\"input-blur\"")
      break;
  }

  // The subframe should now be focused.
  EXPECT_EQ(root->child_at(0), root->frame_tree()->GetFocusedFrame());

  // Click on the root frame.
  SimulateMouseClick(
      shell()->web_contents()->GetRenderViewHost()->GetWidget(), 1, 1);

  // Check that the subframe lost focus and fired blur event on its
  // document's body.
  while (msg_queue.WaitForMessage(&status)) {
    if (status == "\"document-blur\"")
      break;
  }

  // The root frame should be focused again.
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());
}

// Check that when a cross-process subframe is focused, its parent's
// document.activeElement correctly returns the corresponding <iframe> element.
// The test sets up an A-embed-B-embed-C page and shifts focus A->B->A->C,
// checking document.activeElement after each change.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, DocumentActiveElement) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));

  FrameTreeNode* child = root->child_at(0);
  FrameTreeNode* grandchild = root->child_at(0)->child_at(0);

  // The main frame should be focused to start with.
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());

  // Focus the b.com frame.
  FocusFrame(child);
  EXPECT_EQ(child, root->frame_tree()->GetFocusedFrame());

  // Helper function to check a property of document.activeElement in the
  // specified frame.
  auto verify_active_element_property = [](RenderFrameHost* rfh,
                                           const std::string& property,
                                           const std::string& expected_value) {
    std::string script = base::StringPrintf(
        "window.domAutomationController.send(document.activeElement.%s);",
        property.c_str());
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(rfh, script, &result));
    EXPECT_EQ(expected_value, base::ToLowerASCII(result));
  };

  // Verify that document.activeElement on main frame points to the <iframe>
  // element for the b.com frame.
  RenderFrameHost* root_rfh = root->current_frame_host();
  verify_active_element_property(root_rfh, "tagName", "iframe");
  verify_active_element_property(root_rfh, "src", child->current_url().spec());

  // Focus the a.com main frame again.
  FocusFrame(root);
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());

  // Main frame document's <body> should now be the active element.
  verify_active_element_property(root_rfh, "tagName", "body");

  // Now shift focus from main frame to c.com frame.
  FocusFrame(grandchild);

  // Check document.activeElement in main frame.  It should still point to
  // <iframe> for the b.com frame, since Blink computes the focused iframe
  // element by walking the parent chain of the focused frame until it hits the
  // current frame.  This logic should still work with remote frames.
  verify_active_element_property(root_rfh, "tagName", "iframe");
  verify_active_element_property(root_rfh, "src", child->current_url().spec());

  // Check document.activeElement in b.com subframe.  It should point to
  // <iframe> for the c.com frame.  This is a tricky case where B needs to find
  // out that focus changed from one remote frame to another (A to C).
  RenderFrameHost* child_rfh = child->current_frame_host();
  verify_active_element_property(child_rfh, "tagName", "iframe");
  verify_active_element_property(child_rfh, "src",
                                 grandchild->current_url().spec());
}

// Check that window.focus works for cross-process subframes.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, SubframeWindowFocus) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   |--Site B ------- proxies for A C\n"
      "   +--Site C ------- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));

  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);

  // The main frame should be focused to start with.
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());

  DOMMessageQueue msg_queue;

  // Register focus and blur events that will send messages when each frame's
  // window gets or loses focus.
  const char kSetupFocusEvents[] =
      "window.addEventListener('focus', function() {"
      "  domAutomationController.setAutomationId(0);"
      "  domAutomationController.send('%s-got-focus');"
      "});"
      "window.addEventListener('blur', function() {"
      "  domAutomationController.setAutomationId(0);"
      "  domAutomationController.send('%s-lost-focus');"
      "});";
  std::string script = base::StringPrintf(kSetupFocusEvents, "main", "main");
  EXPECT_TRUE(ExecuteScript(shell(), script));
  script = base::StringPrintf(kSetupFocusEvents, "child1", "child1");
  EXPECT_TRUE(ExecuteScript(child1, script));
  script = base::StringPrintf(kSetupFocusEvents, "child2", "child2");
  EXPECT_TRUE(ExecuteScript(child2, script));

  // Execute window.focus on the B subframe from the A main frame.
  EXPECT_TRUE(ExecuteScript(root, "frames[0].focus()"));

  // Helper to wait for two specified messages to arrive on the specified
  // DOMMessageQueue, assuming that the two messages can arrive in any order.
  auto wait_for_two_messages = [](DOMMessageQueue* msg_queue,
                                  const std::string& msg1,
                                  const std::string& msg2) {
    bool msg1_received = false;
    bool msg2_received = false;
    std::string status;
    while (msg_queue->WaitForMessage(&status)) {
      if (status == msg1)
        msg1_received = true;
      if (status == msg2)
        msg2_received = true;
      if (msg1_received && msg2_received)
        break;
    }
  };

  // Process A should fire a blur event, and process B should fire a focus
  // event.  Wait for both events.
  wait_for_two_messages(&msg_queue, "\"main-lost-focus\"",
                        "\"child1-got-focus\"");

  // The B subframe should now be focused in the browser process.
  EXPECT_EQ(child1, root->frame_tree()->GetFocusedFrame());

  // Now, execute window.focus on the C subframe from A main frame.  This
  // checks that we can shift focus from one remote frame to another.
  EXPECT_TRUE(ExecuteScript(root, "frames[1].focus()"));

  // Wait for the two subframes (B and C) to fire blur and focus events.
  wait_for_two_messages(&msg_queue, "\"child1-lost-focus\"",
                        "\"child2-got-focus\"");

  // The C subframe should now be focused.
  EXPECT_EQ(child2, root->frame_tree()->GetFocusedFrame());

  // window.focus the main frame from the C subframe.
  EXPECT_TRUE(ExecuteScript(child2, "parent.focus()"));

  // Wait for the C subframe to blur and main frame to focus.
  wait_for_two_messages(&msg_queue, "\"child2-lost-focus\"",
                        "\"main-got-focus\"");

  // The main frame should now be focused.
  EXPECT_EQ(root, root->frame_tree()->GetFocusedFrame());
}

// There are no cursors on Android.
#if !defined(OS_ANDROID)
class CursorMessageFilter : public content::BrowserMessageFilter {
 public:
  CursorMessageFilter()
      : content::BrowserMessageFilter(ViewMsgStart),
        message_loop_runner_(new content::MessageLoopRunner),
        last_set_cursor_routing_id_(MSG_ROUTING_NONE) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() == ViewHostMsg_SetCursor::ID) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&CursorMessageFilter::OnSetCursor, this,
                     message.routing_id()));
    }
    return false;
  }

  void OnSetCursor(int routing_id) {
    last_set_cursor_routing_id_ = routing_id;
    message_loop_runner_->Quit();
  }

  int last_set_cursor_routing_id() const { return last_set_cursor_routing_id_; }

  void Wait() {
    last_set_cursor_routing_id_ = MSG_ROUTING_NONE;
    message_loop_runner_->Run();
  }

 private:
  ~CursorMessageFilter() override {}

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  int last_set_cursor_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(CursorMessageFilter);
};

// Verify that we receive a mouse cursor update message when we mouse over
// a text field contained in an out-of-process iframe.
// Fails under TSan.  http://crbug.com/545237
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       DISABLED_CursorUpdateFromReceivedFromCrossSiteIframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  FrameTreeNode* child_node = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  scoped_refptr<CursorMessageFilter> filter = new CursorMessageFilter();
  child_node->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Send a MouseMove to the subframe. The frame contains text, and moving the
  // mouse over it should cause the renderer to send a mouse cursor update.
  blink::WebMouseEvent mouse_event(blink::WebInputEvent::kMouseMove,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  mouse_event.SetPositionInWidget(60, 60);
  RenderWidgetHost* rwh_child =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  web_contents()->GetInputEventRouter()->RouteMouseEvent(
      root_view, &mouse_event, ui::LatencyInfo());

  // CursorMessageFilter::Wait() implicitly tests whether we receive a
  // ViewHostMsg_SetCursor message from the renderer process, because it does
  // does not return otherwise.
  filter->Wait();
  EXPECT_EQ(filter->last_set_cursor_routing_id(), rwh_child->GetRoutingID());
}
#endif

// Tests that we are using the correct RenderFrameProxy when navigating an
// opener window.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, OpenerSetLocation) {
  // Navigate the main window.
  GURL main_url(embedded_test_server()->GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), main_url);

  // Load cross-site page into a new window.
  GURL cross_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  Shell* popup = OpenPopup(shell(), cross_url, "");
  EXPECT_EQ(popup->web_contents()->GetLastCommittedURL(), cross_url);

  // Use new window to navigate main window.
  std::string script =
      "window.opener.location.href = '" + cross_url.spec() + "'";
  EXPECT_TRUE(ExecuteScript(popup, script));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), cross_url);
}

#if defined(USE_AURA)
// Browser process hit testing is not implemented on Android, and these tests
// require Aura for RenderWidgetHostViewAura::OnTouchEvent().
// https://crbug.com/491334

// Ensure that scroll events can be cancelled with a wheel handler.
// https://crbug.com/698195

class SitePerProcessMouseWheelBrowserTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessMouseWheelBrowserTest() : rwhv_root_(nullptr) {}

  void SetupWheelAndScrollHandlers(content::RenderFrameHostImpl* rfh) {
    // Set up event handlers. The wheel event handler calls prevent default on
    // alternate events, so only every other wheel generates a scroll. The fact
    // that any scroll events fire is dependent on the event going to the main
    // thread, which requires the nonFastScrollableRegion be set correctly
    // on the compositor.
    std::string script =
        "wheel_count = 0;"
        "function wheel_handler(e) {"
        "  wheel_count++;"
        "  if (wheel_count % 2 == 0)"
        "    e.preventDefault();\n"
        "  domAutomationController.setAutomationId(0);"
        "  domAutomationController.send('wheel: ' + wheel_count);"
        "}"
        "function scroll_handler(e) {"
        "  domAutomationController.setAutomationId(0);"
        "  domAutomationController.send('scroll: ' + wheel_count);"
        "}"
        "scroll_div = document.getElementById('scrollable_div');"
        "scroll_div.addEventListener('wheel', wheel_handler);"
        "scroll_div.addEventListener('scroll', scroll_handler);"
        "domAutomationController.setAutomationId(0);"
        "domAutomationController.send('wheel handler installed');"
        "document.body.style.background = 'black';";

    content::DOMMessageQueue msg_queue;
    std::string reply;
    EXPECT_TRUE(ExecuteScript(rfh, script));

    // Wait until renderer's compositor thread is synced. Otherwise the event
    // handler won't be installed when the event arrives.
    {
      MainThreadFrameObserver observer(rfh->GetRenderWidgetHost());
      observer.Wait();
    }
  }

  void SendMouseWheel(gfx::Point location) {
    DCHECK(rwhv_root_);
    ui::ScrollEvent scroll_event(ui::ET_SCROLL, location, ui::EventTimeForNow(),
                                 0, 0, -ui::MouseWheelEvent::kWheelDelta, 0,
                                 ui::MouseWheelEvent::kWheelDelta,
                                 2);  // This must be '2' or it gets silently
                                      // dropped.
    rwhv_root_->OnScrollEvent(&scroll_event);
  }

  void set_rwhv_root(RenderWidgetHostViewAura* rwhv_root) {
    rwhv_root_ = rwhv_root;
  }

  void RunTest(gfx::Point pos) {
    content::DOMMessageQueue msg_queue;
    std::string reply;

    auto* rwhv_root = static_cast<RenderWidgetHostViewAura*>(
        web_contents()->GetRenderWidgetHostView());
    set_rwhv_root(rwhv_root);

    SendMouseWheel(pos);

    // Expect both wheel and scroll handlers to fire.
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"wheel: 1\"", reply);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"scroll: 1\"", reply);

    SendMouseWheel(pos);

    // This time only the wheel handler fires, since it prevent defaults on
    // even numbered scrolls.
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"wheel: 2\"", reply);

    SendMouseWheel(pos);

    // Odd number of wheels, expect both wheel and scroll handlers to fire
    // again.
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"wheel: 3\"", reply);
    EXPECT_TRUE(msg_queue.WaitForMessage(&reply));
    EXPECT_EQ("\"scroll: 3\"", reply);
  }

 private:
  RenderWidgetHostViewAura* rwhv_root_;
};

IN_PROC_BROWSER_TEST_F(SitePerProcessMouseWheelBrowserTest,
                       SubframeWheelEventsOnMainThread) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/page_with_scrollable_div.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  RenderWidgetHostViewBase* child_rwhv = static_cast<RenderWidgetHostViewBase*>(
      root->child_at(0)->current_frame_host()->GetView());
  WaitForChildFrameSurfaceReady(root->child_at(0)->current_frame_host());

  content::RenderFrameHostImpl* child = root->child_at(0)->current_frame_host();
  SetupWheelAndScrollHandlers(child);

  gfx::Rect bounds = child_rwhv->GetViewBounds();
  gfx::Point pos(bounds.x() + 10, bounds.y() + 10);

  RunTest(pos);
}

// Verifies that test in SubframeWheelEventsOnMainThread also makes sense for
// the same page loaded in the mainframe.
IN_PROC_BROWSER_TEST_F(SitePerProcessMouseWheelBrowserTest,
                       MainframeWheelEventsOnMainThread) {
  GURL main_url(
      embedded_test_server()->GetURL("/page_with_scrollable_div.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  content::RenderFrameHostImpl* rfhi = root->current_frame_host();
  SetupWheelAndScrollHandlers(rfhi);

  gfx::Point pos(10, 10);

  RunTest(pos);
}

// Ensure that a cross-process subframe with a touch-handler can receive touch
// events.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       SubframeTouchEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_touch_handler.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForChildFrameSurfaceReady(root->child_at(0)->current_frame_host());

  // There's no intrinsic reason the following values can't be equal, but they
  // aren't at present, and if they become the same this test will need to be
  // updated to accommodate.
  EXPECT_NE(cc::kTouchActionAuto, cc::kTouchActionNone);

  // Verify the child's input router is initially set for kTouchActionAuto. The
  // TouchStart event will trigger kTouchActionNone being sent back to the
  // browser.
  RenderWidgetHostImpl* child_render_widget_host =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  InputRouterImpl* child_input_router =
      static_cast<InputRouterImpl*>(child_render_widget_host->input_router());
  EXPECT_EQ(cc::kTouchActionAuto,
            child_input_router->touch_action_filter_.allowed_touch_action());

  // Simulate touch event to sub-frame.
  gfx::Point child_center(150, 150);
  auto* rwhv = static_cast<RenderWidgetHostViewAura*>(
      contents->GetRenderWidgetHostView());

  // Wait until renderer's compositor thread is synced.
  {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }

  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, child_center, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         /* pointer_id*/ 0,
                         /* radius_x */ 30.0f,
                         /* radius_y */ 30.0f,
                         /* force */ 0.0f));
  rwhv->OnTouchEvent(&touch_event);
  {
    MainThreadFrameObserver observer(child_render_widget_host);
    observer.Wait();
  }

  // Verify touch handler in subframe was invoked.
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root->child_at(0),
      "window.domAutomationController.send(getLastTouchEvent());", &result));
  EXPECT_EQ("touchstart", result);

  // Verify the presence of the touch handler in the child frame correctly
  // propagates touch-action:none information back to the child's input router.
  EXPECT_EQ(cc::kTouchActionNone,
            child_input_router->touch_action_filter_.allowed_touch_action());
}

// This test verifies that the test in
// SitePerProcessBrowserTest.SubframeTouchEventRouting also works properly for
// the main frame. Prior to the CL in which this test is introduced, use of
// MainThreadFrameObserver in SubframeTouchEventRouting was not necessary since
// the touch events were handled on the main thread. Now they are handled on the
// compositor thread, hence the need to synchronize.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       MainframeTouchEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "/page_with_touch_handler.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();

  // Synchronize with the renderers to guarantee that the
  // surface information required for event hit testing is ready.
  auto* rwhv = static_cast<RenderWidgetHostViewAura*>(
      contents->GetRenderWidgetHostView());

  // There's no intrinsic reason the following values can't be equal, but they
  // aren't at present, and if they become the same this test will need to be
  // updated to accommodate.
  EXPECT_NE(cc::kTouchActionAuto, cc::kTouchActionNone);

  // Verify the main frame's input router is initially set for
  // kTouchActionAuto. The
  // TouchStart event will trigger kTouchActionNone being sent back to the
  // browser.
  RenderWidgetHostImpl* render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  InputRouterImpl* input_router =
      static_cast<InputRouterImpl*>(render_widget_host->input_router());
  EXPECT_EQ(cc::kTouchActionAuto,
            input_router->touch_action_filter_.allowed_touch_action());

  // Simulate touch event to sub-frame.
  gfx::Point frame_center(150, 150);

  // Wait until renderer's compositor thread is synced.
  {
    auto observer =
        base::MakeUnique<MainThreadFrameObserver>(render_widget_host);
    observer->Wait();
  }

  ui::TouchEvent touch_event(
      ui::ET_TOUCH_PRESSED, frame_center, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         /* pointer_id*/ 0,
                         /* radius_x */ 30.0f,
                         /* radius_y */ 30.0f,
                         /* force */ 0.0f));
  rwhv->OnTouchEvent(&touch_event);
  {
    auto observer =
        base::MakeUnique<MainThreadFrameObserver>(render_widget_host);
    observer->Wait();
  }

  // Verify touch handler in subframe was invoked.
  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root, "window.domAutomationController.send(getLastTouchEvent());",
      &result));
  EXPECT_EQ("touchstart", result);

  // Verify the presence of the touch handler in the child frame correctly
  // propagates touch-action:none information back to the child's input router.
  EXPECT_EQ(cc::kTouchActionNone,
            input_router->touch_action_filter_.allowed_touch_action());
}

namespace {

// Declared here to be close to the SubframeGestureEventRouting test.
void OnSyntheticGestureCompleted(scoped_refptr<MessageLoopRunner> runner,
                                 SyntheticGesture::Result result) {
  EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
  runner->Quit();
}

}  // namespace anonymous

// Flaky under TSan. https://crbug.com/592320
#if defined(THREAD_SANITIZER)
#define MAYBE_SubframeGestureEventRouting DISABLED_SubframeGestureEventRouting
#else
#define MAYBE_SubframeGestureEventRouting SubframeGestureEventRouting
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       MAYBE_SubframeGestureEventRouting) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_click_handler.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForChildFrameSurfaceReady(child_frame_host);

  // There have been no GestureTaps sent yet.
  {
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        child_frame_host,
        "window.domAutomationController.send(getClickStatus());", &result));
    EXPECT_EQ("0 clicks received", result);
  }

  // Simulate touch sequence to send GestureTap to sub-frame.
  SyntheticTapGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::TOUCH_INPUT;
  gfx::Point center(150, 150);
  params.position = gfx::PointF(center.x(), center.y());
  params.duration_ms = 100;
  std::unique_ptr<SyntheticTapGesture> gesture(new SyntheticTapGesture(params));

  scoped_refptr<MessageLoopRunner> runner = new MessageLoopRunner();

  RenderWidgetHostImpl* render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();
  // TODO(wjmaclean): Convert the call to base::Bind() to a lambda someday.
  render_widget_host->QueueSyntheticGesture(
      std::move(gesture), base::Bind(OnSyntheticGestureCompleted, runner));

  // We need to run the message loop while we wait for the synthetic gesture
  // to be processed; the callback registered above will get us out of the
  // message loop when that happens.
  runner->Run();
  runner = nullptr;

  // Verify click handler in subframe was invoked
  {
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        child_frame_host,
        "window.domAutomationController.send(getClickStatus());", &result));
    EXPECT_EQ("1 click received", result);
  }
}

namespace {

// Defined here to be close to
// SitePerProcessBrowserTest.InputEventRouterGestureTargetQueueTest.
void SendTouchTapWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& touch_point,
    RenderWidgetHostViewBase*& router_touch_target,
    const RenderWidgetHostViewBase* expected_target) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);
  ui::TouchEvent touch_event_pressed(
      ui::ET_TOUCH_PRESSED, touch_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         /* pointer_id*/ 0,
                         /* radius_x */ 30.0f,
                         /* radius_y */ 30.0f,
                         /* force */ 0.0f));
  root_view_aura->OnTouchEvent(&touch_event_pressed);
  EXPECT_EQ(expected_target, router_touch_target);
  ui::TouchEvent touch_event_released(
      ui::ET_TOUCH_RELEASED, touch_point, ui::EventTimeForNow(),
      ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                         /* pointer_id*/ 0,
                         /* radius_x */ 30.0f,
                         /* radius_y */ 30.0f,
                         /* force */ 0.0f));
  root_view_aura->OnTouchEvent(&touch_event_released);
  EXPECT_EQ(nullptr, router_touch_target);
}

void SendGestureTapSequenceWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& gesture_point,
    RenderWidgetHostViewBase*& router_gesture_target,
    const RenderWidgetHostViewBase* old_expected_target,
    const RenderWidgetHostViewBase* expected_target) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);

  ui::GestureEventDetails gesture_begin_details(ui::ET_GESTURE_BEGIN);
  gesture_begin_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_begin_event(gesture_point.x(), gesture_point.y(), 0,
                                       ui::EventTimeForNow(),
                                       gesture_begin_details);
  root_view_aura->OnGestureEvent(&gesture_begin_event);
  // We expect to still have the old gesture target in place for the
  // GestureFlingCancel that will be inserted before GestureTapDown.
  // Note: the GestureFlingCancel is inserted by RenderWidgetHostViewAura::
  // OnGestureEvent() when it sees ui::ET_GESTURE_TAP_DOWN, so we don't
  // explicitly add it here.
  EXPECT_EQ(old_expected_target, router_gesture_target);

  ui::GestureEventDetails gesture_tap_down_details(ui::ET_GESTURE_TAP_DOWN);
  gesture_tap_down_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_tap_down_event(gesture_point.x(), gesture_point.y(),
                                          0, ui::EventTimeForNow(),
                                          gesture_tap_down_details);
  root_view_aura->OnGestureEvent(&gesture_tap_down_event);
  EXPECT_EQ(expected_target, router_gesture_target);

  ui::GestureEventDetails gesture_show_press_details(ui::ET_GESTURE_SHOW_PRESS);
  gesture_show_press_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_show_press_event(
      gesture_point.x(), gesture_point.y(), 0, ui::EventTimeForNow(),
      gesture_show_press_details);
  root_view_aura->OnGestureEvent(&gesture_show_press_event);
  EXPECT_EQ(expected_target, router_gesture_target);

  ui::GestureEventDetails gesture_tap_details(ui::ET_GESTURE_TAP);
  gesture_tap_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  gesture_tap_details.set_tap_count(1);
  ui::GestureEvent gesture_tap_event(gesture_point.x(), gesture_point.y(), 0,
                                     ui::EventTimeForNow(),
                                     gesture_tap_details);
  root_view_aura->OnGestureEvent(&gesture_tap_event);
  EXPECT_EQ(expected_target, router_gesture_target);

  ui::GestureEventDetails gesture_end_details(ui::ET_GESTURE_END);
  gesture_end_details.set_device_type(
      ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
  ui::GestureEvent gesture_end_event(gesture_point.x(), gesture_point.y(), 0,
                                     ui::EventTimeForNow(),
                                     gesture_end_details);
  root_view_aura->OnGestureEvent(&gesture_end_event);
  EXPECT_EQ(expected_target, router_gesture_target);
}

void SendTouchpadPinchSequenceWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& gesture_point,
    RenderWidgetHostViewBase*& router_touchpad_gesture_target,
    RenderWidgetHostViewBase* expected_target) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);

  ui::GestureEventDetails pinch_begin_details(ui::ET_GESTURE_PINCH_BEGIN);
  pinch_begin_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent pinch_begin(gesture_point.x(), gesture_point.y(), 0,
                               ui::EventTimeForNow(), pinch_begin_details);
  root_view_aura->OnGestureEvent(&pinch_begin);
  EXPECT_EQ(expected_target, router_touchpad_gesture_target);

  ui::GestureEventDetails pinch_update_details(ui::ET_GESTURE_PINCH_UPDATE);
  pinch_update_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent pinch_update(gesture_point.x(), gesture_point.y(), 0,
                                ui::EventTimeForNow(), pinch_update_details);
  root_view_aura->OnGestureEvent(&pinch_update);
  EXPECT_EQ(expected_target, router_touchpad_gesture_target);

  ui::GestureEventDetails pinch_end_details(ui::ET_GESTURE_PINCH_END);
  pinch_end_details.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  ui::GestureEvent pinch_end(gesture_point.x(), gesture_point.y(), 0,
                             ui::EventTimeForNow(), pinch_end_details);
  root_view_aura->OnGestureEvent(&pinch_end);
  EXPECT_EQ(expected_target, router_touchpad_gesture_target);
}

#if !defined(OS_WIN)
// Sending touchpad fling events is not supported on Windows.
void SendTouchpadFlingSequenceWithExpectedTarget(
    RenderWidgetHostViewBase* root_view,
    const gfx::Point& gesture_point,
    RenderWidgetHostViewBase*& router_touchpad_gesture_target,
    RenderWidgetHostViewBase* expected_target) {
  auto* root_view_aura = static_cast<RenderWidgetHostViewAura*>(root_view);

  ui::ScrollEvent fling_start(ui::ET_SCROLL_FLING_START, gesture_point,
                              ui::EventTimeForNow(), 0, 1, 0, 1, 0, 1);
  root_view_aura->OnScrollEvent(&fling_start);
  EXPECT_EQ(expected_target, router_touchpad_gesture_target);

  ui::ScrollEvent fling_cancel(ui::ET_SCROLL_FLING_CANCEL, gesture_point,
                               ui::EventTimeForNow(), 0, 1, 0, 1, 0, 1);
  root_view_aura->OnScrollEvent(&fling_cancel);
  EXPECT_EQ(expected_target, router_touchpad_gesture_target);
}
#endif

}  // namespace anonymous

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       InputEventRouterGestureTargetQueueTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_click_handler.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  auto* child_frame_host = root->child_at(0)->current_frame_host();
  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  WaitForChildFrameSurfaceReady(child_frame_host);

  // All touches & gestures are sent to the main frame's view, and should be
  // routed appropriately from there.
  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  RenderWidgetHostInputEventRouter* router = contents->GetInputEventRouter();
  EXPECT_TRUE(router->touchscreen_gesture_target_queue_.empty());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send touch sequence to main-frame.
  gfx::Point main_frame_point(25, 25);
  SendTouchTapWithExpectedTarget(rwhv_parent, main_frame_point,
                                 router->touch_target_.target, rwhv_parent);
  EXPECT_EQ(1LU, router->touchscreen_gesture_target_queue_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send touch sequence to child.
  gfx::Point child_center(150, 150);
  SendTouchTapWithExpectedTarget(rwhv_parent, child_center,
                                 router->touch_target_.target, rwhv_child);
  EXPECT_EQ(2LU, router->touchscreen_gesture_target_queue_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send another touch sequence to main frame.
  SendTouchTapWithExpectedTarget(rwhv_parent, main_frame_point,
                                 router->touch_target_.target, rwhv_parent);
  EXPECT_EQ(3LU, router->touchscreen_gesture_target_queue_.size());
  EXPECT_EQ(nullptr, router->touchscreen_gesture_target_.target);

  // Send Gestures to clear GestureTargetQueue.

  // The first touch sequence should generate a GestureTapDown, sent to the
  // main frame.
  SendGestureTapSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchscreen_gesture_target_.target,
      nullptr, rwhv_parent);
  EXPECT_EQ(2LU, router->touchscreen_gesture_target_queue_.size());
  // Note: rwhv_parent is the target used for GestureFlingCancel sent by
  // RenderWidgetHostViewAura::OnGestureEvent() at the start of the next gesture
  // sequence; the sequence itself goes to rwhv_child.
  EXPECT_EQ(rwhv_parent, router->touchscreen_gesture_target_.target);

  // The second touch sequence should generate a GestureTapDown, sent to the
  // child frame.
  SendGestureTapSequenceWithExpectedTarget(
      rwhv_parent, child_center, router->touchscreen_gesture_target_.target,
      rwhv_parent, rwhv_child);
  EXPECT_EQ(1LU, router->touchscreen_gesture_target_queue_.size());
  EXPECT_EQ(rwhv_child, router->touchscreen_gesture_target_.target);

  // The third touch sequence should generate a GestureTapDown, sent to the
  // main frame.
  SendGestureTapSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchscreen_gesture_target_.target,
      rwhv_child, rwhv_parent);
  EXPECT_EQ(0LU, router->touchscreen_gesture_target_queue_.size());
  EXPECT_EQ(rwhv_parent, router->touchscreen_gesture_target_.target);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       InputEventRouterTouchpadGestureTargetTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/frame_tree/page_with_positioned_nested_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());

  GURL frame_url(
      embedded_test_server()->GetURL("b.com", "/page_with_click_handler.html"));
  NavigateFrameToURL(root->child_at(0), frame_url);
  auto* child_frame_host = root->child_at(0)->current_frame_host();

  // Synchronize with the child and parent renderers to guarantee that the
  // surface information required for event hit testing is ready.
  auto* rwhv_child =
      static_cast<RenderWidgetHostViewBase*>(child_frame_host->GetView());
  WaitForChildFrameSurfaceReady(child_frame_host);

  // All touches & gestures are sent to the main frame's view, and should be
  // routed appropriately from there.
  auto* rwhv_parent = static_cast<RenderWidgetHostViewBase*>(
      contents->GetRenderWidgetHostView());

  RenderWidgetHostInputEventRouter* router = contents->GetInputEventRouter();
  EXPECT_EQ(nullptr, router->touchpad_gesture_target_.target);

  gfx::Point main_frame_point(25, 25);
  gfx::Point child_center(150, 150);

  // Send touchpad pinch sequence to main-frame.
  SendTouchpadPinchSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchpad_gesture_target_.target,
      rwhv_parent);

  // Send touchpad pinch sequence to child.
  SendTouchpadPinchSequenceWithExpectedTarget(
      rwhv_parent, child_center, router->touchpad_gesture_target_.target,
      rwhv_child);

  // Send another touchpad pinch sequence to main frame.
  SendTouchpadPinchSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchpad_gesture_target_.target,
      rwhv_parent);

#if !defined(OS_WIN)
  // Sending touchpad fling events is not supported on Windows.

  // Send touchpad fling sequence to main-frame.
  SendTouchpadFlingSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchpad_gesture_target_.target,
      rwhv_parent);

  // Send touchpad fling sequence to child.
  SendTouchpadFlingSequenceWithExpectedTarget(
      rwhv_parent, child_center, router->touchpad_gesture_target_.target,
      rwhv_child);

  // Send another touchpad fling sequence to main frame.
  SendTouchpadFlingSequenceWithExpectedTarget(
      rwhv_parent, main_frame_point, router->touchpad_gesture_target_.target,
      rwhv_parent);
#endif
}
#endif  // defined(USE_AURA)

// A WebContentsDelegate to capture ContextMenu creation events.
class ContextMenuObserverDelegate : public WebContentsDelegate {
 public:
  ContextMenuObserverDelegate()
      : context_menu_created_(false),
        message_loop_runner_(new MessageLoopRunner) {}

  ~ContextMenuObserverDelegate() override {}

  bool HandleContextMenu(const content::ContextMenuParams& params) override {
    context_menu_created_ = true;
    menu_params_ = params;
    message_loop_runner_->Quit();
    return true;
  }

  ContextMenuParams getParams() { return menu_params_; }

  void Wait() {
    if (!context_menu_created_)
      message_loop_runner_->Run();
    context_menu_created_ = false;
  }

 private:
  bool context_menu_created_;
  ContextMenuParams menu_params_;

  // The MessageLoopRunner used to spin the message loop.
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(ContextMenuObserverDelegate);
};

// Helper function to run the CreateContextMenuTest in either normal
// or high DPI mode.
void CreateContextMenuTestHelper(
    Shell* shell,
    net::test_server::EmbeddedTestServer* embedded_test_server) {
  GURL main_url(embedded_test_server->GetURL(
      "/frame_tree/page_with_positioned_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell, main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell->web_contents())
                            ->GetFrameTree()
                            ->root();
  ASSERT_EQ(1U, root->child_count());

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server->GetURL("baz.com", "/title1.html"));
  EXPECT_EQ(site_url, child_node->current_url());
  EXPECT_NE(shell->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  RenderWidgetHostViewBase* root_view = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  // Ensure that the child process renderer is ready to have input events
  // routed to it. This happens when the browser process has received
  // updated compositor surfaces from both renderer processes.
  WaitForChildFrameSurfaceReady(child_node->current_frame_host());

  // A WebContentsDelegate to listen for the ShowContextMenu message.
  ContextMenuObserverDelegate context_menu_delegate;
  shell->web_contents()->SetDelegate(&context_menu_delegate);

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell->web_contents())
          ->GetInputEventRouter();

  float scale_factor = GetPageScaleFactor(shell);

  gfx::Rect root_bounds = root_view->GetViewBounds();
  gfx::Rect bounds = rwhv_child->GetViewBounds();

  gfx::Point point(
      gfx::ToCeiledInt((bounds.x() - root_bounds.x() + 5) * scale_factor),
      gfx::ToCeiledInt((bounds.y() - root_bounds.y() + 5) * scale_factor));

  // Target right-click event to child frame.
  blink::WebMouseEvent click_event(blink::WebInputEvent::kMouseDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  click_event.button = blink::WebPointerProperties::Button::kRight;
  click_event.SetPositionInWidget(point.x(), point.y());
  click_event.click_count = 1;
  router->RouteMouseEvent(root_view, &click_event, ui::LatencyInfo());

  // We also need a MouseUp event, needed by Windows.
  click_event.SetType(blink::WebInputEvent::kMouseUp);
  click_event.SetPositionInWidget(point.x(), point.y());
  router->RouteMouseEvent(root_view, &click_event, ui::LatencyInfo());

  context_menu_delegate.Wait();

  ContextMenuParams params = context_menu_delegate.getParams();

  EXPECT_NEAR(point.x(), params.x, 2);
  EXPECT_NEAR(point.y(), params.y, 2);
}

// Test that a mouse right-click to an out-of-process iframe causes a context
// menu to be generated with the correct screen position.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CreateContextMenuTest) {
  CreateContextMenuTestHelper(shell(), embedded_test_server());
}

// Test that a mouse right-click to an out-of-process iframe causes a context
// menu to be generated with the correct screen position on a screen with
// non-default scale factor.
#if defined(OS_ANDROID) || defined(OS_WIN)
// High DPI tests don't work properly on Android, which has fixed scale factor.
// Windows is disabled because of https://crbug.com/545547.
#define MAYBE_HighDPICreateContextMenuTest DISABLED_HighDPICreateContextMenuTest
#else
#define MAYBE_HighDPICreateContextMenuTest HighDPICreateContextMenuTest
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessHighDPIBrowserTest,
                       MAYBE_HighDPICreateContextMenuTest) {
  CreateContextMenuTestHelper(shell(), embedded_test_server());
}

class ShowWidgetMessageFilter : public content::BrowserMessageFilter {
 public:
  ShowWidgetMessageFilter()
#if defined(OS_MACOSX) || defined(OS_ANDROID)
      : content::BrowserMessageFilter(FrameMsgStart),
#else
      : content::BrowserMessageFilter(ViewMsgStart),
#endif
        message_loop_runner_(new content::MessageLoopRunner) {
  }

  bool OnMessageReceived(const IPC::Message& message) override {
    IPC_BEGIN_MESSAGE_MAP(ShowWidgetMessageFilter, message)
#if defined(OS_MACOSX) || defined(OS_ANDROID)
      IPC_MESSAGE_HANDLER(FrameHostMsg_ShowPopup, OnShowPopup)
#else
      IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
#endif
    IPC_END_MESSAGE_MAP()
    return false;
  }

  gfx::Rect last_initial_rect() const { return initial_rect_; }

  int last_routing_id() const { return routing_id_; }

  void Wait() {
    initial_rect_ = gfx::Rect();
    routing_id_ = MSG_ROUTING_NONE;
    message_loop_runner_->Run();
  }

  void Reset() {
    initial_rect_ = gfx::Rect();
    routing_id_ = MSG_ROUTING_NONE;
    message_loop_runner_ = new content::MessageLoopRunner;
  }

 private:
  ~ShowWidgetMessageFilter() override {}

  void OnShowWidget(int route_id, const gfx::Rect& initial_rect) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&ShowWidgetMessageFilter::OnShowWidgetOnUI, this, route_id,
                   initial_rect));
  }

#if defined(OS_MACOSX) || defined(OS_ANDROID)
  void OnShowPopup(const FrameHostMsg_ShowPopup_Params& params) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&ShowWidgetMessageFilter::OnShowWidgetOnUI, this,
                   MSG_ROUTING_NONE, params.bounds));
  }
#endif

  void OnShowWidgetOnUI(int route_id, const gfx::Rect& initial_rect) {
    initial_rect_ = initial_rect;
    routing_id_ = route_id;
    message_loop_runner_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  gfx::Rect initial_rect_;
  int routing_id_;

  DISALLOW_COPY_AND_ASSIGN(ShowWidgetMessageFilter);
};

// Test that clicking a select element in an out-of-process iframe creates
// a popup menu in the correct position.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, PopupMenuTest) {
  GURL main_url(
      embedded_test_server()->GetURL("/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
  // Unused variable on Mac and Android.
  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
#endif

  FrameTreeNode* child_node = root->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "baz.com", "/site_isolation/page-with-select.html"));
  NavigateFrameToURL(child_node, site_url);

  web_contents()->SendScreenRects();

  WaitForChildFrameSurfaceReady(child_node->current_frame_host());

  RenderWidgetHostViewBase* rwhv_child = static_cast<RenderWidgetHostViewBase*>(
      child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child_node->current_frame_host()->GetSiteInstance());

  scoped_refptr<ShowWidgetMessageFilter> filter = new ShowWidgetMessageFilter();
  child_node->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Target left-click event to child frame.
  blink::WebMouseEvent click_event(blink::WebInputEvent::kMouseDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  click_event.button = blink::WebPointerProperties::Button::kLeft;
  click_event.SetPositionInWidget(15, 15);
  click_event.click_count = 1;
  rwhv_child->ProcessMouseEvent(click_event, ui::LatencyInfo());

  // Dismiss the popup.
  click_event.SetPositionInWidget(1, 1);
  rwhv_child->ProcessMouseEvent(click_event, ui::LatencyInfo());

  filter->Wait();
  gfx::Rect popup_rect = filter->last_initial_rect();
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  // On Mac and Android we receive the coordinates before they are transformed,
  // so they are still relative to the out-of-process iframe origin.
  EXPECT_EQ(popup_rect.x(), 9);
  EXPECT_EQ(popup_rect.y(), 9);
#else
  EXPECT_EQ(popup_rect.x() - rwhv_root->GetViewBounds().x(), 354);
  EXPECT_EQ(popup_rect.y() - rwhv_root->GetViewBounds().y(), 94);
#endif

#if defined(OS_LINUX)
  // Verify click-and-drag selection of popups still works on Linux with
  // OOPIFs enabled. This is only necessary to test on Aura because Mac and
  // Android use native widgets. Windows does not support this as UI
  // convention (it requires separate clicks to open the menu and select an
  // option). See https://crbug.com/703191.
  int process_id = child_node->current_frame_host()->GetProcess()->GetID();
  filter->Reset();
  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();
  // Re-open the select element.
  click_event.SetPositionInWidget(360, 90);
  click_event.click_count = 1;
  router->RouteMouseEvent(rwhv_root, &click_event, ui::LatencyInfo());

  filter->Wait();

  RenderWidgetHostView* popup_view =
      RenderWidgetHost::FromID(process_id, filter->last_routing_id())
          ->GetView();

  RenderWidgetHostMouseEventMonitor popup_monitor(
      popup_view->GetRenderWidgetHost());

  // Next send a mouse up directly targeting the first option, simulating a
  // drag. This requires a ui::MouseEvent because it tests behavior that is
  // above RWH input event routing.
  ui::MouseEvent mouse_up_event(ui::ET_MOUSE_RELEASED, gfx::Point(10, 5),
                                gfx::Point(10, 5), ui::EventTimeForNow(),
                                ui::EF_LEFT_MOUSE_BUTTON,
                                ui::EF_LEFT_MOUSE_BUTTON);
  static_cast<RenderWidgetHostViewAura*>(popup_view)
      ->OnMouseEvent(&mouse_up_event);

  // This verifies that the popup actually received the event, and it wasn't
  // diverted to a different RenderWidgetHostView due to mouse capture.
  EXPECT_TRUE(popup_monitor.EventWasReceived());
#endif
}

// Test that clicking a select element in a nested out-of-process iframe creates
// a popup menu in the correct position, even if the top-level page repositions
// its out-of-process iframe. This verifies that screen positioning information
// is propagating down the frame tree correctly.
#if defined(OS_ANDROID)
// Surface-based hit testing and coordinate translation is not yet avaiable on
// Android.
#define MAYBE_NestedPopupMenuTest DISABLED_NestedPopupMenuTest
#else
// Times out frequently. https://crbug.com/599730.
#define MAYBE_NestedPopupMenuTest DISABLED_NestedPopupMenuTest
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, MAYBE_NestedPopupMenuTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

#if !defined(OS_MACOSX)
  // Undefined variable on Mac.
  RenderWidgetHostViewBase* rwhv_root = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());
#endif
  web_contents()->SendScreenRects();

  // For clarity, we are labeling the frame tree nodes as:
  //  - root_node
  //   \-> b_node (out-of-process from root and c_node)
  //     \-> c_node (out-of-process from root and b_node)

  content::TestNavigationObserver navigation_observer(shell()->web_contents());
  FrameTreeNode* b_node = root->child_at(0);
  FrameTreeNode* c_node = b_node->child_at(0);
  GURL site_url(embedded_test_server()->GetURL(
      "baz.com", "/site_isolation/page-with-select.html"));
  NavigateFrameToURL(c_node, site_url);

  RenderWidgetHostViewBase* rwhv_c_node =
      static_cast<RenderWidgetHostViewBase*>(
          c_node->current_frame_host()->GetRenderWidgetHost()->GetView());

  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            c_node->current_frame_host()->GetSiteInstance());

  scoped_refptr<ShowWidgetMessageFilter> filter = new ShowWidgetMessageFilter();
  c_node->current_frame_host()->GetProcess()->AddFilter(filter.get());

  // Target left-click event to child frame.
  blink::WebMouseEvent click_event(blink::WebInputEvent::kMouseDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  click_event.button = blink::WebPointerProperties::Button::kLeft;
  click_event.SetPositionInWidget(15, 15);
  click_event.click_count = 1;
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  // Prompt the WebContents to dismiss the popup by clicking elsewhere.
  click_event.SetPositionInWidget(1, 1);
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  filter->Wait();

  gfx::Rect popup_rect = filter->last_initial_rect();

#if defined(OS_MACOSX)
  EXPECT_EQ(popup_rect.x(), 9);
  EXPECT_EQ(popup_rect.y(), 9);
#else
  EXPECT_EQ(popup_rect.x() - rwhv_root->GetViewBounds().x(), 354);
  EXPECT_EQ(popup_rect.y() - rwhv_root->GetViewBounds().y(), 154);
#endif

  // Save the screen rect for b_node. Since it updates asynchronously from
  // the script command that changes it, we need to wait for it to change
  // before attempting to create the popup widget again.
  gfx::Rect last_b_node_bounds_rect =
      b_node->current_frame_host()->GetView()->GetViewBounds();

  std::string script =
      "var iframe = document.querySelector('iframe');"
      "iframe.style.position = 'absolute';"
      "iframe.style.left = 150;"
      "iframe.style.top = 150;";
  EXPECT_TRUE(ExecuteScript(root, script));

  filter->Reset();

  // Busy loop to wait for b_node's screen rect to get updated. There
  // doesn't seem to be any better way to find out when this happens.
  while (last_b_node_bounds_rect.x() ==
             b_node->current_frame_host()->GetView()->GetViewBounds().x() &&
         last_b_node_bounds_rect.y() ==
             b_node->current_frame_host()->GetView()->GetViewBounds().y()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  click_event.button = blink::WebPointerProperties::Button::kLeft;
  click_event.SetPositionInWidget(15, 15);
  click_event.click_count = 1;
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  click_event.SetPositionInWidget(1, 1);
  rwhv_c_node->ProcessMouseEvent(click_event, ui::LatencyInfo());

  filter->Wait();

  popup_rect = filter->last_initial_rect();

#if defined(OS_MACOSX)
  EXPECT_EQ(popup_rect.x(), 9);
  EXPECT_EQ(popup_rect.y(), 9);
#else
  EXPECT_EQ(popup_rect.x() - rwhv_root->GetViewBounds().x(), 203);
  EXPECT_EQ(popup_rect.y() - rwhv_root->GetViewBounds().y(), 248);
#endif
}

// Test for https://crbug.com/526304, where a parent frame executes a
// remote-to-local navigation on a child frame and immediately removes the same
// child frame.  This test exercises the path where the detach happens before
// the provisional local frame is created.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateProxyAndDetachBeforeProvisionalFrameCreation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();
  EXPECT_EQ(2U, root->child_count());

  // Navigate the first child frame to 'about:blank' (which is a
  // remote-to-local transition), and then detach it.
  FrameDeletedObserver observer(root->child_at(0));
  std::string script =
      "var f = document.querySelector('iframe');"
      "f.contentWindow.location.href = 'about:blank';"
      "setTimeout(function() { document.body.removeChild(f); }, 0);";
  EXPECT_TRUE(ExecuteScript(root, script));
  observer.Wait();
  EXPECT_EQ(1U, root->child_count());

  // Make sure the main frame renderer does not crash and ignores the
  // navigation to the frame that's already been deleted.
  int child_count = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root, "domAutomationController.send(frames.length)", &child_count));
  EXPECT_EQ(1, child_count);
}

// Test for a variation of https://crbug.com/526304, where a child frame does a
// remote-to-local navigation, and the parent frame removes that child frame
// after the provisional local frame is created and starts to navigate, but
// before it commits.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateProxyAndDetachBeforeCommit) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();
  EXPECT_EQ(2U, root->child_count());
  FrameTreeNode* child = root->child_at(0);

  // Start a remote-to-local navigation for the child, but don't wait for
  // commit.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigationController::LoadURLParams params(same_site_url);
  params.transition_type = ui::PAGE_TRANSITION_LINK;
  params.frame_tree_node_id = child->frame_tree_node_id();
  child->navigator()->GetController()->LoadURLWithParams(params);

  // Tell parent to remove the first child.  This should happen after the
  // previous navigation starts but before it commits.
  FrameDeletedObserver observer(child);
  EXPECT_TRUE(ExecuteScript(
      root, "document.body.removeChild(document.querySelector('iframe'));"));
  observer.Wait();
  EXPECT_EQ(1U, root->child_count());

  // Make sure the a.com renderer does not crash.
  int child_count = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root, "domAutomationController.send(frames.length)", &child_count));
  EXPECT_EQ(1, child_count);
}

// Similar to NavigateProxyAndDetachBeforeCommit, but uses a synchronous
// navigation to about:blank and the parent removes the child frame in a load
// event handler for the subframe.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, NavigateAboutBlankAndDetach) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/remove_frame_on_load.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();
  EXPECT_EQ(1U, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_NE(shell()->web_contents()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());

  // Navigate the child frame to "about:blank" from the parent document and
  // wait for it to be removed.
  FrameDeletedObserver observer(child);
  EXPECT_TRUE(ExecuteScript(
      root, base::StringPrintf("f.src = '%s'", url::kAboutBlankURL)));
  observer.Wait();

  // Make sure the a.com renderer does not crash and the frame is removed.
  int child_count = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root, "domAutomationController.send(frames.length)", &child_count));
  EXPECT_EQ(0, child_count);
}

// Test for https://crbug.com/568670.  In A-embed-B, simultaneously have B
// create a new (local) child frame, and have A detach B's proxy.  The child
// frame creation sends an IPC to create a new proxy in A's process, and if
// that IPC arrives after the detach, the new frame's parent (a proxy) won't be
// available, and this shouldn't cause RenderFrameProxy::CreateFrameProxy to
// crash.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       RaceBetweenCreateChildFrameAndDetachParentProxy) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContents* contents = shell()->web_contents();
  FrameTreeNode* root =
      static_cast<WebContentsImpl*>(contents)->GetFrameTree()->root();

  // Simulate subframe B creating a new child frame in parallel to main frame A
  // detaching subframe B.  We can't use ExecuteScript in both A and B to do
  // this simultaneously, as that won't guarantee the timing that we want.
  // Instead, tell A to detach B and then send a fake proxy creation IPC to A
  // that would've come from create-child-frame code in B.  Prepare parameters
  // for that IPC ahead of the detach, while B's FrameTreeNode still exists.
  SiteInstance* site_instance_a = root->current_frame_host()->GetSiteInstance();
  RenderProcessHost* process_a =
      root->render_manager()->current_frame_host()->GetProcess();
  int new_routing_id = process_a->GetNextRoutingID();
  int view_routing_id =
      root->frame_tree()->GetRenderViewHost(site_instance_a)->GetRoutingID();
  int parent_routing_id =
      root->child_at(0)->render_manager()->GetProxyToParent()->GetRoutingID();

  // Tell main frame A to delete its subframe B.
  FrameDeletedObserver observer(root->child_at(0));
  EXPECT_TRUE(ExecuteScript(
      root, "document.body.removeChild(document.querySelector('iframe'));"));

  // Send the message to create a proxy for B's new child frame in A.  This
  // used to crash, as parent_routing_id refers to a proxy that doesn't exist
  // anymore.
  process_a->GetRendererInterface()->CreateFrameProxy(
      new_routing_id, view_routing_id, MSG_ROUTING_NONE, parent_routing_id,
      FrameReplicationState());

  // Ensure the subframe is detached in the browser process.
  observer.Wait();
  EXPECT_EQ(0U, root->child_count());

  // Make sure process A did not crash.
  int child_count = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root, "domAutomationController.send(frames.length)", &child_count));
  EXPECT_EQ(0, child_count);
}

// This test ensures that the RenderFrame isn't leaked in the renderer process
// if a pending cross-process navigation is cancelled. The test works by trying
// to create a new RenderFrame with the same routing id. If there is an
// entry with the same routing ID, a CHECK is hit and the process crashes.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       SubframePendingAndBackToSameSiteInstance) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Capture the FrameTreeNode this test will be navigating.
  FrameTreeNode* node = web_contents()->GetFrameTree()->root()->child_at(0);
  EXPECT_TRUE(node);
  EXPECT_NE(node->current_frame_host()->GetSiteInstance(),
            node->parent()->current_frame_host()->GetSiteInstance());

  // Navigate to the site of the parent, but to a page that will not commit.
  GURL same_site_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  NavigationStallDelegate stall_delegate(same_site_url);
  ResourceDispatcherHost::Get()->SetDelegate(&stall_delegate);
  {
    NavigationController::LoadURLParams params(same_site_url);
    params.transition_type = ui::PAGE_TRANSITION_LINK;
    params.frame_tree_node_id = node->frame_tree_node_id();
    node->navigator()->GetController()->LoadURLWithParams(params);
  }

  // Grab the routing id of the pending RenderFrameHost and set up a process
  // observer to ensure there is no crash when a new RenderFrame creation is
  // attempted.
  RenderProcessHost* process =
      IsBrowserSideNavigationEnabled()
          ? node->render_manager()->speculative_frame_host()->GetProcess()
          : node->render_manager()->pending_frame_host()->GetProcess();
  RenderProcessHostWatcher watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  int frame_routing_id =
      IsBrowserSideNavigationEnabled()
          ? node->render_manager()->speculative_frame_host()->GetRoutingID()
          : node->render_manager()->pending_frame_host()->GetRoutingID();
  int proxy_routing_id =
      node->render_manager()->GetProxyToParent()->GetRoutingID();

  // Now go to c.com so the navigation to a.com is cancelled and send an IPC
  // to create a new RenderFrame with the routing id of the previously pending
  // one.
  NavigateFrameToURL(node,
                     embedded_test_server()->GetURL("c.com", "/title2.html"));
  {
    mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
    params->routing_id = frame_routing_id;
    params->proxy_routing_id = proxy_routing_id;
    params->opener_routing_id = IPC::mojom::kRoutingIdNone;
    params->parent_routing_id =
        shell()->web_contents()->GetMainFrame()->GetRoutingID();
    params->previous_sibling_routing_id = IPC::mojom::kRoutingIdNone;
    params->widget_params = mojom::CreateFrameWidgetParams::New();
    params->widget_params->routing_id = IPC::mojom::kRoutingIdNone;
    params->widget_params->hidden = true;
    params->replication_state.name = "name";
    params->replication_state.unique_name = "name";
    process->GetRendererInterface()->CreateFrame(std::move(params));
  }

  // The test must wait for the process to exit, but if there is no leak, the
  // RenderFrame will be properly created and there will be no crash.
  // Therefore, navigate the main frame to completely different site, which
  // will cause the original process to exit cleanly.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("d.com", "/title3.html")));
  watcher.Wait();
  EXPECT_TRUE(watcher.did_exit_normally());

  ResourceDispatcherHost::Get()->SetDelegate(nullptr);
}

// This test ensures that the RenderFrame isn't leaked in the renderer process
// when a remote parent detaches a child frame. The test works by trying
// to create a new RenderFrame with the same routing id. If there is an
// entry with the same routing ID, a CHECK is hit and the process crashes.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, ParentDetachRemoteChild) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  EXPECT_EQ(2U, contents->GetFrameTree()->root()->child_count());

  // Capture the FrameTreeNode this test will be navigating.
  FrameTreeNode* node = contents->GetFrameTree()->root()->child_at(0);
  EXPECT_TRUE(node);
  EXPECT_NE(node->current_frame_host()->GetSiteInstance(),
            node->parent()->current_frame_host()->GetSiteInstance());

  // Grab the routing id of the first child RenderFrameHost and set up a process
  // observer to ensure there is no crash when a new RenderFrame creation is
  // attempted.
  RenderProcessHost* process = node->current_frame_host()->GetProcess();
  RenderProcessHostWatcher watcher(
      process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  int frame_routing_id = node->current_frame_host()->GetRoutingID();
  int widget_routing_id =
      node->current_frame_host()->GetRenderWidgetHost()->GetRoutingID();
  int parent_routing_id =
      node->parent()->render_manager()->GetRoutingIdForSiteInstance(
          node->current_frame_host()->GetSiteInstance());

  // Have the parent frame remove the child frame from its DOM. This should
  // result in the child RenderFrame being deleted in the remote process.
  EXPECT_TRUE(ExecuteScript(contents,
                            "document.body.removeChild("
                            "document.querySelectorAll('iframe')[0])"));
  EXPECT_EQ(1U, contents->GetFrameTree()->root()->child_count());

  {
    mojom::CreateFrameParamsPtr params = mojom::CreateFrameParams::New();
    params->routing_id = frame_routing_id;
    params->proxy_routing_id = IPC::mojom::kRoutingIdNone;
    params->opener_routing_id = IPC::mojom::kRoutingIdNone;
    params->parent_routing_id = parent_routing_id;
    params->previous_sibling_routing_id = IPC::mojom::kRoutingIdNone;
    params->widget_params = mojom::CreateFrameWidgetParams::New();
    params->widget_params->routing_id = widget_routing_id;
    params->widget_params->hidden = true;
    params->replication_state.name = "name";
    params->replication_state.unique_name = "name";
    process->GetRendererInterface()->CreateFrame(std::move(params));
  }

  // The test must wait for the process to exit, but if there is no leak, the
  // RenderFrame will be properly created and there will be no crash.
  // Therefore, navigate the remaining subframe to completely different site,
  // which will cause the original process to exit cleanly.
  NavigateFrameToURL(contents->GetFrameTree()->root()->child_at(0),
                     embedded_test_server()->GetURL("d.com", "/title3.html"));
  watcher.Wait();
  EXPECT_TRUE(watcher.did_exit_normally());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, VisibilityChanged) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  EXPECT_EQ(shell()->web_contents()->GetLastCommittedURL(), main_url);

  GURL cross_site_url =
      embedded_test_server()->GetURL("oopif.com", "/title1.html");

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  TestNavigationObserver observer(shell()->web_contents());

  NavigateFrameToURL(root->child_at(0), cross_site_url);
  EXPECT_EQ(cross_site_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  RenderWidgetHostImpl* render_widget_host =
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost();
  EXPECT_FALSE(render_widget_host->is_hidden());

  std::string show_script =
      "document.querySelector('iframe').style.visibility = 'visible';";
  std::string hide_script =
      "document.querySelector('iframe').style.visibility = 'hidden';";

  // Verify that hiding leads to a notification from RenderWidgetHost.
  RenderWidgetHostVisibilityObserver hide_observer(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), false);
  EXPECT_TRUE(ExecuteScript(shell(), hide_script));
  EXPECT_TRUE(hide_observer.WaitUntilSatisfied());

  // Verify showing leads to a notification as well.
  RenderWidgetHostVisibilityObserver show_observer(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), true);
  EXPECT_TRUE(ExecuteScript(shell(), show_script));
  EXPECT_TRUE(show_observer.WaitUntilSatisfied());
}

// Verify that sandbox flags inheritance works across multiple levels of
// frames.  See https://crbug.com/576845.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, SandboxFlagsInheritance) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Set sandbox flags for child frame.
  EXPECT_TRUE(ExecuteScript(
      root, "document.querySelector('iframe').sandbox = 'allow-scripts';"));

  // Calculate expected flags.  Note that "allow-scripts" resets both
  // WebSandboxFlags::Scripts and WebSandboxFlags::AutomaticFeatures bits per
  // blink::parseSandboxPolicy().
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
      ~blink::WebSandboxFlags::kAutomaticFeatures;
  EXPECT_EQ(expected_flags, root->child_at(0)->pending_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            root->child_at(0)->effective_sandbox_flags());

  // Navigate child frame so that the sandbox flags take effect.  Use a page
  // with three levels of frames and make sure all frames properly inherit
  // sandbox flags.
  GURL frame_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b(c(d))"));
  NavigateFrameToURL(root->child_at(0), frame_url);

  // Wait for subframes to load as well.
  ASSERT_TRUE(WaitForLoadStop(shell()->web_contents()));

  // Check each new frame's sandbox flags on the browser process side.
  FrameTreeNode* b_child = root->child_at(0);
  FrameTreeNode* c_child = b_child->child_at(0);
  FrameTreeNode* d_child = c_child->child_at(0);
  EXPECT_EQ(expected_flags, b_child->effective_sandbox_flags());
  EXPECT_EQ(expected_flags, c_child->effective_sandbox_flags());
  EXPECT_EQ(expected_flags, d_child->effective_sandbox_flags());

  // Check whether each frame is sandboxed on the renderer side, by seeing if
  // each frame's origin is unique ("null").
  EXPECT_EQ("null", GetDocumentOrigin(b_child));
  EXPECT_EQ("null", GetDocumentOrigin(c_child));
  EXPECT_EQ("null", GetDocumentOrigin(d_child));
}

// Check that sandbox flags are not inherited before they take effect.  Create
// a child frame, update its sandbox flags but don't navigate the frame, and
// ensure that a new cross-site grandchild frame doesn't inherit the new flags
// (which shouldn't have taken effect).
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       SandboxFlagsNotInheritedBeforeNavigation) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Set sandbox flags for child frame.
  EXPECT_TRUE(ExecuteScript(
      root, "document.querySelector('iframe').sandbox = 'allow-scripts';"));

  // These flags should be pending but not take effect, since there's been no
  // navigation.
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
      ~blink::WebSandboxFlags::kAutomaticFeatures;
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(expected_flags, child->pending_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone, child->effective_sandbox_flags());

  // Add a new grandchild frame and navigate it cross-site.
  RenderFrameHostCreatedObserver frame_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(
      child, "document.body.appendChild(document.createElement('iframe'));"));
  frame_observer.Wait();

  FrameTreeNode* grandchild = child->child_at(0);
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestFrameNavigationObserver navigation_observer(grandchild);
  NavigateFrameToURL(grandchild, frame_url);
  navigation_observer.Wait();

  // Since the update flags haven't yet taken effect in its parent, this
  // grandchild frame should not be sandboxed.
  EXPECT_EQ(blink::WebSandboxFlags::kNone, grandchild->pending_sandbox_flags());
  EXPECT_EQ(blink::WebSandboxFlags::kNone,
            grandchild->effective_sandbox_flags());

  // Check that the grandchild frame isn't sandboxed on the renderer side.  If
  // sandboxed, its origin would be unique ("null").
  EXPECT_EQ(frame_url.GetOrigin().spec(), GetDocumentOrigin(grandchild) + "/");
}

// Verify that popups opened from sandboxed frames inherit sandbox flags from
// their opener, and that they keep these inherited flags after being navigated
// cross-site.  See https://crbug.com/483584.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NewPopupInheritsSandboxFlagsFromOpener) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Set sandbox flags for child frame.
  EXPECT_TRUE(ExecuteScript(root,
                            "document.querySelector('iframe').sandbox = "
                            "    'allow-scripts allow-popups';"));

  // Calculate expected flags.  Note that "allow-scripts" resets both
  // WebSandboxFlags::Scripts and WebSandboxFlags::AutomaticFeatures bits per
  // blink::parseSandboxPolicy().
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
      ~blink::WebSandboxFlags::kAutomaticFeatures &
      ~blink::WebSandboxFlags::kPopups;
  EXPECT_EQ(expected_flags, root->child_at(0)->pending_sandbox_flags());

  // Navigate child frame cross-site.  The sandbox flags should take effect.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  NavigateFrameToURL(root->child_at(0), frame_url);
  frame_observer.Wait();
  EXPECT_EQ(expected_flags, root->child_at(0)->effective_sandbox_flags());

  // Verify that they've also taken effect on the renderer side.  The sandboxed
  // frame's origin should be unique.
  EXPECT_EQ("null", GetDocumentOrigin(root->child_at(0)));

  // Open a popup named "foo" from the sandboxed child frame.
  Shell* foo_shell =
      OpenPopup(root->child_at(0), GURL(url::kAboutBlankURL), "foo");
  EXPECT_TRUE(foo_shell);

  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetFrameTree()
          ->root();

  // Check that the sandbox flags for new popup are correct in the browser
  // process.
  EXPECT_EQ(expected_flags, foo_root->effective_sandbox_flags());

  // The popup's origin should be unique, since it's sandboxed.
  EXPECT_EQ("null", GetDocumentOrigin(foo_root));

  // Navigate the popup cross-site.  This should keep the unique origin and the
  // inherited sandbox flags.
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  TestFrameNavigationObserver popup_observer(foo_root);
  EXPECT_TRUE(
      ExecuteScript(foo_root, "location.href = '" + c_url.spec() + "';"));
  popup_observer.Wait();
  EXPECT_EQ(c_url, foo_shell->web_contents()->GetLastCommittedURL());

  // Confirm that the popup is still sandboxed, both on browser and renderer
  // sides.
  EXPECT_EQ(expected_flags, foo_root->effective_sandbox_flags());
  EXPECT_EQ("null", GetDocumentOrigin(foo_root));
}

// Verify that popups opened from frames sandboxed with the
// "allow-popups-to-escape-sandbox" directive do *not* inherit sandbox flags
// from their opener.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       OpenUnsandboxedPopupFromSandboxedFrame) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // It is safe to obtain the root frame tree node here, as it doesn't change.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Set sandbox flags for child frame, specifying that popups opened from it
  // should not be sandboxed.
  EXPECT_TRUE(ExecuteScript(
      root,
      "document.querySelector('iframe').sandbox = "
      "    'allow-scripts allow-popups allow-popups-to-escape-sandbox';"));

  // Set expected flags for the child frame.  Note that "allow-scripts" resets
  // both WebSandboxFlags::Scripts and WebSandboxFlags::AutomaticFeatures bits
  // per blink::parseSandboxPolicy().
  blink::WebSandboxFlags expected_flags =
      blink::WebSandboxFlags::kAll & ~blink::WebSandboxFlags::kScripts &
      ~blink::WebSandboxFlags::kAutomaticFeatures &
      ~blink::WebSandboxFlags::kPopups &
      ~blink::WebSandboxFlags::kPropagatesToAuxiliaryBrowsingContexts;
  EXPECT_EQ(expected_flags, root->child_at(0)->pending_sandbox_flags());

  // Navigate child frame cross-site.  The sandbox flags should take effect.
  GURL frame_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  TestFrameNavigationObserver frame_observer(root->child_at(0));
  NavigateFrameToURL(root->child_at(0), frame_url);
  frame_observer.Wait();
  EXPECT_EQ(expected_flags, root->child_at(0)->effective_sandbox_flags());

  // Open a cross-site popup named "foo" from the child frame.
  GURL b_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  Shell* foo_shell = OpenPopup(root->child_at(0), b_url, "foo");
  EXPECT_TRUE(foo_shell);

  FrameTreeNode* foo_root =
      static_cast<WebContentsImpl*>(foo_shell->web_contents())
          ->GetFrameTree()
          ->root();

  // Check that the sandbox flags for new popup are correct in the browser
  // process.  They should not have been inherited.
  EXPECT_EQ(blink::WebSandboxFlags::kNone, foo_root->effective_sandbox_flags());

  // The popup's origin should match |b_url|, since it's not sandboxed.
  std::string popup_origin;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      foo_root, "domAutomationController.send(document.origin)",
      &popup_origin));
  EXPECT_EQ(b_url.GetOrigin().spec(), popup_origin + "/");
}

// Tests that the WebContents is notified when passive mixed content is
// displayed in an OOPIF. The test ignores cert errors so that an HTTPS
// iframe can be loaded from a site other than localhost (the
// EmbeddedTestServer serves a certificate that is valid for localhost).
// This test crashes on Windows under Dr. Memory, see https://crbug.com/600942.
#if defined(OS_WIN)
#define MAYBE_PassiveMixedContentInIframe DISABLED_PassiveMixedContentInIframe
#else
#define MAYBE_PassiveMixedContentInIframe PassiveMixedContentInIframe
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessIgnoreCertErrorsBrowserTest,
                       MAYBE_PassiveMixedContentInIframe) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL iframe_url(
      https_server.GetURL("/mixed-content/basic-passive-in-iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url));
  NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 SSLStatus::DISPLAYED_INSECURE_CONTENT));

  // When the subframe navigates, the WebContents should still be marked
  // as having displayed insecure content.
  GURL navigate_url(https_server.GetURL("/title1.html"));
  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  NavigateFrameToURL(root->child_at(0), navigate_url);
  entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 SSLStatus::DISPLAYED_INSECURE_CONTENT));

  // When the main frame navigates, it should no longer be marked as
  // displaying insecure content.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server.GetURL("b.com", "/title1.html")));
  entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));
}

// Tests that, when a parent frame is set to strictly block mixed
// content via Content Security Policy, child OOPIFs cannot display
// mixed content.
IN_PROC_BROWSER_TEST_F(SitePerProcessIgnoreCertErrorsBrowserTest,
                       PassiveMixedContentInIframeWithStrictBlocking) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL iframe_url_with_strict_blocking(https_server.GetURL(
      "/mixed-content/basic-passive-in-iframe-with-strict-blocking.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url_with_strict_blocking));
  NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  EXPECT_EQ(blink::kBlockAllMixedContent,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::kBlockAllMixedContent,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the subframe navigates, it should still be marked as enforcing
  // strict mixed content.
  GURL navigate_url(https_server.GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), navigate_url);
  EXPECT_EQ(blink::kBlockAllMixedContent,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::kBlockAllMixedContent,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the main frame navigates, it should no longer be marked as
  // enforcing strict mixed content.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server.GetURL("b.com", "/title1.html")));
  EXPECT_EQ(blink::kLeaveInsecureRequestsAlone,
            root->current_replication_state().insecure_request_policy);
}

// Tests that, when a parent frame is set to upgrade insecure requests
// via Content Security Policy, child OOPIFs will upgrade as well.
IN_PROC_BROWSER_TEST_F(SitePerProcessIgnoreCertErrorsBrowserTest,
                       PassiveMixedContentInIframeWithUpgrade) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  GURL iframe_url_with_upgrade(https_server.GetURL(
      "/mixed-content/basic-passive-in-iframe-with-upgrade.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url_with_upgrade));
  NavigationEntry* entry = web_contents->GetController().GetVisibleEntry();
  EXPECT_FALSE(!!(entry->GetSSL().content_status &
                  SSLStatus::DISPLAYED_INSECURE_CONTENT));

  FrameTreeNode* root = web_contents->GetFrameTree()->root();
  EXPECT_EQ(blink::kUpgradeInsecureRequests,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::kUpgradeInsecureRequests,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the subframe navigates, it should still be marked as upgrading
  // insecure requests.
  GURL navigate_url(https_server.GetURL("/title1.html"));
  NavigateFrameToURL(root->child_at(0), navigate_url);
  EXPECT_EQ(blink::kUpgradeInsecureRequests,
            root->current_replication_state().insecure_request_policy);
  EXPECT_EQ(
      blink::kUpgradeInsecureRequests,
      root->child_at(0)->current_replication_state().insecure_request_policy);

  // When the main frame navigates, it should no longer be marked as
  // upgrading insecure requests.
  EXPECT_TRUE(
      NavigateToURL(shell(), https_server.GetURL("b.com", "/title1.html")));
  EXPECT_EQ(blink::kLeaveInsecureRequestsAlone,
            root->current_replication_state().insecure_request_policy);
}

// Tests that active mixed content is blocked in an OOPIF. The test
// ignores cert errors so that an HTTPS iframe can be loaded from a site
// other than localhost (the EmbeddedTestServer serves a certificate
// that is valid for localhost).
IN_PROC_BROWSER_TEST_F(SitePerProcessIgnoreCertErrorsBrowserTest,
                       ActiveMixedContentInIframe) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  GURL iframe_url(
      https_server.GetURL("/mixed-content/basic-active-in-iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), iframe_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1U, root->child_count());
  FrameTreeNode* mixed_child = root->child_at(0)->child_at(0);
  ASSERT_TRUE(mixed_child);
  // The child iframe attempted to create a mixed iframe; this should
  // have been blocked, so the mixed iframe should not have committed a
  // load.
  EXPECT_FALSE(mixed_child->has_committed_real_load());
}

// Test that subresources with certificate errors get reported to the
// browser. That is, if https://example.test frames https://a.com which
// loads an image with certificate errors, the browser should be
// notified about the subresource with certificate errors and downgrade
// the UI appropriately.
IN_PROC_BROWSER_TEST_F(SitePerProcessIgnoreCertErrorsBrowserTest,
                       SubresourceWithCertificateErrors) {
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("content/test/data");
  SetupCrossSiteRedirector(&https_server);
  ASSERT_TRUE(https_server.Start());

  GURL url(https_server.GetURL(
      "example.test",
      "/mixed-content/non-redundant-cert-error-in-iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  NavigationEntry* entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);

  // The main page was loaded with certificate errors.
  EXPECT_TRUE(net::IsCertStatusError(entry->GetSSL().cert_status));

  // The image that the iframe loaded had certificate errors also, so
  // the page should be marked as having displayed subresources with
  // cert errors.
  EXPECT_TRUE(!!(entry->GetSSL().content_status &
                 SSLStatus::DISPLAYED_CONTENT_WITH_CERT_ERRORS));
}

// Test setting a cross-origin iframe to display: none.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, CrossSiteIframeDisplayNone) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  RenderWidgetHost* root_render_widget_host =
      root->current_frame_host()->GetRenderWidgetHost();

  // Set the iframe to display: none.
  EXPECT_TRUE(ExecuteScript(
      shell(), "document.querySelector('iframe').style.display = 'none'"));

  // Waits until pending frames are done.
  std::unique_ptr<MainThreadFrameObserver> observer(
      new MainThreadFrameObserver(root_render_widget_host));
  observer->Wait();

  // Force the renderer to generate a new frame.
  EXPECT_TRUE(
      ExecuteScript(shell(), "document.body.style.background = 'black'"));

  // Waits for the next frame.
  observer->Wait();
}

// Test that a cross-origin iframe can be blocked by X-Frame-Options and CSP
// frame-ancestors.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CrossSiteIframeBlockedByXFrameOptionsOrCSP) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Add a load event handler for the iframe element.
  EXPECT_TRUE(ExecuteScript(shell(),
                            "document.querySelector('iframe').onload = "
                            "    function() { document.title = 'loaded'; };"));

  GURL blocked_urls[] = {
    embedded_test_server()->GetURL("b.com", "/frame-ancestors-none.html"),
    embedded_test_server()->GetURL("b.com", "/x-frame-options-deny.html")
  };

  for (size_t i = 0; i < arraysize(blocked_urls); ++i) {
    EXPECT_TRUE(ExecuteScript(shell(), "document.title = 'not loaded';"));
    base::string16 expected_title(base::UTF8ToUTF16("loaded"));
    TitleWatcher title_watcher(shell()->web_contents(), expected_title);

    // Navigate the subframe to a blocked URL.
    TestNavigationObserver load_observer(shell()->web_contents());
    EXPECT_TRUE(ExecuteScript(shell(), "frames[0].location.href = '" +
                                           blocked_urls[i].spec() + "';"));
    load_observer.Wait();

    // The blocked frame's origin should become unique.
    EXPECT_EQ("null", root->child_at(0)->current_origin().Serialize());

    // Ensure that we don't use the blocked URL as the blocked frame's last
    // committed URL (see https://crbug.com/622385).
    EXPECT_NE(root->child_at(0)->current_frame_host()->GetLastCommittedURL(),
              blocked_urls[i]);

    // The blocked frame should still fire a load event in its parent's process.
    EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

    // Check that the current RenderFrameHost has stopped loading.
    EXPECT_FALSE(root->child_at(0)->current_frame_host()->is_loading());

    // The blocked navigation should behave like an empty 200 response. Make
    // sure that the frame's document.title is empty: this double-checks both
    // that the blocked URL's contents wasn't loaded, and that the old page
    // isn't active anymore (both of these pages have non-empty titles).
    std::string frame_title;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        root->child_at(0), "domAutomationController.send(document.title)",
        &frame_title));
    EXPECT_EQ("", frame_title);

    // Navigate the subframe to another cross-origin page and ensure that this
    // navigation succeeds.  Use a renderer-initiated navigation to test the
    // transfer logic, which used to have some issues with this.
    GURL c_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "child-0", c_url));
    EXPECT_EQ(c_url, root->child_at(0)->current_url());

    // When a page gets blocked due to XFO or CSP, it is sandboxed with the
    // SandboxOrigin flag (i.e., its origin is set to be unique) to ensure that
    // the blocked page is seen as cross-origin. However, those flags shouldn't
    // affect future navigations for a frame. Verify this for the above
    // navigation.
    EXPECT_EQ(c_url.GetOrigin().spec(),
              root->child_at(0)->current_origin().Serialize() + "/");
    EXPECT_EQ(blink::WebSandboxFlags::kNone,
              root->child_at(0)->effective_sandbox_flags());
  }
}

// Test that a cross-origin frame's navigation can be blocked by CSP frame-src.
// In this version of a test, CSP comes from HTTP headers.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CrossSiteIframeBlockedByParentCSPFromHeaders) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/frame-src-self-and-b.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Sanity-check that the test page has the expected shape for testing.
  GURL old_subframe_url(
      embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_FALSE(root->child_at(0)->HasSameOrigin(*root));
  EXPECT_EQ(old_subframe_url, root->child_at(0)->current_url());
  const std::vector<ContentSecurityPolicyHeader>& root_csp =
      root->current_replication_state().accumulated_csp_headers;
  EXPECT_EQ(1u, root_csp.size());
  EXPECT_EQ("frame-src 'self' http://b.com:*", root_csp[0].header_value);

  // Monitor subframe's load events via main frame's title.
  EXPECT_TRUE(ExecuteScript(shell(),
                            "document.querySelector('iframe').onload = "
                            "    function() { document.title = 'loaded'; };"));
  EXPECT_TRUE(ExecuteScript(shell(), "document.title = 'not loaded';"));
  base::string16 expected_title(base::UTF8ToUTF16("loaded"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Try to navigate the subframe to a blocked URL.
  TestNavigationObserver load_observer(shell()->web_contents());
  GURL blocked_url = embedded_test_server()->GetURL("c.com", "/title3.html");
  EXPECT_TRUE(ExecuteScript(root->child_at(0), "window.location.href = '" +
                                                   blocked_url.spec() + "';"));

  // The blocked frame should still fire a load event in its parent's process.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Check that the current RenderFrameHost has stopped loading.
  if (root->child_at(0)->current_frame_host()->is_loading()) {
    if (!IsBrowserSideNavigationEnabled())
      ADD_FAILURE() << "Blocked RenderFrameHost shouldn't be loading anything";
    load_observer.Wait();
  }

  // The last successful url shouldn't be the blocked url.
  EXPECT_EQ(old_subframe_url,
            root->child_at(0)->current_frame_host()->last_successful_url());

  if (IsBrowserSideNavigationEnabled()) {
    // The blocked frame should go to an error page. Errors currently commit
    // with the URL of the blocked page.
    EXPECT_EQ(blocked_url, root->child_at(0)->current_url());

    // The page should get the title of an error page (i.e "") and not the
    // title of the blocked page.
    std::string frame_title;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        root->child_at(0), "domAutomationController.send(document.title)",
        &frame_title));
    EXPECT_EQ("", frame_title);
  } else {
    // The blocked frame should stay at the old location.
    EXPECT_EQ(old_subframe_url, root->child_at(0)->current_url());

    // The blocked frame should keep the old title.
    std::string frame_title;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        root->child_at(0), "domAutomationController.send(document.title)",
        &frame_title));
    EXPECT_EQ("Title Of Awesomeness", frame_title);
  }

  // Navigate to a URL without CSP.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Verify that the frame's CSP got correctly reset to an empty set.
  EXPECT_EQ(0u,
            root->current_replication_state().accumulated_csp_headers.size());
}

// Test that a cross-origin frame's navigation can be blocked by CSP frame-src.
// In this version of a test, CSP comes from a <meta> element added after the
// page has already loaded.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CrossSiteIframeBlockedByParentCSPFromMeta) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Navigate the subframe to a location we will disallow in the future.
  GURL old_subframe_url(
      embedded_test_server()->GetURL("b.com", "/title2.html"));
  NavigateFrameToURL(root->child_at(0), old_subframe_url);

  // Add frame-src CSP via a new <meta> element.
  EXPECT_TRUE(ExecuteScript(
      shell(),
      "var meta = document.createElement('meta');"
      "meta.httpEquiv = 'Content-Security-Policy';"
      "meta.content = 'frame-src https://a.com:*';"
      "document.getElementsByTagName('head')[0].appendChild(meta);"));

  // Sanity-check that the test page has the expected shape for testing.
  // (the CSP should not have an effect on the already loaded frames).
  EXPECT_FALSE(root->child_at(0)->HasSameOrigin(*root));
  EXPECT_EQ(old_subframe_url, root->child_at(0)->current_url());
  const std::vector<ContentSecurityPolicyHeader>& root_csp =
      root->current_replication_state().accumulated_csp_headers;
  EXPECT_EQ(1u, root_csp.size());
  EXPECT_EQ("frame-src https://a.com:*", root_csp[0].header_value);

  // Monitor subframe's load events via main frame's title.
  EXPECT_TRUE(ExecuteScript(shell(),
                            "document.querySelector('iframe').onload = "
                            "    function() { document.title = 'loaded'; };"));
  EXPECT_TRUE(ExecuteScript(shell(), "document.title = 'not loaded';"));
  base::string16 expected_title(base::UTF8ToUTF16("loaded"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Try to navigate the subframe to a blocked URL.
  TestNavigationObserver load_observer2(shell()->web_contents());
  GURL blocked_url = embedded_test_server()->GetURL("c.com", "/title3.html");
  EXPECT_TRUE(ExecuteScript(root->child_at(0), "window.location.href = '" +
                                                   blocked_url.spec() + "';"));

  // The blocked frame should still fire a load event in its parent's process.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Check that the current RenderFrameHost has stopped loading.
  if (root->child_at(0)->current_frame_host()->is_loading()) {
    if (!IsBrowserSideNavigationEnabled())
      ADD_FAILURE() << "Blocked RenderFrameHost shouldn't be loading anything";
    load_observer2.Wait();
  }

  // The last successful url shouldn't be the blocked url.
  EXPECT_EQ(old_subframe_url,
            root->child_at(0)->current_frame_host()->last_successful_url());

  if (IsBrowserSideNavigationEnabled()) {
    // The blocked frame should go to an error page. Errors currently commit
    // with the URL of the blocked page.
    EXPECT_EQ(blocked_url, root->child_at(0)->current_url());

    // The page should get the title of an error page (i.e "") and not the
    // title of the blocked page.
    std::string frame_title;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        root->child_at(0), "domAutomationController.send(document.title)",
        &frame_title));
    EXPECT_EQ("", frame_title);
  } else {
    // The blocked frame should stay at the old location.
    EXPECT_EQ(old_subframe_url, root->child_at(0)->current_url());

    // The blocked frame should keep the old title.
    std::string frame_title;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        root->child_at(0), "domAutomationController.send(document.title)",
        &frame_title));
    EXPECT_EQ("Title Of Awesomeness", frame_title);
  }
}

// Test that a cross-origin frame's navigation can be blocked by CSP frame-src.
// In this version of a test, CSP is inherited by srcdoc iframe from a parent
// that declared CSP via HTTP headers.  Cross-origin frame navigating to a
// blocked location is a child of the srcdoc iframe.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CrossSiteIframeBlockedByCSPInheritedBySrcDocParent) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/frame-src-self-and-b.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* srcdoc_frame = root->child_at(1);
  EXPECT_TRUE(srcdoc_frame != nullptr);
  FrameTreeNode* navigating_frame = srcdoc_frame->child_at(0);
  EXPECT_TRUE(navigating_frame != nullptr);

  // Sanity-check that the test page has the expected shape for testing.
  // (the CSP should not have an effect on the already loaded frames).
  GURL old_subframe_url(
      embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_TRUE(srcdoc_frame->HasSameOrigin(*root));
  EXPECT_FALSE(srcdoc_frame->HasSameOrigin(*navigating_frame));
  EXPECT_EQ(old_subframe_url, navigating_frame->current_url());
  const std::vector<ContentSecurityPolicyHeader>& srcdoc_csp =
      srcdoc_frame->current_replication_state().accumulated_csp_headers;
  EXPECT_EQ(1u, srcdoc_csp.size());
  EXPECT_EQ("frame-src 'self' http://b.com:*", srcdoc_csp[0].header_value);

  // Monitor navigating_frame's load events via srcdoc_frame posting
  // a message to the parent frame.
  EXPECT_TRUE(
      ExecuteScript(root,
                    "window.addEventListener('message', function(event) {"
                    "  document.title = event.data;"
                    "});"));
  EXPECT_TRUE(ExecuteScript(
      srcdoc_frame,
      "document.querySelector('iframe').onload = "
      "    function() { window.top.postMessage('loaded', '*'); };"));
  EXPECT_TRUE(ExecuteScript(shell(), "document.title = 'not loaded';"));
  base::string16 expected_title(base::UTF8ToUTF16("loaded"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);

  // Try to navigate the subframe to a blocked URL.
  TestNavigationObserver load_observer2(shell()->web_contents());
  GURL blocked_url = embedded_test_server()->GetURL("c.com", "/title3.html");
  EXPECT_TRUE(ExecuteScript(navigating_frame, "window.location.href = '" +
                                                  blocked_url.spec() + "';"));

  // The blocked frame should still fire a load event in its parent's process.
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());

  // Check that the current RenderFrameHost has stopped loading.
  if (navigating_frame->current_frame_host()->is_loading()) {
    if (!IsBrowserSideNavigationEnabled())
      ADD_FAILURE() << "Blocked RenderFrameHost shouldn't be loading anything";
    load_observer2.Wait();
  }

  // The last successful url shouldn't be the blocked url.
  EXPECT_EQ(old_subframe_url,
            navigating_frame->current_frame_host()->last_successful_url());

  if (IsBrowserSideNavigationEnabled()) {
    // The blocked frame should go to an error page. Errors currently commit
    // with the URL of the blocked page.
    EXPECT_EQ(blocked_url, navigating_frame->current_url());

    // The page should get the title of an error page (i.e "") and not the
    // title of the blocked page.
    std::string frame_title;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        navigating_frame, "domAutomationController.send(document.title)",
        &frame_title));
    EXPECT_EQ("", frame_title);
  } else {
    // The blocked frame should stay at the old location.
    EXPECT_EQ(old_subframe_url, navigating_frame->current_url());

    // The blocked frame should keep the old title.
    std::string frame_title;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        navigating_frame, "domAutomationController.send(document.title)",
        &frame_title));
    EXPECT_EQ("Title Of Awesomeness", frame_title);
  }

  // Navigate the subframe to a URL without CSP.
  NavigateFrameToURL(srcdoc_frame,
                     embedded_test_server()->GetURL("a.com", "/title1.html"));

  // Verify that the frame's CSP got correctly reset to an empty set.
  EXPECT_EQ(
      0u,
      srcdoc_frame->current_replication_state().accumulated_csp_headers.size());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, ScreenCoordinates) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  const char* properties[] = {"screenX", "screenY", "outerWidth",
                              "outerHeight"};

  for (const char* property : properties) {
    std::string script = "window.domAutomationController.send(window.";
    script += property;
    script += ");";
    int root_value = 1;
    int child_value = 2;
    EXPECT_TRUE(ExecuteScriptAndExtractInt(root, script.c_str(), &root_value));

    EXPECT_TRUE(
        ExecuteScriptAndExtractInt(child, script.c_str(), &child_value));

    EXPECT_EQ(root_value, child_value);
  }
}

// Tests that the swapped out state on RenderViewHost is properly reset when
// the main frame is navigated to the same SiteInstance as one of its child
// frames.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateMainFrameToChildSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();
  EXPECT_EQ(1U, root->child_count());

  // Ensure the RenderViewHost for the SiteInstance of the child is considered
  // in swapped out state.
  RenderViewHostImpl* rvh = contents->GetFrameTree()->GetRenderViewHost(
      root->child_at(0)->current_frame_host()->GetSiteInstance());
  EXPECT_TRUE(rvh->is_swapped_out_);

  // Have the child frame navigate its parent to its SiteInstance.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  std::string script = base::StringPrintf(
      "window.domAutomationController.send("
      "parent.location = '%s');",
      b_url.spec().c_str());

  // Ensure the child has received a user gesture, so that it has permission
  // to framebust.
  SimulateMouseClick(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), 1, 1);
  TestFrameNavigationObserver frame_observer(root);
  EXPECT_TRUE(ExecuteScript(root->child_at(0), script));
  frame_observer.Wait();
  EXPECT_EQ(b_url, root->current_url());

  // Verify that the same RenderViewHost is preserved and that it is no longer
  // in swapped out state.
  EXPECT_EQ(rvh, contents->GetFrameTree()->GetRenderViewHost(
                     root->current_frame_host()->GetSiteInstance()));
  EXPECT_FALSE(rvh->is_swapped_out_);
}

// Helper class to wait for a ChildProcessHostMsg_ShutdownRequest message to
// arrive.
class ShutdownRequestMessageFilter : public BrowserMessageFilter {
 public:
  ShutdownRequestMessageFilter()
      : BrowserMessageFilter(ChildProcessMsgStart),
        message_loop_runner_(new MessageLoopRunner) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    if (message.type() == ChildProcessHostMsg_ShutdownRequest::ID) {
      content::BrowserThread::PostTask(
          content::BrowserThread::UI, FROM_HERE,
          base::Bind(&ShutdownRequestMessageFilter::OnShutdownRequest, this));
    }
    return false;
  }

  void OnShutdownRequest() { message_loop_runner_->Quit(); }

  void Wait() { message_loop_runner_->Run(); }

 private:
  ~ShutdownRequestMessageFilter() override {}

  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(ShutdownRequestMessageFilter);
};

// Test for https://crbug.com/568836.  From an A-embed-B page, navigate the
// subframe from B to A.  This cleans up the process for B, but the test delays
// the browser side from killing the B process right away.  This allows the
// B process to process two ViewMsg_Close messages sent to the subframe's
// RenderWidget and to the RenderView, in that order.  In the bug, the latter
// crashed while detaching the subframe's LocalFrame (triggered as part of
// closing the RenderView), because this tried to access the subframe's
// WebFrameWidget (from RenderFrameImpl::didChangeSelection), which had already
// been cleared by the former.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CloseSubframeWidgetAndViewOnProcessExit) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // "Select all" in the subframe.  The bug only happens if there's a selection
  // change, which triggers the path through didChangeSelection.
  root->child_at(0)->current_frame_host()->Send(new InputMsg_SelectAll(
      root->child_at(0)->current_frame_host()->GetRoutingID()));

  // Prevent b.com process from terminating right away once the subframe
  // navigates away from b.com below.  This is necessary so that the renderer
  // process has time to process the closings of RenderWidget and RenderView,
  // which is where the original bug was triggered.  Incrementing worker
  // RefCount will cause RenderProcessHostImpl::Cleanup to forego process
  // termination.
  RenderProcessHost* subframe_process =
      root->child_at(0)->current_frame_host()->GetProcess();
  subframe_process->IncrementSharedWorkerRefCount();

  // Navigate the subframe away from b.com.  Since this is the last active
  // frame in the b.com process, this causes the RenderWidget and RenderView to
  // be closed.  If this succeeds without crashing, the renderer will release
  // the process and send a ChildProcessHostMsg_ShutdownRequest to the browser
  // process to ask whether it's ok to terminate.  Thus, wait for this message
  // to ensure that the RenderView and widget were closed without crashing.
  scoped_refptr<ShutdownRequestMessageFilter> filter =
      new ShutdownRequestMessageFilter();
  subframe_process->AddFilter(filter.get());
  NavigateFrameToURL(root->child_at(0),
                     embedded_test_server()->GetURL("a.com", "/title1.html"));
  filter->Wait();

  // TODO(alexmos): Navigating the subframe back to b.com at this point would
  // trigger the race in https://crbug.com/535246, where the browser process
  // tries to reuse the b.com process thinking it's still initialized, whereas
  // the process has actually been destroyed by the renderer (but the browser
  // process hasn't heard the OnChannelError yet).  This race will need to be
  // fixed.

  subframe_process->DecrementSharedWorkerRefCount();
}

// Tests that an input event targeted to a out-of-process iframe correctly
// triggers a user interaction notification for WebContentsObservers.
// This is used for browser features such as download request limiting and
// launching multiple external protocol handlers, which can block repeated
// actions from a page when a user is not interacting with the page.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       UserInteractionForChildFrameTest) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  UserInteractionObserver observer(web_contents());

  // Target an event to the child frame's RenderWidgetHostView.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  SimulateMouseClick(
      root->child_at(0)->current_frame_host()->GetRenderWidgetHost(), 5, 5);

  EXPECT_TRUE(observer.WasUserInteractionReceived());

  // Target an event to the main frame.
  observer.Reset();
  SimulateMouseClick(root->current_frame_host()->GetRenderWidgetHost(), 1, 1);

  EXPECT_TRUE(observer.WasUserInteractionReceived());
}

// Ensures that navigating to data: URLs present in session history will
// correctly commit the navigation in the same process as the parent frame.
// See https://crbug.com/606996.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateSubframeToDataUrlInSessionHistory) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(2U, root->child_count());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  TestNavigationObserver observer(shell()->web_contents());
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe to a data URL, which will commit in a new SiteInstance.
  GURL data_url("data:text/html,dataurl");
  NavigateFrameToURL(child, data_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(data_url, observer.last_navigation_url());
  scoped_refptr<SiteInstanceImpl> orig_site_instance =
    child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(), orig_site_instance);

  // Navigate it to another cross-site url.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  NavigateFrameToURL(child, cross_site_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(cross_site_url, observer.last_navigation_url());
  EXPECT_EQ(3, web_contents()->GetController().GetEntryCount());
  EXPECT_NE(orig_site_instance, child->current_frame_host()->GetSiteInstance());

  // Go back and ensure the data: URL committed in the same SiteInstance as the
  // original navigation.
  EXPECT_TRUE(web_contents()->GetController().CanGoBack());
  TestFrameNavigationObserver frame_observer(child);
  web_contents()->GetController().GoBack();
  frame_observer.WaitForCommit();
  EXPECT_EQ(orig_site_instance, child->current_frame_host()->GetSiteInstance());
}

// Ensures that navigating to about:blank URLs present in session history will
// correctly commit the navigation in the same process as the one used for
// the original navigation.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateSubframeToAboutBlankInSessionHistory) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(2U, root->child_count());
  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   |--Site B ------- proxies for A\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  TestNavigationObserver observer(shell()->web_contents());
  FrameTreeNode* child = root->child_at(0);

  // Navigate iframe to about:blank, which will commit in a new SiteInstance.
  GURL about_blank_url("about:blank");
  NavigateFrameToURL(child, about_blank_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(about_blank_url, observer.last_navigation_url());
  scoped_refptr<SiteInstanceImpl> orig_site_instance =
    child->current_frame_host()->GetSiteInstance();
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(), orig_site_instance);

  // Navigate it to another cross-site url.
  GURL cross_site_url(embedded_test_server()->GetURL("c.com", "/title1.html"));
  NavigateFrameToURL(child, cross_site_url);
  EXPECT_TRUE(observer.last_navigation_succeeded());
  EXPECT_EQ(cross_site_url, observer.last_navigation_url());
  EXPECT_EQ(3, web_contents()->GetController().GetEntryCount());
  EXPECT_NE(orig_site_instance, child->current_frame_host()->GetSiteInstance());

  // Go back and ensure the about:blank URL committed in the same SiteInstance
  // as the original navigation.
  EXPECT_TRUE(web_contents()->GetController().CanGoBack());
  TestFrameNavigationObserver frame_observer(child);
  web_contents()->GetController().GoBack();
  frame_observer.WaitForCommit();
  EXPECT_EQ(orig_site_instance, child->current_frame_host()->GetSiteInstance());
}

// Tests that there are no crashes if a subframe is detached in its unload
// handler. See https://crbug.com/590054.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, DetachInUnloadHandler) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  int child_count = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root->child_at(0), "window.domAutomationController.send(frames.length);",
      &child_count));
  EXPECT_EQ(1, child_count);

  RenderFrameDeletedObserver deleted_observer(
      root->child_at(0)->child_at(0)->current_frame_host());

  // Add an unload handler to the grandchild that causes it to be synchronously
  // detached, then navigate it.
  EXPECT_TRUE(ExecuteScript(
      root->child_at(0)->child_at(0),
      "window.onunload=function(e){\n"
      "    window.parent.document.getElementById('child-0').remove();\n"
      "};\n"));
  std::string script =
      std::string("window.document.getElementById('child-0').src = \"") +
      embedded_test_server()
          ->GetURL("c.com", "/cross_site_iframe_factory.html?c")
          .spec() +
      "\"";
  EXPECT_TRUE(ExecuteScript(root->child_at(0), script.c_str()));

  deleted_observer.WaitUntilDeleted();

  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root->child_at(0), "window.domAutomationController.send(frames.length);",
      &child_count));
  EXPECT_EQ(0, child_count);

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));
}

// Helper filter class to wait for a ShowCreatedWindow or ShowWidget message,
// record the routing ID from the message, and then drop the message.
const uint32_t kMessageClasses[] = {ViewMsgStart, FrameMsgStart};
class PendingWidgetMessageFilter : public BrowserMessageFilter {
 public:
  PendingWidgetMessageFilter()
      : BrowserMessageFilter(kMessageClasses, arraysize(kMessageClasses)),
        routing_id_(MSG_ROUTING_NONE),
        message_loop_runner_(new MessageLoopRunner) {}

  bool OnMessageReceived(const IPC::Message& message) override {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(PendingWidgetMessageFilter, message)
      IPC_MESSAGE_HANDLER(FrameHostMsg_ShowCreatedWindow, OnShowCreatedWindow)
      IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget, OnShowWidget)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  void Wait() {
    message_loop_runner_->Run();
  }

  int routing_id() { return routing_id_; }

 private:
  ~PendingWidgetMessageFilter() override {}

  void OnShowCreatedWindow(int pending_widget_routing_id,
                           WindowOpenDisposition disposition,
                           const gfx::Rect& initial_rect,
                           bool user_gesture) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&PendingWidgetMessageFilter::OnReceivedRoutingIDOnUI, this,
                   pending_widget_routing_id));
  }

  void OnShowWidget(int routing_id, const gfx::Rect& initial_rect) {
    content::BrowserThread::PostTask(
        content::BrowserThread::UI, FROM_HERE,
        base::Bind(&PendingWidgetMessageFilter::OnReceivedRoutingIDOnUI, this,
                   routing_id));
  }

  void OnReceivedRoutingIDOnUI(int widget_routing_id) {
    routing_id_ = widget_routing_id;
    message_loop_runner_->Quit();
  }

  int routing_id_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(PendingWidgetMessageFilter);
};

// Test for https://crbug.com/612276.  Simultaneously open two new windows from
// two subframes in different processes, where each subframe process's next
// routing ID is the same.  Make sure that both windows are created properly.
//
// Each new window requires two IPCs to first create it (handled by
// CreateNewWindow) and then show it (ShowCreatedWindow).  In the bug, both
// CreateNewWindow calls arrived before the ShowCreatedWindow calls, resulting
// in the two pending windows colliding in the pending WebContents map, which
// used to be keyed only by routing_id.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       TwoSubframesCreatePopupsSimultaneously) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);
  RenderProcessHost* process1 = child1->current_frame_host()->GetProcess();
  RenderProcessHost* process2 = child2->current_frame_host()->GetProcess();

  // Call window.open simultaneously in both subframes to create two popups.
  // Wait for and then drop both FrameHostMsg_ShowCreatedWindow messages.  This
  // will ensure that both CreateNewWindow calls happen before either
  // ShowCreatedWindow call.
  scoped_refptr<PendingWidgetMessageFilter> filter1 =
      new PendingWidgetMessageFilter();
  process1->AddFilter(filter1.get());
  EXPECT_TRUE(ExecuteScript(child1, "window.open();"));
  filter1->Wait();

  scoped_refptr<PendingWidgetMessageFilter> filter2 =
      new PendingWidgetMessageFilter();
  process2->AddFilter(filter2.get());
  EXPECT_TRUE(ExecuteScript(child2, "window.open();"));
  filter2->Wait();

  // At this point, we should have two pending WebContents.
  EXPECT_TRUE(base::ContainsKey(
      web_contents()->pending_contents_,
      std::make_pair(process1->GetID(), filter1->routing_id())));
  EXPECT_TRUE(base::ContainsKey(
      web_contents()->pending_contents_,
      std::make_pair(process2->GetID(), filter2->routing_id())));

  // Both subframes were set up in the same way, so the next routing ID for the
  // new popup windows should match up (this led to the collision in the
  // pending contents map in the original bug).
  EXPECT_EQ(filter1->routing_id(), filter2->routing_id());

  // Now, simulate that both FrameHostMsg_ShowCreatedWindow messages arrive by
  // showing both of the pending WebContents.
  web_contents()->ShowCreatedWindow(process1->GetID(), filter1->routing_id(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    gfx::Rect(), true);
  web_contents()->ShowCreatedWindow(process2->GetID(), filter2->routing_id(),
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB,
                                    gfx::Rect(), true);

  // Verify that both shells were properly created.
  EXPECT_EQ(3u, Shell::windows().size());
}

// Test for https://crbug.com/612276.  Similar to
// TwoSubframesOpenWindowsSimultaneously, but use popup menu widgets instead of
// windows.
//
// The plumbing that this test is verifying is not utilized on Mac/Android,
// where popup menus don't create a popup RenderWidget, but rather they trigger
// a FrameHostMsg_ShowPopup to ask the browser to build and display the actual
// popup using native controls.
#if !defined(OS_MACOSX) && !defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       TwoSubframesCreatePopupMenuWidgetsSimultaneously) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);
  RenderProcessHost* process1 = child1->current_frame_host()->GetProcess();
  RenderProcessHost* process2 = child2->current_frame_host()->GetProcess();

  // Navigate both subframes to a page with a <select> element.
  NavigateFrameToURL(child1, embedded_test_server()->GetURL(
      "b.com", "/site_isolation/page-with-select.html"));
  NavigateFrameToURL(child2, embedded_test_server()->GetURL(
      "c.com", "/site_isolation/page-with-select.html"));

  // Open both <select> menus by focusing each item and sending a space key
  // at the focused node. This creates a popup widget in both processes.
  // Wait for and then drop the ViewHostMsg_ShowWidget messages, so that both
  // widgets are left in pending-but-not-shown state.
  NativeWebKeyboardEvent event(blink::WebKeyboardEvent::kChar,
                               blink::WebInputEvent::kNoModifiers,
                               blink::WebInputEvent::kTimeStampForTesting);
  event.text[0] = ' ';

  scoped_refptr<PendingWidgetMessageFilter> filter1 =
      new PendingWidgetMessageFilter();
  process1->AddFilter(filter1.get());
  EXPECT_TRUE(ExecuteScript(child1, "focusSelectMenu();"));
  child1->current_frame_host()->GetRenderWidgetHost()->ForwardKeyboardEvent(
      event);
  filter1->Wait();

  scoped_refptr<PendingWidgetMessageFilter> filter2 =
      new PendingWidgetMessageFilter();
  process2->AddFilter(filter2.get());
  EXPECT_TRUE(ExecuteScript(child2, "focusSelectMenu();"));
  child2->current_frame_host()->GetRenderWidgetHost()->ForwardKeyboardEvent(
      event);
  filter2->Wait();

  // At this point, we should have two pending widgets.
  EXPECT_TRUE(base::ContainsKey(
      web_contents()->pending_widget_views_,
      std::make_pair(process1->GetID(), filter1->routing_id())));
  EXPECT_TRUE(base::ContainsKey(
      web_contents()->pending_widget_views_,
      std::make_pair(process2->GetID(), filter2->routing_id())));

  // Both subframes were set up in the same way, so the next routing ID for the
  // new popup widgets should match up (this led to the collision in the
  // pending widgets map in the original bug).
  EXPECT_EQ(filter1->routing_id(), filter2->routing_id());

  // Now simulate both widgets being shown.
  web_contents()->ShowCreatedWidget(process1->GetID(), filter1->routing_id(),
                                    false, gfx::Rect());
  web_contents()->ShowCreatedWidget(process2->GetID(), filter2->routing_id(),
                                    false, gfx::Rect());
  EXPECT_FALSE(base::ContainsKey(
      web_contents()->pending_widget_views_,
      std::make_pair(process1->GetID(), filter1->routing_id())));
  EXPECT_FALSE(base::ContainsKey(
      web_contents()->pending_widget_views_,
      std::make_pair(process2->GetID(), filter2->routing_id())));
}
#endif

// Check that out-of-process frames correctly calculate their ability to enter
// fullscreen. A frame is allowed enter fullscreen if the allowFullscreen
// attribute is present in all of its ancestor <iframe> elements.  For OOPIF,
// when a parent frame changes this attribute, the change is replicated to the
// child frame and its proxies.
//
// The test checks the following cases:
//
// 1. Static attribute (<iframe allowfullscreen>)
// 2. Attribute injected dynamically via JavaScript
// 3. Multiple levels of nesting (A-embed-B-embed-C)
// 4. Cross-site subframe navigation
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, AllowFullscreen) {
  // Load a page with a cross-site <iframe allowFullscreen>.
  GURL url_1(embedded_test_server()->GetURL(
      "a.com", "/page_with_allowfullscreen_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url_1));

  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();

  // Helper to check if a frame is allowed to go fullscreen on the renderer
  // side.
  auto is_fullscreen_allowed = [](FrameTreeNode* ftn) {
    bool fullscreen_allowed = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        ftn,
        "window.domAutomationController.send(document.webkitFullscreenEnabled)",
        &fullscreen_allowed));
    return fullscreen_allowed;
  };

  EXPECT_TRUE(is_fullscreen_allowed(root));
  EXPECT_TRUE(is_fullscreen_allowed(root->child_at(0)));
  EXPECT_TRUE(root->child_at(0)->frame_owner_properties().allow_fullscreen);

  // Now navigate to a page with two <iframe>'s, both without allowFullscreen.
  GURL url_2(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,c)"));
  EXPECT_TRUE(NavigateToURL(shell(), url_2));
  EXPECT_FALSE(root->child_at(0)->frame_owner_properties().allow_fullscreen);
  EXPECT_FALSE(root->child_at(1)->frame_owner_properties().allow_fullscreen);

  EXPECT_TRUE(is_fullscreen_allowed(root));
  EXPECT_FALSE(is_fullscreen_allowed(root->child_at(0)));
  EXPECT_FALSE(is_fullscreen_allowed(root->child_at(1)));

  // Dynamically enable fullscreen for first subframe and check that the
  // fullscreen property was updated on the FrameTreeNode.
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-0').allowFullscreen='true'"));
  EXPECT_TRUE(root->child_at(0)->frame_owner_properties().allow_fullscreen);

  // Check that the first subframe is now allowed to go fullscreen.  Other
  // frames shouldn't be affected.
  EXPECT_TRUE(is_fullscreen_allowed(root));
  EXPECT_TRUE(is_fullscreen_allowed(root->child_at(0)));
  EXPECT_FALSE(is_fullscreen_allowed(root->child_at(1)));

  // Now navigate to a page with two levels of nesting.
  GURL url_3(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(c))"));
  EXPECT_TRUE(NavigateToURL(shell(), url_3));

  EXPECT_TRUE(is_fullscreen_allowed(root));
  EXPECT_FALSE(is_fullscreen_allowed(root->child_at(0)));
  EXPECT_FALSE(is_fullscreen_allowed(root->child_at(0)->child_at(0)));

  // Dynamically enable fullscreen for bottom subframe.
  EXPECT_TRUE(ExecuteScript(
      root->child_at(0),
      "document.getElementById('child-0').allowFullscreen='true'"));

  // This still shouldn't allow the bottom child to go fullscreen, since the
  // top frame hasn't allowed fullscreen for the middle frame.
  EXPECT_TRUE(is_fullscreen_allowed(root));
  EXPECT_FALSE(is_fullscreen_allowed(root->child_at(0)));
  EXPECT_FALSE(is_fullscreen_allowed(root->child_at(0)->child_at(0)));

  // Now allow fullscreen for the middle frame.
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-0').allowFullscreen='true'"));

  // All frames should be allowed to go fullscreen now.
  EXPECT_TRUE(is_fullscreen_allowed(root));
  EXPECT_TRUE(is_fullscreen_allowed(root->child_at(0)));
  EXPECT_TRUE(is_fullscreen_allowed(root->child_at(0)->child_at(0)));

  // Cross-site navigation should preserve the fullscreen flags.
  NavigateFrameToURL(root->child_at(0)->child_at(0),
                     embedded_test_server()->GetURL("d.com", "/title1.html"));
  EXPECT_TRUE(is_fullscreen_allowed(root->child_at(0)->child_at(0)));
}

// Test for https://crbug.com/615575. It ensures that file chooser triggered
// by a document in an out-of-process subframe works properly.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, FileChooserInSubframe) {
  EXPECT_TRUE(NavigateToURL(shell(), embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)")));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  GURL url(embedded_test_server()->GetURL("b.com", "/file_input.html"));
  NavigateFrameToURL(root->child_at(0), url);

  // Use FileChooserDelegate to avoid showing the actual dialog and to respond
  // back to the renderer process with predefined file.
  base::FilePath file;
  EXPECT_TRUE(PathService::Get(base::DIR_TEMP, &file));
  file = file.AppendASCII("bar");
  std::unique_ptr<FileChooserDelegate> delegate(new FileChooserDelegate(file));
  shell()->web_contents()->SetDelegate(delegate.get());
  EXPECT_TRUE(ExecuteScript(root->child_at(0),
                            "document.getElementById('fileinput').click();"));
  EXPECT_TRUE(delegate->file_chosen());

  // Also, extract the file from the renderer process to ensure that the
  // response made it over successfully and the proper filename is set.
  std::string file_name;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root->child_at(0),
      "window.domAutomationController.send("
      "document.getElementById('fileinput').files[0].name);",
      &file_name));
  EXPECT_EQ("bar", file_name);
}

// Tests that an out-of-process iframe receives the visibilitychange event.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, VisibilityChange) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  EXPECT_TRUE(ExecuteScript(
      root->child_at(0)->current_frame_host(),
      "var event_fired = 0;\n"
      "document.addEventListener('visibilitychange',\n"
      "                          function() { event_fired++; });\n"));

  shell()->web_contents()->WasHidden();

  int event_fired = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root->child_at(0)->current_frame_host(),
      "window.domAutomationController.send(event_fired);", &event_fired));
  EXPECT_EQ(1, event_fired);

  shell()->web_contents()->WasShown();

  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root->child_at(0)->current_frame_host(),
      "window.domAutomationController.send(event_fired);", &event_fired));
  EXPECT_EQ(2, event_fired);
}

#if defined(USE_AURA)
class SitePerProcessGestureBrowserTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessGestureBrowserTest() {}

  // This functions simulates a sequence of events that are typical of a
  // gesture pinch at |position|. We need this since machinery in the event
  // codepath will require GesturePinch* to be enclosed in
  // GestureScrollBegin/End, and since RenderWidgetHostInputEventRouter needs
  // both the preceding touch events, as well as GestureTapDown, in order to
  // correctly target the subsequent gesture event stream. The minimum stream
  // required to trigger the correct behaviours is represented here, but could
  // be expanded to include additional events such as one or more
  // GestureScrollUpdate and GesturePinchUpdate events.
  void SendPinchBeginEndSequence(RenderWidgetHostViewAura* rwhva,
                                 const gfx::Point& position) {
    DCHECK(rwhva);
    // Use full version of constructor with radius, angle and force since it
    // will crash in the renderer otherwise.
    ui::TouchEvent touch_pressed(
        ui::ET_TOUCH_PRESSED, position, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           /* pointer_id*/ 0,
                           /* radius_x */ 1.0f,
                           /* radius_y */ 1.0f,
                           /* force */ 1.0f));
    rwhva->OnTouchEvent(&touch_pressed);
    ui::TouchEvent touch_released(
        ui::ET_TOUCH_RELEASED, position, ui::EventTimeForNow(),
        ui::PointerDetails(ui::EventPointerType::POINTER_TYPE_TOUCH,
                           /* pointer_id*/ 0,
                           /* radius_x */ 1.0f,
                           /* radius_y */ 1.0f,
                           /* force */ 1.0f));
    rwhva->OnTouchEvent(&touch_released);

    ui::GestureEventDetails gesture_tap_down_details(ui::ET_GESTURE_TAP_DOWN);
    gesture_tap_down_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_tap_down(position.x(), position.y(), 0,
                                      ui::EventTimeForNow(),
                                      gesture_tap_down_details);
    rwhva->OnGestureEvent(&gesture_tap_down);

    ui::GestureEventDetails gesture_scroll_begin_details(
        ui::ET_GESTURE_SCROLL_BEGIN);
    gesture_scroll_begin_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_scroll_begin(position.x(), position.y(), 0,
                                          ui::EventTimeForNow(),
                                          gesture_scroll_begin_details);
    rwhva->OnGestureEvent(&gesture_scroll_begin);

    ui::GestureEventDetails gesture_pinch_begin_details(
        ui::ET_GESTURE_PINCH_BEGIN);
    gesture_pinch_begin_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_pinch_begin(position.x(), position.y(), 0,
                                         ui::EventTimeForNow(),
                                         gesture_pinch_begin_details);
    rwhva->OnGestureEvent(&gesture_pinch_begin);

    ui::GestureEventDetails gesture_pinch_end_details(ui::ET_GESTURE_PINCH_END);
    gesture_pinch_end_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_pinch_end(position.x(), position.y(), 0,
                                       ui::EventTimeForNow(),
                                       gesture_pinch_end_details);
    rwhva->OnGestureEvent(&gesture_pinch_end);

    ui::GestureEventDetails gesture_scroll_end_details(
        ui::ET_GESTURE_SCROLL_END);
    gesture_scroll_end_details.set_device_type(
        ui::GestureDeviceType::DEVICE_TOUCHSCREEN);
    ui::GestureEvent gesture_scroll_end(position.x(), position.y(), 0,
                                        ui::EventTimeForNow(),
                                        gesture_scroll_end_details);
    rwhva->OnGestureEvent(&gesture_scroll_end);
  }

  void SetupRootAndChild() {
    GURL main_url(embedded_test_server()->GetURL(
        "a.com", "/cross_site_iframe_factory.html?a(b)"));
    EXPECT_TRUE(NavigateToURL(shell(), main_url));

    FrameTreeNode* root_node =
        static_cast<WebContentsImpl*>(shell()->web_contents())
            ->GetFrameTree()
            ->root();
    FrameTreeNode* child_node = root_node->child_at(0);

    rwhv_child_ = static_cast<RenderWidgetHostViewBase*>(
        child_node->current_frame_host()->GetRenderWidgetHost()->GetView());

    rwhva_root_ = static_cast<RenderWidgetHostViewAura*>(
        shell()->web_contents()->GetRenderWidgetHostView());

    WaitForChildFrameSurfaceReady(child_node->current_frame_host());

    rwhi_child_ = child_node->current_frame_host()->GetRenderWidgetHost();
    rwhi_root_ = root_node->current_frame_host()->GetRenderWidgetHost();
  }

 protected:
  RenderWidgetHostViewBase* rwhv_child_;
  RenderWidgetHostViewAura* rwhva_root_;
  RenderWidgetHostImpl* rwhi_child_;
  RenderWidgetHostImpl* rwhi_root_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessGestureBrowserTest);
};

IN_PROC_BROWSER_TEST_F(SitePerProcessGestureBrowserTest,
                       SubframeGesturePinchGoesToMainFrame) {
  SetupRootAndChild();

  TestInputEventObserver root_frame_monitor(rwhi_root_);
  TestInputEventObserver child_frame_monitor(rwhi_child_);

  // Need child rect in main frame coords.
  gfx::Rect bounds = rwhv_child_->GetViewBounds();
  bounds.Offset(gfx::Point() - rwhva_root_->GetViewBounds().origin());
  SendPinchBeginEndSequence(rwhva_root_, bounds.CenterPoint());

  // Verify root-RWHI gets GSB/GPB/GPE/GSE.
  EXPECT_TRUE(root_frame_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin,
            root_frame_monitor.events_received()[0]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchBegin,
            root_frame_monitor.events_received()[1]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchEnd,
            root_frame_monitor.events_received()[2]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd,
            root_frame_monitor.events_received()[3]);

  // Verify child-RWHI gets TS/TE, GTD/GSB/GSE.
  EXPECT_TRUE(child_frame_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::kTouchStart,
            child_frame_monitor.events_received()[0]);
  EXPECT_EQ(blink::WebInputEvent::kTouchEnd,
            child_frame_monitor.events_received()[1]);
  EXPECT_EQ(blink::WebInputEvent::kGestureTapDown,
            child_frame_monitor.events_received()[2]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin,
            child_frame_monitor.events_received()[3]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd,
            child_frame_monitor.events_received()[4]);
}

IN_PROC_BROWSER_TEST_F(SitePerProcessGestureBrowserTest,
                       MainframeGesturePinchGoesToMainFrame) {
  SetupRootAndChild();

  TestInputEventObserver root_frame_monitor(rwhi_root_);
  TestInputEventObserver child_frame_monitor(rwhi_child_);

  // Need child rect in main frame coords.
  gfx::Rect bounds = rwhv_child_->GetViewBounds();
  bounds.Offset(gfx::Point() - rwhva_root_->GetViewBounds().origin());

  gfx::Point main_frame_point(bounds.origin());
  main_frame_point += gfx::Vector2d(-5, -5);
  SendPinchBeginEndSequence(rwhva_root_, main_frame_point);

  // Verify root-RWHI gets TS/TE/GTD/GSB/GPB/GPE/GSE.
  EXPECT_TRUE(root_frame_monitor.EventWasReceived());
  EXPECT_EQ(blink::WebInputEvent::kTouchStart,
            root_frame_monitor.events_received()[0]);
  EXPECT_EQ(blink::WebInputEvent::kTouchEnd,
            root_frame_monitor.events_received()[1]);
  EXPECT_EQ(blink::WebInputEvent::kGestureTapDown,
            root_frame_monitor.events_received()[2]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollBegin,
            root_frame_monitor.events_received()[3]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchBegin,
            root_frame_monitor.events_received()[4]);
  EXPECT_EQ(blink::WebInputEvent::kGesturePinchEnd,
            root_frame_monitor.events_received()[5]);
  EXPECT_EQ(blink::WebInputEvent::kGestureScrollEnd,
            root_frame_monitor.events_received()[6]);

  // Verify child-RWHI gets no events.
  EXPECT_FALSE(child_frame_monitor.EventWasReceived());
}
#endif

// Test that the pending RenderFrameHost is canceled and destroyed when its
// process dies. Previously, reusing a top-level pending RFH which
// is not live was hitting a CHECK in CreateRenderView due to having neither a
// main frame routing ID nor a proxy routing ID.  See https://crbug.com/627400
// for more details.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       PendingRFHIsCanceledWhenItsProcessDies) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a popup at b.com.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  Shell* popup_shell = OpenPopup(root, popup_url, "foo");
  EXPECT_TRUE(popup_shell);

  // The RenderViewHost for b.com in the main tab should not be active.
  SiteInstance* b_instance = popup_shell->web_contents()->GetSiteInstance();
  RenderViewHostImpl* rvh =
      web_contents()->GetFrameTree()->GetRenderViewHost(b_instance);
  EXPECT_FALSE(rvh->is_active());

  // Navigate main tab to a b.com URL that will not commit.
  GURL stall_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  TestNavigationManager delayer(shell()->web_contents(), stall_url);
  shell()->LoadURL(stall_url);
  EXPECT_TRUE(delayer.WaitForRequestStart());

  // The pending RFH should be in the same process as the popup.
  RenderFrameHostImpl* pending_rfh =
      IsBrowserSideNavigationEnabled()
          ? root->render_manager()->speculative_frame_host()
          : root->render_manager()->pending_frame_host();
  RenderProcessHost* pending_process = pending_rfh->GetProcess();
  EXPECT_EQ(pending_process,
            popup_shell->web_contents()->GetMainFrame()->GetProcess());

  // Kill the b.com process, currently in use by the pending RenderFrameHost
  // and the popup.
  RenderProcessHostWatcher crash_observer(
      pending_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(pending_process->Shutdown(0, false));
  crash_observer.Wait();

  // The pending RFH should have been canceled and destroyed, so that it won't
  // be reused while it's not live in the next navigation.
  {
    RenderFrameHostImpl* pending_rfh =
        IsBrowserSideNavigationEnabled()
            ? root->render_manager()->speculative_frame_host()
            : root->render_manager()->pending_frame_host();
    EXPECT_FALSE(pending_rfh);
  }

  // Navigate main tab to b.com again.  This should not crash.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURL(shell(), b_url));

  // The b.com RVH in the main tab should become active.
  EXPECT_TRUE(rvh->is_active());
}

// Test that killing a pending RenderFrameHost's process doesn't leave its
// RenderViewHost confused whether it's active or not for future navigations
// that try to reuse it.  See https://crbug.com/627893 for more details.
// Similar to the test above for https://crbug.com/627400, except the popup is
// navigated after pending RFH's process is killed, rather than the main tab.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       RenderViewHostKeepsSwappedOutStateIfPendingRFHDies) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Open a popup at b.com.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  Shell* popup_shell = OpenPopup(root, popup_url, "foo");
  EXPECT_TRUE(popup_shell);

  // The RenderViewHost for b.com in the main tab should not be active.
  SiteInstance* b_instance = popup_shell->web_contents()->GetSiteInstance();
  RenderViewHostImpl* rvh =
      web_contents()->GetFrameTree()->GetRenderViewHost(b_instance);
  EXPECT_FALSE(rvh->is_active());

  // Navigate main tab to a b.com URL that will not commit.
  GURL stall_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  NavigationHandleObserver handle_observer(shell()->web_contents(), stall_url);
  TestNavigationManager delayer(shell()->web_contents(), stall_url);
  shell()->LoadURL(stall_url);
  EXPECT_TRUE(delayer.WaitForRequestStart());

  // Kill the b.com process, currently in use by the pending RenderFrameHost
  // and the popup.
  RenderProcessHost* pending_process =
      popup_shell->web_contents()->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      pending_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  EXPECT_TRUE(pending_process->Shutdown(0, false));
  crash_observer.Wait();

  // Since the navigation above didn't commit, the b.com RenderViewHost in the
  // main tab should still not be active.
  EXPECT_FALSE(rvh->is_active());
  EXPECT_EQ(net::ERR_ABORTED, handle_observer.net_error_code());

  // Navigate popup to b.com to recreate the b.com process.  When creating
  // opener proxies, |rvh| should be reused as a swapped out RVH.  In
  // https://crbug.com/627893, recreating the opener RenderView was hitting a
  // CHECK(params.swapped_out) in the renderer process, since its
  // RenderViewHost was brought into an active state by the navigation to
  // |stall_url| above, even though it never committed.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title3.html"));
  EXPECT_TRUE(NavigateToURL(popup_shell, b_url));
  EXPECT_FALSE(rvh->is_active());
}

// Test that a crashed subframe can be successfully navigated to the site it
// was on before crashing.  See https://crbug.com/634368.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigateCrashedSubframeToSameSite) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Set up a postMessage handler in the main frame for later use.
  EXPECT_TRUE(ExecuteScript(
      root->current_frame_host(),
      "window.addEventListener('message',"
      "                        function(e) { document.title = e.data; });"));

  // Crash the subframe process.
  RenderProcessHost* child_process = child->current_frame_host()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  child_process->Shutdown(0, false);
  crash_observer.Wait();
  EXPECT_FALSE(child->current_frame_host()->IsRenderFrameLive());

  // When the subframe dies, its RenderWidgetHostView should be cleared and
  // reset in the CrossProcessFrameConnector.
  EXPECT_FALSE(child->current_frame_host()->GetView());
  RenderFrameProxyHost* proxy_to_parent =
      child->render_manager()->GetProxyToParent();
  EXPECT_FALSE(
      proxy_to_parent->cross_process_frame_connector()->get_view_for_testing());

  // Navigate the subframe to the same site it was on before crashing.  This
  // should reuse the subframe's current RenderFrameHost and reinitialize the
  // RenderFrame in a new process.
  NavigateFrameToURL(child,
                     embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_TRUE(child->current_frame_host()->IsRenderFrameLive());

  // The RenderWidgetHostView for the child should be recreated and set to be
  // used in the CrossProcessFrameConnector.  Without this, the frame won't be
  // rendered properly.
  EXPECT_TRUE(child->current_frame_host()->GetView());
  EXPECT_EQ(
      child->current_frame_host()->GetView(),
      proxy_to_parent->cross_process_frame_connector()->get_view_for_testing());

  // Make sure that the child frame has submitted a compositor frame.
  WaitForChildFrameSurfaceReady(child->current_frame_host());

  // Send a postMessage from the child to its parent.  This verifies that the
  // parent's proxy in the child's SiteInstance was also restored.
  base::string16 expected_title(base::UTF8ToUTF16("I am alive!"));
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_TRUE(ExecuteScript(child->current_frame_host(),
                            "parent.postMessage('I am alive!', '*');"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Test that session history length and offset are replicated to all renderer
// processes in a FrameTree.  This allows each renderer to see correct values
// for history.length, and to check the offset validity properly for
// navigations initiated via history.go(). See https:/crbug.com/501116.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, SessionHistoryReplication) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a,a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child1 = root->child_at(0);
  FrameTreeNode* child2 = root->child_at(1);
  GURL child_first_url(child1->current_url());
  EXPECT_EQ(child1->current_url(), child2->current_url());

  // Helper to retrieve the history length from a given frame.
  auto history_length = [](FrameTreeNode* ftn) {
    int history_length = -1;
    EXPECT_TRUE(ExecuteScriptAndExtractInt(
        ftn->current_frame_host(),
        "window.domAutomationController.send(history.length);",
        &history_length));
    return history_length;
  };

  // All frames should see a history length of 1 to start with.
  EXPECT_EQ(1, history_length(root));
  EXPECT_EQ(1, history_length(child1));
  EXPECT_EQ(1, history_length(child2));

  // Navigate first child cross-site.  This increases history length to 2.
  NavigateFrameToURL(child1,
                     embedded_test_server()->GetURL("b.com", "/title1.html"));
  EXPECT_EQ(2, history_length(root));
  EXPECT_EQ(2, history_length(child1));
  EXPECT_EQ(2, history_length(child2));

  // Navigate second child same-site.
  GURL child2_last_url(embedded_test_server()->GetURL("a.com", "/title2.html"));
  NavigateFrameToURL(child2, child2_last_url);
  EXPECT_EQ(3, history_length(root));
  EXPECT_EQ(3, history_length(child1));
  EXPECT_EQ(3, history_length(child2));

  // Navigate first child same-site to another b.com URL.
  GURL child1_last_url(embedded_test_server()->GetURL("b.com", "/title3.html"));
  NavigateFrameToURL(child1, child1_last_url);
  EXPECT_EQ(4, history_length(root));
  EXPECT_EQ(4, history_length(child1));
  EXPECT_EQ(4, history_length(child2));

  // Go back three entries using the history API from the main frame. This
  // checks that both history length and offset are not stale in a.com, as
  // otherwise this navigation might be dropped by Blink.
  EXPECT_TRUE(ExecuteScript(root, "history.go(-3);"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(child_first_url, child1->current_url());
  EXPECT_EQ(child_first_url, child2->current_url());

  // Now go forward three entries from the child1 frame and check that the
  // history length and offset are not stale in b.com.
  EXPECT_TRUE(ExecuteScript(child1, "history.go(3);"));
  EXPECT_TRUE(WaitForLoadStop(shell()->web_contents()));
  EXPECT_EQ(main_url, root->current_url());
  EXPECT_EQ(child1_last_url, child1->current_url());
  EXPECT_EQ(child2_last_url, child2->current_url());
}

// A BrowserMessageFilter that drops FrameHostMsg_OnDispatchLoad messages.
class DispatchLoadMessageFilter : public BrowserMessageFilter {
 public:
  DispatchLoadMessageFilter() : BrowserMessageFilter(FrameMsgStart) {}

 protected:
  ~DispatchLoadMessageFilter() override {}

 private:
  // BrowserMessageFilter:
  bool OnMessageReceived(const IPC::Message& message) override {
    return message.type() == FrameHostMsg_DispatchLoad::ID;
  }

  DISALLOW_COPY_AND_ASSIGN(DispatchLoadMessageFilter);
};

// Test that the renderer isn't killed when a frame generates a load event just
// after becoming pending deletion.  See https://crbug.com/636513.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       LoadEventForwardingWhilePendingDeletion) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Open a popup in the b.com process for later use.
  GURL popup_url(embedded_test_server()->GetURL("b.com", "/title1.html"));
  Shell* popup_shell = OpenPopup(root, popup_url, "foo");
  EXPECT_TRUE(popup_shell);

  // Install a filter to drop DispatchLoad messages from b.com.
  scoped_refptr<DispatchLoadMessageFilter> filter =
      new DispatchLoadMessageFilter();
  RenderProcessHost* b_process =
      popup_shell->web_contents()->GetMainFrame()->GetProcess();
  b_process->AddFilter(filter.get());

  // Navigate subframe to b.com.  Wait for commit but not full load.
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  {
    TestFrameNavigationObserver commit_observer(child);
    EXPECT_TRUE(
        ExecuteScript(child, "location.href = '" + b_url.spec() + "';"));
    commit_observer.WaitForCommit();
  }
  RenderFrameHostImpl* child_rfh = child->current_frame_host();
  child_rfh->DisableSwapOutTimerForTesting();

  // At this point, the subframe should have a proxy in its parent's
  // SiteInstance, a.com.
  EXPECT_TRUE(child->render_manager()->GetProxyToParent());

  // Now, go back to a.com in the subframe and wait for commit.
  {
    TestFrameNavigationObserver commit_observer(child);
    web_contents()->GetController().GoBack();
    commit_observer.WaitForCommit();
  }

  // At this point, the subframe's old RFH for b.com should be pending
  // deletion, and the subframe's proxy in a.com should've been cleared.
  EXPECT_FALSE(child_rfh->is_active());
  EXPECT_FALSE(child->render_manager()->GetProxyToParent());

  // Simulate that the load event is dispatched from |child_rfh| just after
  // it's become pending deletion.
  child_rfh->OnDispatchLoad();

  // In the bug, OnDispatchLoad killed the b.com renderer.  Ensure that this is
  // not the case. Note that the process kill doesn't happen immediately, so
  // IsRenderFrameLive() can't be checked here (yet).  Instead, check that
  // JavaScript can still execute in b.com using the popup.
  EXPECT_TRUE(ExecuteScript(popup_shell->web_contents(), "true"));
}

// Tests that trying to navigate in the unload handler doesn't crash the
// browser.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, NavigateInUnloadHandler) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b(b))"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  EXPECT_EQ(
      " Site A ------------ proxies for B\n"
      "   +--Site B ------- proxies for A\n"
      "        +--Site B -- proxies for A\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/",
      DepictFrameTree(root));

  int child_count = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root->child_at(0)->current_frame_host(),
      "window.domAutomationController.send(frames.length);", &child_count));
  EXPECT_EQ(1, child_count);

  // Add an unload handler to B's subframe.
  EXPECT_TRUE(
      ExecuteScript(root->child_at(0)->child_at(0)->current_frame_host(),
                    "window.onunload=function(e){\n"
                    "    window.location = '#navigate';\n"
                    "};\n"));

  // Navigate B's subframe to a cross-site C.
  RenderFrameDeletedObserver deleted_observer(
      root->child_at(0)->child_at(0)->current_frame_host());
  std::string script =
      std::string("window.document.getElementById('child-0').src = \"") +
      embedded_test_server()
          ->GetURL("c.com", "/cross_site_iframe_factory.html")
          .spec() +
      "\"";
  EXPECT_TRUE(
      ExecuteScript(root->child_at(0)->current_frame_host(), script.c_str()));

  // Wait until B's subframe RenderFrameHost is destroyed.
  deleted_observer.WaitUntilDeleted();

  // Check that C's subframe is alive and the navigation in the unload handler
  // was ignored.
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root->child_at(0)->child_at(0)->current_frame_host(),
      "window.domAutomationController.send(frames.length);", &child_count));
  EXPECT_EQ(0, child_count);

  EXPECT_EQ(
      " Site A ------------ proxies for B C\n"
      "   +--Site B ------- proxies for A C\n"
      "        +--Site C -- proxies for A B\n"
      "Where A = http://a.com/\n"
      "      B = http://b.com/\n"
      "      C = http://c.com/",
      DepictFrameTree(root));
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       RFHTransfersWhilePendingDeletion) {
  GURL main_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // Start a cross-process navigation and wait until the response is received.
  GURL cross_site_url_1 =
      embedded_test_server()->GetURL("b.com", "/title1.html");
  TestNavigationManager cross_site_manager(shell()->web_contents(),
                                           cross_site_url_1);
  shell()->web_contents()->GetController().LoadURL(
      cross_site_url_1, Referrer(), ui::PAGE_TRANSITION_LINK, std::string());
  EXPECT_TRUE(cross_site_manager.WaitForResponse());

  // Start a renderer-initiated navigation to a cross-process url and make sure
  // the navigation will be blocked before being transferred.
  GURL cross_site_url_2 =
      embedded_test_server()->GetURL("c.com", "/title1.html");
  TestNavigationManager transfer_manager(shell()->web_contents(),
                                         cross_site_url_2);
  EXPECT_TRUE(ExecuteScript(
      root, "location.href = '" + cross_site_url_2.spec() + "';"));
  EXPECT_TRUE(transfer_manager.WaitForResponse());

  // Now have the cross-process navigation commit and mark the current RFH as
  // pending deletion.
  cross_site_manager.WaitForNavigationFinished();

  // Resume the navigation in the previous RFH that has just been marked as
  // pending deletion. We should not crash.
  transfer_manager.WaitForNavigationFinished();
}

class NavigationHandleWatcher : public WebContentsObserver {
 public:
  NavigationHandleWatcher(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    DCHECK_EQ(GURL("http://b.com/"),
              navigation_handle->GetStartingSiteInstance()->GetSiteURL());
  }
};

// Verifies that the SiteInstance of a NavigationHandle correctly identifies the
// RenderFrameHost that started the navigation (and not the destination RFH).
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       NavigationHandleSiteInstance) {
  // Navigate to a page with a cross-site iframe.
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Navigate the iframe cross-site.
  NavigationHandleWatcher watcher(shell()->web_contents());
  TestNavigationObserver load_observer(shell()->web_contents());
  GURL frame_url = embedded_test_server()->GetURL("c.com", "/title1.html");
  EXPECT_TRUE(ExecuteScript(
      shell()->web_contents(),
      "window.frames[0].location = \"" + frame_url.spec() + "\";"));
  load_observer.Wait();
}

// Test that when canceling a pending RenderFrameHost in the middle of a
// redirect, and then killing the corresponding RenderView's renderer process,
// the RenderViewHost isn't reused in an improper state later.  Previously this
// led to a crash in CreateRenderView when recreating the RenderView due to a
// stale main frame routing ID.  See https://crbug.com/627400.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       ReuseNonLiveRenderViewHostAfterCancelPending) {
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title3.html"));

  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  // Open a popup and navigate it to b.com.
  Shell* popup = OpenPopup(shell(), a_url, "popup");
  EXPECT_TRUE(NavigateToURL(popup, b_url));

  // Open a second popup and navigate it to b.com, which redirects to c.com.
  // The navigation to b.com will create a pending RenderFrameHost, which will
  // be canceled during the redirect to c.com.  Note that NavigateToURL will
  // return false because the committed URL won't match the requested URL due
  // to the redirect.
  Shell* popup2 = OpenPopup(shell(), a_url, "popup2");
  TestNavigationObserver observer(popup2->web_contents());
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/server-redirect?" + c_url.spec()));
  EXPECT_FALSE(NavigateToURL(popup2, redirect_url));
  EXPECT_EQ(c_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Kill the b.com process (which currently hosts a RenderFrameProxy that
  // replaced the pending RenderFrame in |popup2|, as well as the RenderFrame
  // for |popup|).
  RenderProcessHost* b_process =
      popup->web_contents()->GetMainFrame()->GetProcess();
  RenderProcessHostWatcher crash_observer(
      b_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
  b_process->Shutdown(0, false);
  crash_observer.Wait();

  // Navigate the second popup to b.com.  This used to crash when creating the
  // RenderView, because it reused the RenderViewHost created by the canceled
  // navigation to b.com, and that RenderViewHost had a stale main frame
  // routing ID and active state.
  EXPECT_TRUE(NavigateToURL(popup2, b_url));
}

// Check that after a pending RFH is canceled and replaced with a proxy (which
// reuses the canceled RFH's RenderViewHost), navigating to a main frame in the
// same site as the canceled RFH doesn't lead to a renderer crash.  The steps
// here are similar to ReuseNonLiveRenderViewHostAfterCancelPending, but don't
// involve crashing the renderer. See https://crbug.com/651980.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       RecreateMainFrameAfterCancelPending) {
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title3.html"));

  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  // Open a popup and navigate it to b.com.
  Shell* popup = OpenPopup(shell(), a_url, "popup");
  EXPECT_TRUE(NavigateToURL(popup, b_url));

  // Open a second popup and navigate it to b.com, which redirects to c.com.
  // The navigation to b.com will create a pending RenderFrameHost, which will
  // be canceled during the redirect to c.com.  Note that NavigateToURL will
  // return false because the committed URL won't match the requested URL due
  // to the redirect.
  Shell* popup2 = OpenPopup(shell(), a_url, "popup2");
  TestNavigationObserver observer(popup2->web_contents());
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/server-redirect?" + c_url.spec()));
  EXPECT_FALSE(NavigateToURL(popup2, redirect_url));
  EXPECT_EQ(c_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Navigate the second popup to b.com.  This used to crash the b.com renderer
  // because it failed to delete the canceled RFH's RenderFrame, so this caused
  // it to try to create a frame widget which already existed.
  EXPECT_TRUE(NavigateToURL(popup2, b_url));
}

// Check that when a pending RFH is canceled and a proxy needs to be created in
// its place, the proxy is properly initialized on the renderer side.  See
// https://crbug.com/653746.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CommunicateWithProxyAfterCancelPending) {
  GURL a_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  GURL b_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  GURL c_url(embedded_test_server()->GetURL("c.com", "/title3.html"));

  EXPECT_TRUE(NavigateToURL(shell(), a_url));

  // Open a popup and navigate it to b.com.
  Shell* popup = OpenPopup(shell(), a_url, "popup");
  EXPECT_TRUE(NavigateToURL(popup, b_url));

  // Open a second popup and navigate it to b.com, which redirects to c.com.
  // The navigation to b.com will create a pending RenderFrameHost, which will
  // be canceled during the redirect to c.com.  Note that NavigateToURL will
  // return false because the committed URL won't match the requested URL due
  // to the redirect.
  Shell* popup2 = OpenPopup(shell(), a_url, "popup2");
  TestNavigationObserver observer(popup2->web_contents());
  GURL redirect_url(embedded_test_server()->GetURL(
      "b.com", "/server-redirect?" + c_url.spec()));
  EXPECT_FALSE(NavigateToURL(popup2, redirect_url));
  EXPECT_EQ(c_url, observer.last_navigation_url());
  EXPECT_TRUE(observer.last_navigation_succeeded());

  // Because b.com has other active frames (namely, the frame in |popup|),
  // there should be a proxy created for the canceled RFH, and it should be
  // live.
  SiteInstance* b_instance = popup->web_contents()->GetSiteInstance();
  FrameTreeNode* popup2_root =
      static_cast<WebContentsImpl*>(popup2->web_contents())
          ->GetFrameTree()
          ->root();
  RenderFrameProxyHost* proxy =
      popup2_root->render_manager()->GetRenderFrameProxyHost(b_instance);
  EXPECT_TRUE(proxy);
  EXPECT_TRUE(proxy->is_render_frame_proxy_live());

  // Add a postMessage listener in |popup2| (currently at a c.com URL).
  EXPECT_TRUE(
      ExecuteScript(popup2,
                    "window.addEventListener('message', function(event) {\n"
                    "  document.title=event.data;\n"
                    "});"));

  // Check that a postMessage can be sent via |proxy| above.  This needs to be
  // done from the b.com process.  |popup| is currently in b.com, but it can't
  // reach the window reference for |popup2| due to a security restriction in
  // Blink. So, navigate the main tab to b.com and then send a postMessage to
  // |popup2|. This is allowed since the main tab is |popup2|'s opener.
  EXPECT_TRUE(NavigateToURL(shell(), b_url));

  base::string16 expected_title(base::UTF8ToUTF16("foo"));
  TitleWatcher title_watcher(popup2->web_contents(), expected_title);
  EXPECT_TRUE(ExecuteScript(
      shell(), "window.open('','popup2').postMessage('foo', '*');"));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

// Checks that everything is cleaned up even when the frame tree is destroyed
// during a transfer.  See also https://crbug.com/657195.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       FrameTreeDestroyedInMiddleOfTransfer) {
  // Transfer navigations don't occur with PlzNavigate.
  if (IsBrowserSideNavigationEnabled())
    return;
  GURL page_url(embedded_test_server()->GetURL(
      "main.com", "/frame_tree/page_with_one_frame.html"));
  GURL initial_frame_url(embedded_test_server()->GetURL(
      "main.com", "/cross-site/baz.com/title1.html"));

  {
    // Navigation below is needed to make OpenPopup (next statement) work.
    EXPECT_TRUE(
        NavigateToURL(shell(), embedded_test_server()->GetURL("/title2.html")));

    // Create a separate Shell + WebContents - these will be destroyed during
    // the test at a very special moment.
    Shell* other_shell = OpenPopup(shell()->web_contents(), GURL(), "popup");

    // Load the test page, while monitoring navigations of the frame (to catch
    // when the frame navigation will initiate a transfer to another renderer).
    TestNavigationManager navigation_manager(other_shell->web_contents(),
                                             initial_frame_url);
    other_shell->LoadURL(page_url);

    // Wait until |navigation_manager| detects a WillProcessResponse associated
    // with the frame navigation.
    ASSERT_TRUE(navigation_manager.WaitForResponse());

    // At this point we have almost (but not quite) triggered a transfer
    // request. The transfer will be initiated when resuming the navigation.
    // Posting a task to destroy the frame being navigated means that the
    // destruction won't happen now, but will happen right after initiating the
    // transfer AND before the transfer completes. i.e. This task will be
    // executed on the message queue before the task to process the
    // DidStartProvisionalLoad IPC from the renderer.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&WebContents::Close,
                              base::Unretained(other_shell->web_contents())));

    // Resume the navigation. This will 1) initiate the transfer and 2) shortly
    // after destroy the |other_shell| via WebContents::Close task posted above.
    // Destroying the NavigationHandle at this special moment used to trigger
    // https://crbug.com/657195.
    navigation_manager.WaitForNavigationFinished();
  }

  // Start a URLRequest to the same URL. This should succeed. This would have
  // hit the 20 seconds delay before https://crbug.com/657195 was fixed.
  EXPECT_TRUE(NavigateToURL(shell(), page_url));
  EXPECT_EQ(page_url, shell()->web_contents()->GetLastCommittedURL());

  // Note: even if the test fails and for some reason, the test has not timed
  // out by this point, the test teardown code will still hit a DCHECK when it
  // calls AssertNoURLRequests() in the shell's URLRequestContext destructor.
}

IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       TestFeaturePolicyReplicationOnSameOriginNavigation) {
  GURL start_url(
      embedded_test_server()->GetURL("a.com", "/feature-policy1.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("a.com", "/feature-policy2.html"));
  GURL second_nav_url(embedded_test_server()->GetURL("a.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(CreateFPHeader(blink::WebFeaturePolicyFeature::kVibrate,
                           {start_url.GetOrigin()}),
            root->current_replication_state().feature_policy_header);

  // When the main frame navigates to a page with a new policy, it should
  // overwrite the old one.
  EXPECT_TRUE(NavigateToURL(shell(), first_nav_url));
  EXPECT_EQ(CreateFPHeaderMatchesAll(blink::WebFeaturePolicyFeature::kVibrate),
            root->current_replication_state().feature_policy_header);

  // When the main frame navigates to a page without a policy, the replicated
  // policy header should be cleared.
  EXPECT_TRUE(NavigateToURL(shell(), second_nav_url));
  EXPECT_TRUE(root->current_replication_state().feature_policy_header.empty());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       TestFeaturePolicyReplicationOnCrossOriginNavigation) {
  GURL start_url(
      embedded_test_server()->GetURL("a.com", "/feature-policy1.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("b.com", "/feature-policy2.html"));
  GURL second_nav_url(embedded_test_server()->GetURL("c.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(CreateFPHeader(blink::WebFeaturePolicyFeature::kVibrate,
                           {start_url.GetOrigin()}),
            root->current_replication_state().feature_policy_header);

  // When the main frame navigates to a page with a new policy, it should
  // overwrite the old one.
  EXPECT_TRUE(NavigateToURL(shell(), first_nav_url));
  EXPECT_EQ(CreateFPHeaderMatchesAll(blink::WebFeaturePolicyFeature::kVibrate),
            root->current_replication_state().feature_policy_header);

  // When the main frame navigates to a page without a policy, the replicated
  // policy header should be cleared.
  EXPECT_TRUE(NavigateToURL(shell(), second_nav_url));
  EXPECT_TRUE(root->current_replication_state().feature_policy_header.empty());
}

// Test that the replicated feature policy header is correct in subframes as
// they navigate.
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       TestFeaturePolicyReplicationFromRemoteFrames) {
  GURL main_url(
      embedded_test_server()->GetURL("a.com", "/feature-policy-main.html"));
  GURL first_nav_url(
      embedded_test_server()->GetURL("b.com", "/feature-policy2.html"));
  GURL second_nav_url(embedded_test_server()->GetURL("c.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  EXPECT_EQ(CreateFPHeader(blink::WebFeaturePolicyFeature::kVibrate,
                           {main_url.GetOrigin(), GURL("http://example.com/")}),
            root->current_replication_state().feature_policy_header);
  EXPECT_EQ(1UL, root->child_count());
  EXPECT_EQ(
      CreateFPHeader(blink::WebFeaturePolicyFeature::kVibrate,
                     {main_url.GetOrigin()}),
      root->child_at(0)->current_replication_state().feature_policy_header);

  // Navigate the iframe cross-site.
  NavigateFrameToURL(root->child_at(0), first_nav_url);
  EXPECT_EQ(
      CreateFPHeaderMatchesAll(blink::WebFeaturePolicyFeature::kVibrate),
      root->child_at(0)->current_replication_state().feature_policy_header);

  // Navigate the iframe to another location, this one with no policy header
  NavigateFrameToURL(root->child_at(0), second_nav_url);
  EXPECT_TRUE(root->child_at(0)
                  ->current_replication_state()
                  .feature_policy_header.empty());

  // Navigate the iframe back to a page with a policy
  NavigateFrameToURL(root->child_at(0), first_nav_url);
  EXPECT_EQ(
      CreateFPHeaderMatchesAll(blink::WebFeaturePolicyFeature::kVibrate),
      root->child_at(0)->current_replication_state().feature_policy_header);
}

// Ensure that an iframe that navigates cross-site doesn't use the same process
// as its parent. Then when its parent navigates it via the "srcdoc" attribute,
// it must reuse its parent's process.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       IframeSrcdocAfterCrossSiteNavigation) {
  GURL parent_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  GURL child_url(embedded_test_server()->GetURL(
      "b.com", "/cross_site_iframe_factory.html?b()"));
  GURL srcdoc_url(kAboutSrcDocURL);

  // #1 Navigate to a page with a cross-site iframe.
  EXPECT_TRUE(NavigateToURL(shell(), parent_url));

  // Ensure that the iframe uses its own process.
  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  ASSERT_EQ(1u, root->child_count());
  FrameTreeNode* child = root->child_at(0);
  EXPECT_EQ(parent_url, root->current_url());
  EXPECT_EQ(child_url, child->current_url());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_NE(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());

  // #2 Navigate the iframe to its srcdoc attribute.
  TestNavigationObserver load_observer(shell()->web_contents());
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-0').srcdoc = 'srcdoc content';"));
  load_observer.Wait();

  // Ensure that the iframe reuses its parent's process.
  EXPECT_EQ(srcdoc_url, child->current_url());
  EXPECT_EQ(root->current_frame_host()->GetSiteInstance(),
            child->current_frame_host()->GetSiteInstance());
  EXPECT_EQ(root->current_frame_host()->GetProcess(),
            child->current_frame_host()->GetProcess());
}

// Test that MouseDown and MouseUp to the same coordinates do not result in
// different coordinates after routing. See bug https://crbug.com/670253.
#if defined(OS_ANDROID)
// Android uses fixed scale factor, which makes this test unnecessary.
#define MAYBE_MouseClickWithNonIntegerScaleFactor \
  DISABLED_MouseClickWithNonIntegerScaleFactor
#else
#define MAYBE_MouseClickWithNonIntegerScaleFactor \
  MouseClickWithNonIntegerScaleFactor
#endif
IN_PROC_BROWSER_TEST_F(SitePerProcessNonIntegerScaleFactorBrowserTest,
                       MAYBE_MouseClickWithNonIntegerScaleFactor) {
  GURL initial_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), initial_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  RenderWidgetHostViewBase* rwhv = static_cast<RenderWidgetHostViewBase*>(
      root->current_frame_host()->GetRenderWidgetHost()->GetView());

  RenderWidgetHostInputEventRouter* router =
      static_cast<WebContentsImpl*>(shell()->web_contents())
          ->GetInputEventRouter();

  // Create listener for input events.
  RenderWidgetHostMouseEventMonitor event_monitor(
      root->current_frame_host()->GetRenderWidgetHost());

  blink::WebMouseEvent mouse_event(blink::WebInputEvent::kMouseDown,
                                   blink::WebInputEvent::kNoModifiers,
                                   blink::WebInputEvent::kTimeStampForTesting);
  mouse_event.button = blink::WebPointerProperties::Button::kLeft;
  mouse_event.SetPositionInWidget(75, 75);
  mouse_event.click_count = 1;
  event_monitor.ResetEventReceived();
  router->RouteMouseEvent(rwhv, &mouse_event, ui::LatencyInfo());

  EXPECT_TRUE(event_monitor.EventWasReceived());
  gfx::Point mouse_down_coords =
      gfx::Point(event_monitor.event().PositionInWidget().x,
                 event_monitor.event().PositionInWidget().y);
  event_monitor.ResetEventReceived();

  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  mouse_event.SetPositionInWidget(75, 75);
  router->RouteMouseEvent(rwhv, &mouse_event, ui::LatencyInfo());

  EXPECT_TRUE(event_monitor.EventWasReceived());
  EXPECT_EQ(mouse_down_coords,
            gfx::Point(event_monitor.event().PositionInWidget().x,
                       event_monitor.event().PositionInWidget().y));
}

// Verify that a remote-to-local navigation in a crashed subframe works.  See
// https://crbug.com/487872.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       RemoteToLocalNavigationInCrashedSubframe) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();
  FrameTreeNode* child = root->child_at(0);

  // Crash the subframe process.
  RenderProcessHost* child_process = child->current_frame_host()->GetProcess();
  {
    RenderProcessHostWatcher crash_observer(
        child_process, RenderProcessHostWatcher::WATCH_FOR_PROCESS_EXIT);
    child_process->Shutdown(0, false);
    crash_observer.Wait();
  }
  EXPECT_FALSE(child->current_frame_host()->IsRenderFrameLive());

  // Do a remote-to-local navigation of the child frame from the parent frame.
  TestFrameNavigationObserver frame_observer(child);
  GURL frame_url(embedded_test_server()->GetURL("a.com", "/title1.html"));
  EXPECT_TRUE(ExecuteScript(root, "document.querySelector('iframe').src = '" +
                                      frame_url.spec() + "';\n"));
  frame_observer.Wait();

  EXPECT_TRUE(child->current_frame_host()->IsRenderFrameLive());
  EXPECT_FALSE(child->IsLoading());
  EXPECT_EQ(child->current_frame_host()->GetSiteInstance(),
            root->current_frame_host()->GetSiteInstance());

  // Ensure the subframe is correctly attached in the frame tree, and that it
  // has correct content.
  int child_count = 0;
  EXPECT_TRUE(ExecuteScriptAndExtractInt(
      root, "window.domAutomationController.send(frames.length);",
      &child_count));
  EXPECT_EQ(1, child_count);

  std::string result;
  EXPECT_TRUE(ExecuteScriptAndExtractString(
      root,
      "window.domAutomationController.send(frames[0].document.body.innerText);",
      &result));
  EXPECT_EQ("This page has no title.", result);
}

// Tests that trying to open a context menu in the old RFH after commiting a
// navigation doesn't crash the browser. https://crbug.com/677266.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       ContextMenuAfterCrossProcessNavigation) {
  // Navigate to a.com.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("a.com", "/title1.html")));

  // Disable the swapout ACK and the swapout timer.
  RenderFrameHostImpl* rfh = static_cast<RenderFrameHostImpl*>(
      shell()->web_contents()->GetMainFrame());
  scoped_refptr<SwapoutACKMessageFilter> filter = new SwapoutACKMessageFilter();
  rfh->GetProcess()->AddFilter(filter.get());
  rfh->DisableSwapOutTimerForTesting();

  // Open a popup on a.com to keep the process alive.
  OpenPopup(shell(), embedded_test_server()->GetURL("a.com", "/title2.html"),
            "foo");

  // Cross-process navigation to b.com.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL("b.com", "/title3.html")));

  // Pretend that a.com just requested a context menu. This used to cause a
  // because the RenderWidgetHostView is destroyed when the frame is swapped and
  // added to pending delete list.
  rfh->OnMessageReceived(
      FrameHostMsg_ContextMenu(rfh->GetRoutingID(), ContextMenuParams()));
}

// Test iframe "allow" attribute is propagated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest, AllowedIFrames) {
  GURL url(embedded_test_server()->GetURL("/allowed_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Iframes without "allow" attribute, or an empty "allow" attribute will not
  // have any allowed features propagated.
  EXPECT_TRUE(
      root->child_at(0)->frame_owner_properties().allowed_features.empty());
  EXPECT_TRUE(
      root->child_at(1)->frame_owner_properties().allowed_features.empty());

  // Make sure the third frame starts out at the correct cross-site page.
  EXPECT_EQ(embedded_test_server()->GetURL("bar.com", "/title1.html"),
            root->child_at(2)->current_url());
  // Check allowed features are propagated correctly for cross-site iframes.
  EXPECT_EQ(root->child_at(2)->frame_owner_properties().allowed_features.size(),
            2u);
  EXPECT_EQ(root->child_at(2)->frame_owner_properties().allowed_features[0],
            blink::WebFeaturePolicyFeature::kFullscreen);
  EXPECT_EQ(root->child_at(2)->frame_owner_properties().allowed_features[1],
            blink::WebFeaturePolicyFeature::kVibrate);

  // Check allowed features are propagated correctly for same-site iframes.
  EXPECT_EQ(root->child_at(3)->frame_owner_properties().allowed_features.size(),
            2u);
  EXPECT_EQ(root->child_at(3)->frame_owner_properties().allowed_features[0],
            blink::WebFeaturePolicyFeature::kFullscreen);
  EXPECT_EQ(root->child_at(3)->frame_owner_properties().allowed_features[1],
            blink::WebFeaturePolicyFeature::kVibrate);
}

// Test dynamic updates to iframe "allow" attribute are propagated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       AllowedIFramesDynamic) {
  GURL main_url(embedded_test_server()->GetURL("/allowed_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Test for dynamically removing "allow" attribute.
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-2').removeAttribute('allow')"));
  EXPECT_TRUE(
      root->child_at(2)->frame_owner_properties().allowed_features.empty());

  // Test for dynamically setting "allow" attribute by element.setAttribute.
  EXPECT_TRUE(ExecuteScript(
      root,
      "document.getElementById('child-3').setAttribute('allow', 'payment')"));
  EXPECT_EQ(root->child_at(3)->frame_owner_properties().allowed_features.size(),
            1u);
  EXPECT_EQ(root->child_at(3)->frame_owner_properties().allowed_features[0],
            blink::WebFeaturePolicyFeature::kPayment);

  // Test for dynamically setting "allow" attribute by frame.allow="...".
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-3').allow='fullscreen vibrate'"));
  EXPECT_EQ(root->child_at(3)->frame_owner_properties().allowed_features.size(),
            2u);
  EXPECT_EQ(root->child_at(3)->frame_owner_properties().allowed_features[0],
            blink::WebFeaturePolicyFeature::kFullscreen);
  EXPECT_EQ(root->child_at(3)->frame_owner_properties().allowed_features[1],
            blink::WebFeaturePolicyFeature::kVibrate);
}

// Test iframe container policy is replicated properly to the browser.
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       ContainerPolicy) {
  GURL url(embedded_test_server()->GetURL("/allowed_frames.html"));
  EXPECT_TRUE(NavigateToURL(shell(), url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(0UL, root->effective_container_policy().size());
  EXPECT_EQ(0UL, root->child_at(0)->effective_container_policy().size());
  EXPECT_EQ(0UL, root->child_at(1)->effective_container_policy().size());
  EXPECT_EQ(2UL, root->child_at(2)->effective_container_policy().size());
  EXPECT_EQ(2UL, root->child_at(3)->effective_container_policy().size());
}

// Test dynamic updates to iframe "allow" attribute are propagated correctly.
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       ContainerPolicyDynamic) {
  GURL main_url(embedded_test_server()->GetURL("/allowed_frames.html"));
  GURL nav_url(
      embedded_test_server()->GetURL("b.com", "/feature-policy2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  EXPECT_EQ(2UL, root->child_at(2)->effective_container_policy().size());

  // Removing the "allow" attribute; pending policy should update, but effective
  // policy remains unchanged.
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-2').setAttribute('allow','')"));
  EXPECT_EQ(2UL, root->child_at(2)->effective_container_policy().size());
  EXPECT_EQ(0UL, root->child_at(2)->pending_container_policy_.size());

  // Navigate the frame; pending policy should be committed.
  NavigateFrameToURL(root->child_at(2), nav_url);
  EXPECT_EQ(0UL, root->child_at(2)->effective_container_policy().size());
}

// Check that out-of-process frames correctly calculate the container policy in
// the renderer when navigating cross-origin. The policy should be unchanged
// when modified dynamically in the parent frame. When the frame is navigated,
// the new renderer should have the correct container policy.
//
// TODO(iclelland): Once there is a proper JS inspection API from the renderer,
// use that to check the policy. Until then, we test webkitFullscreenEnabled,
// which conveniently just returns the result of calling isFeatureEnabled on
// the fullscreen feature. Since there are no HTTP header policies involved,
// this verifies the presence of the container policy in the iframe.
// https://crbug.com/703703
// TODO(lunalu): Currently feature policy is shipped without fullscreen (e.g.,
// the implementation of allowfullscreen does not use feature policy
// information). Once allowfullscreen is controlled by feature policy, re-enable
// this test.
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       DISABLED_ContainerPolicyCrossOriginNavigation) {
  WebContentsImpl* contents = web_contents();
  FrameTreeNode* root = contents->GetFrameTree()->root();

  // Helper to check if a frame is allowed to go fullscreen on the renderer
  // side.
  auto is_fullscreen_allowed = [](FrameTreeNode* ftn) {
    bool fullscreen_allowed = false;
    EXPECT_TRUE(ExecuteScriptAndExtractBool(
        ftn,
        "window.domAutomationController.send(document.webkitFullscreenEnabled)",
        &fullscreen_allowed));
    return fullscreen_allowed;
  };

  // Load a page with an <iframe> without allowFullscreen.
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   "a.com", "/cross_site_iframe_factory.html?a(b)")));

  // Dynamically enable fullscreen for the subframe and check that the
  // fullscreen property was updated on the FrameTreeNode.
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-0').allowFullscreen='true'"));

  // No change is expected to the container policy for dynamic modification of
  // a loaded frame.
  EXPECT_FALSE(is_fullscreen_allowed(root->child_at(0)));

  // Cross-site navigation should update the container policy in the new render
  // frame.
  NavigateFrameToURL(root->child_at(0),
                     embedded_test_server()->GetURL("c.com", "/title1.html"));
  EXPECT_TRUE(is_fullscreen_allowed(root->child_at(0)));
}

// Test that dynamic updates to iframe sandbox attribute correctly set the
// replicated container policy.
IN_PROC_BROWSER_TEST_F(SitePerProcessFeaturePolicyBrowserTest,
                       ContainerPolicySandboxDynamic) {
  GURL main_url(embedded_test_server()->GetURL("/allowed_frames.html"));
  GURL nav_url(
      embedded_test_server()->GetURL("b.com", "/feature-policy2.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = web_contents()->GetFrameTree()->root();

  // Validate that the effective container policy contains a single non-unique
  // origin.
  const ParsedFeaturePolicyHeader initial_effective_policy =
      root->child_at(2)->effective_container_policy();
  EXPECT_EQ(1UL, initial_effective_policy[0].origins.size());
  EXPECT_FALSE(initial_effective_policy[0].origins[0].unique());

  // Set the "sandbox" attribute; pending policy should update, and should now
  // contain a unique origin, but effective policy should remain unchanged.
  EXPECT_TRUE(ExecuteScript(
      root, "document.getElementById('child-2').setAttribute('sandbox','')"));
  const ParsedFeaturePolicyHeader updated_effective_policy =
      root->child_at(2)->effective_container_policy();
  const ParsedFeaturePolicyHeader updated_pending_policy =
      root->child_at(2)->pending_container_policy_;
  EXPECT_EQ(1UL, updated_effective_policy[0].origins.size());
  EXPECT_FALSE(updated_effective_policy[0].origins[0].unique());
  EXPECT_EQ(1UL, updated_pending_policy[0].origins.size());
  EXPECT_TRUE(updated_pending_policy[0].origins[0].unique());

  // Navigate the frame; pending policy should now be committed.
  NavigateFrameToURL(root->child_at(2), nav_url);
  const ParsedFeaturePolicyHeader final_effective_policy =
      root->child_at(2)->effective_container_policy();
  EXPECT_EQ(1UL, final_effective_policy[0].origins.size());
  EXPECT_TRUE(final_effective_policy[0].origins[0].unique());
}

// Test harness that allows for "barrier" style delaying of requests matching
// certain paths. Call SetDelayedRequestsForPath to delay requests, then
// SetUpEmbeddedTestServer to register handlers and start the server.
class RequestDelayingSitePerProcessBrowserTest
    : public SitePerProcessBrowserTest {
 public:
  RequestDelayingSitePerProcessBrowserTest()
      : test_server_(base::MakeUnique<net::EmbeddedTestServer>()) {}

  // Must be called after any calls to SetDelayedRequestsForPath.
  void SetUpEmbeddedTestServer() {
    SetupCrossSiteRedirector(test_server_.get());
    test_server_->RegisterRequestHandler(base::Bind(
        &RequestDelayingSitePerProcessBrowserTest::HandleMockResource,
        base::Unretained(this)));
    ASSERT_TRUE(test_server_->Start());
  }

  // Delays |num_delayed| requests with URLs whose path parts match |path|. When
  // the |num_delayed| + 1 request matching the path comes in, the rest are
  // unblocked.
  // Note: must be called on the UI thread before |test_server_| is started.
  void SetDelayedRequestsForPath(const std::string& path, int num_delayed) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(!test_server_->Started());
    num_remaining_requests_to_delay_for_path_[path] = num_delayed;
  }

 private:
  // Called on the test server's thread.
  void AddDelayedResponse(const net::test_server::SendBytesCallback& send,
                          const net::test_server::SendCompleteCallback& done) {
    // Just create a closure that closes the socket without sending a response.
    // This will propagate an error to the underlying request.
    send_response_closures_.push_back(base::Bind(send, "", done));
  }

  // Custom embedded test server handler. Looks for requests matching
  // num_remaining_requests_to_delay_for_path_, and delays them if necessary. As
  // soon as a single request comes in and:
  // 1) It matches a delayed path
  // 2) No path has any more requests to delay
  // Then we release the barrier and finish all delayed requests.
  std::unique_ptr<net::test_server::HttpResponse> HandleMockResource(
      const net::test_server::HttpRequest& request) {
    auto it =
        num_remaining_requests_to_delay_for_path_.find(request.GetURL().path());
    if (it == num_remaining_requests_to_delay_for_path_.end())
      return nullptr;

    // If there are requests to delay for this path, make a delayed request
    // which will be finished later. Otherwise fall through to the bottom and
    // send an empty response.
    if (it->second > 0) {
      --it->second;
      return base::MakeUnique<DelayedResponse>(this);
    }
    MaybeStartRequests();
    return std::unique_ptr<net::test_server::BasicHttpResponse>();
  }

  // If there are no more requests to delay, post a series of tasks finishing
  // all the delayed tasks. This will be called on the test server's thread.
  void MaybeStartRequests() {
    for (auto it : num_remaining_requests_to_delay_for_path_) {
      if (it.second > 0)
        return;
    }
    for (const auto it : send_response_closures_) {
      it.Run();
    }
  }

  // This class passes the callbacks needed to respond to a request to the
  // underlying test fixture.
  class DelayedResponse : public net::test_server::BasicHttpResponse {
   public:
    explicit DelayedResponse(
        RequestDelayingSitePerProcessBrowserTest* test_harness)
        : test_harness_(test_harness) {}
    void SendResponse(
        const net::test_server::SendBytesCallback& send,
        const net::test_server::SendCompleteCallback& done) override {
      test_harness_->AddDelayedResponse(send, done);
    }

   private:
    RequestDelayingSitePerProcessBrowserTest* test_harness_;

    DISALLOW_COPY_AND_ASSIGN(DelayedResponse);
  };

  // Set of closures to call which will complete delayed requests. May only be
  // modified on the test_server_'s thread.
  std::vector<base::Closure> send_response_closures_;

  // Map from URL paths to the number of requests to delay for that particular
  // path. Initialized on the UI thread but modified and read on the test
  // server's thread after the |test_server_| is started.
  std::map<std::string, int> num_remaining_requests_to_delay_for_path_;

  // Don't use embedded_test_server() because this one requires custom
  // initialization.
  std::unique_ptr<net::EmbeddedTestServer> test_server_;
};

// Regression tests for https://crbug.com/678206, where the request throttling
// in ResourceScheduler was not updated for OOPIFs. This resulted in a single
// hung delayable request (e.g. video) starving all other delayable requests.
// The tests work by delaying n requests in a cross-domain iframe. Once the n +
// 1st request goes through to the network stack (ensuring it was not starved),
// the delayed request completed.
//
// If the logic is not correct, these tests will time out, as the n + 1st
// request will never start.
IN_PROC_BROWSER_TEST_F(RequestDelayingSitePerProcessBrowserTest,
                       DelayableSubframeRequestsOneFrame) {
  std::string path = "/mock-video.mp4";
  SetDelayedRequestsForPath(path, 2);
  SetUpEmbeddedTestServer();
  GURL url(embedded_test_server()->GetURL(
      "a.com", base::StringPrintf("/site_isolation/"
                                  "subframes_with_resources.html?urls=%s&"
                                  "numSubresources=3",
                                  path.c_str())));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  bool result;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell(), "createFrames()", &result));
  EXPECT_TRUE(result);
}

IN_PROC_BROWSER_TEST_F(RequestDelayingSitePerProcessBrowserTest,
                       DelayableSubframeRequestsTwoFrames) {
  std::string path0 = "/mock-video0.mp4";
  std::string path1 = "/mock-video1.mp4";
  SetDelayedRequestsForPath(path0, 2);
  SetDelayedRequestsForPath(path1, 2);
  SetUpEmbeddedTestServer();
  GURL url(embedded_test_server()->GetURL(
      "a.com", base::StringPrintf("/site_isolation/"
                                  "subframes_with_resources.html?urls=%s,%s&"
                                  "numSubresources=3",
                                  path0.c_str(), path1.c_str())));
  EXPECT_TRUE(NavigateToURL(shell(), url));
  bool result;
  EXPECT_TRUE(ExecuteScriptAndExtractBool(shell(), "createFrames()", &result));
  EXPECT_TRUE(result);
}

#if defined(OS_ANDROID)
class TextSelectionObserver : public TextInputManager::Observer {
 public:
  explicit TextSelectionObserver(TextInputManager* text_input_manager)
      : text_input_manager_(text_input_manager) {
    text_input_manager->AddObserver(this);
  }

  ~TextSelectionObserver() { text_input_manager_->RemoveObserver(this); }

  void WaitForSelectedText(const std::string& expected_text) {
    if (last_selected_text_ == expected_text)
      return;
    expected_text_ = expected_text;
    loop_runner_ = new MessageLoopRunner();
    loop_runner_->Run();
  }

 private:
  void OnTextSelectionChanged(TextInputManager* text_input_manager,
                              RenderWidgetHostViewBase* updated_view) override {
    last_selected_text_ = base::UTF16ToUTF8(
        text_input_manager->GetTextSelection(updated_view)->selected_text());
    if (last_selected_text_ == expected_text_ && loop_runner_)
      loop_runner_->Quit();
  }

  TextInputManager* const text_input_manager_;
  std::string last_selected_text_;
  std::string expected_text_;
  scoped_refptr<MessageLoopRunner> loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(TextSelectionObserver);
};

class SitePerProcessAndroidImeTest : public SitePerProcessBrowserTest {
 public:
  SitePerProcessAndroidImeTest() : SitePerProcessBrowserTest() {}
  ~SitePerProcessAndroidImeTest() override {}

 protected:
  ImeAdapterAndroid* ime_adapter() {
    return static_cast<RenderWidgetHostViewAndroid*>(
               web_contents()->GetRenderWidgetHostView())
        ->ime_adapter_for_testing();
  }

  std::string GetInputValue(RenderFrameHostImpl* frame) {
    std::string result;
    EXPECT_TRUE(ExecuteScriptAndExtractString(
        frame, "window.domAutomationController.send(input.value);", &result));
    return result;
  }

  void FocusInputInFrame(RenderFrameHostImpl* frame) {
    ASSERT_TRUE(ExecuteScript(frame, "window.focus(); input.focus();"));
  }

  // Creates a page with multiple (nested) OOPIFs and populates all of them
  // with an <input> element along with the required handlers for the test.
  void LoadPage() {
    ASSERT_TRUE(NavigateToURL(
        shell(),
        GURL(embedded_test_server()->GetURL(
            "a.com", "/cross_site_iframe_factory.html?a(b,c(a(b)))"))));
    FrameTreeNode* root = web_contents()->GetFrameTree()->root();
    frames_.push_back(root->current_frame_host());
    frames_.push_back(root->child_at(0)->current_frame_host());
    frames_.push_back(root->child_at(1)->current_frame_host());
    frames_.push_back(root->child_at(1)->child_at(0)->current_frame_host());
    frames_.push_back(
        root->child_at(1)->child_at(0)->child_at(0)->current_frame_host());

    // Adds an <input> to frame and sets up a handler for |window.oninput|. When
    // the input event is fired (by changing the value of <input> element), the
    // handler will select all the text so that the corresponding text selection
    // update on the browser side notifies the test about input insertion.
    std::string add_input_script =
        "var input = document.createElement('input');"
        "document.body.appendChild(input);"
        "window.oninput = function() {"
        "  input.select();"
        "};";

    for (auto* frame : frames_)
      ASSERT_TRUE(ExecuteScript(frame, add_input_script));
  }

  // This methods tries to commit |text| by simulating a native call from Java.
  void CommitText(const char* text) {
    JNIEnv* env = base::android::AttachCurrentThread();

    // A valid caller is needed for ImeAdapterAndroid::GetUnderlinesFromSpans.
    base::android::ScopedJavaLocalRef<jobject> caller =
        ime_adapter()->java_ime_adapter_for_testing(env);

    // Input string from Java side.
    base::android::ScopedJavaLocalRef<jstring> jtext =
        base::android::ConvertUTF8ToJavaString(env, text);

    // Simulating a native call from Java side.
    ime_adapter()->CommitText(
        env, base::android::JavaParamRef<jobject>(env, caller.obj()),
        base::android::JavaParamRef<jobject>(env, jtext.obj()),
        base::android::JavaParamRef<jstring>(env, jtext.obj()), 0);
  }

  std::vector<RenderFrameHostImpl*> frames_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SitePerProcessAndroidImeTest);
};

// This test verifies that committing text will be applied on the focused
// RenderWidgetHost.
IN_PROC_BROWSER_TEST_F(SitePerProcessAndroidImeTest,
                       CommitTextForFocusedWidget) {
  LoadPage();
  TextSelectionObserver selection_observer(
      web_contents()->GetTextInputManager());
  for (size_t index = 0; index < frames_.size(); ++index) {
    std::string text = base::StringPrintf("text%zu", index);
    FocusInputInFrame(frames_[index]);
    CommitText(text.c_str());
    selection_observer.WaitForSelectedText(text);
  }
}
#endif  // OS_ANDROID

// Test that an OOPIF at b.com can navigate to a cross-site a.com URL that
// transfers back to b.com.  See https://crbug.com/681077#c10 and
// https://crbug.com/660407.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       SubframeTransfersToCurrentRFH) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b)"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();
  scoped_refptr<SiteInstanceImpl> b_site_instance =
      root->child_at(0)->current_frame_host()->GetSiteInstance();

  // Navigate subframe to a URL that will redirect from a.com back to b.com.
  // This navigation shouldn't time out.  Also ensure that the pending RFH
  // that was created for a.com is destroyed.
  GURL frame_url(
      embedded_test_server()->GetURL("a.com", "/cross-site/b.com/title2.html"));
  NavigateIframeToURL(shell()->web_contents(), "child-0", frame_url);
  EXPECT_FALSE(root->child_at(0)->render_manager()->pending_frame_host());
  GURL redirected_url(embedded_test_server()->GetURL("b.com", "/title2.html"));
  EXPECT_EQ(root->child_at(0)->current_url(), redirected_url);
  EXPECT_EQ(b_site_instance,
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Try the same navigation, but use the browser-initiated path.
  NavigateFrameToURL(root->child_at(0), frame_url);
  EXPECT_FALSE(root->child_at(0)->render_manager()->pending_frame_host());
  EXPECT_EQ(root->child_at(0)->current_url(), redirected_url);
  EXPECT_EQ(b_site_instance,
            root->child_at(0)->current_frame_host()->GetSiteInstance());
}

IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       FrameSwapPreservesUniqueName) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(a)"));
  ASSERT_TRUE(NavigateToURL(shell(), main_url));

  // Navigate the subframe cross-site…
  {
    GURL url(embedded_test_server()->GetURL("b.com", "/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "child-0", url));
  }
  // and then same-site…
  {
    GURL url(embedded_test_server()->GetURL("a.com", "/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "child-0", url));
  }
  // and cross-site once more.
  {
    GURL url(embedded_test_server()->GetURL("b.com", "/title1.html"));
    EXPECT_TRUE(NavigateIframeToURL(shell()->web_contents(), "child-0", url));
  }

  // Inspect the navigation entries and make sure that the navigation target
  // remained constant across frame swaps.
  const auto& controller = static_cast<const NavigationControllerImpl&>(
      shell()->web_contents()->GetController());
  EXPECT_EQ(4, controller.GetEntryCount());

  std::set<std::string> names;
  for (int i = 0; i < controller.GetEntryCount(); ++i) {
    NavigationEntryImpl::TreeNode* root =
        controller.GetEntryAtIndex(i)->root_node();
    ASSERT_EQ(1U, root->children.size());
    names.insert(root->children[0]->frame_entry->frame_unique_name());
  }

  // More than one entry in the set means that the subframe frame navigation
  // entries didn't have a consistent unique name. This will break history
  // navigations =(
  EXPECT_THAT(names, SizeIs(1)) << "Mismatched names for subframe!";
}

// Tests that POST body is not lost when it targets a OOPIF.
// See https://crbug.com/710937.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest, PostTargetSubFrame) {
  // Navigate to a page with an OOPIF.
  GURL main_url(
      embedded_test_server()->GetURL("/frame_tree/page_with_one_frame.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));
  FrameTreeNode* root = static_cast<WebContentsImpl*>(shell()->web_contents())
                            ->GetFrameTree()
                            ->root();

  // The main frame and the subframe live on different processes.
  EXPECT_EQ(1u, root->child_count());
  EXPECT_NE(root->current_frame_host()->GetSiteInstance(),
            root->child_at(0)->current_frame_host()->GetSiteInstance());

  // Make a form submission from the main frame and target the OOPIF.
  GURL form_url(embedded_test_server()->GetURL("/echoall"));
  TestNavigationObserver form_post_observer(shell()->web_contents(), 1);
  EXPECT_TRUE(ExecuteScript(shell()->web_contents(), R"(
    var form = document.createElement('form');

    // POST form submission to /echoall.
    form.setAttribute("method", "POST");
    form.setAttribute("action", ")" + form_url.spec() + R"(");

    // Target the OOPIF.
    form.setAttribute("target", "child-name-0");

    // Add some POST data: "my_token=my_value";
    var input = document.createElement("input");
    input.setAttribute("type", "hidden");
    input.setAttribute("name", "my_token");
    input.setAttribute("value", "my_value");
    form.appendChild(input);

    // Submit the form.
    document.body.appendChild(form);
    form.submit();
  )"));
  form_post_observer.Wait();

  NavigationEntryImpl* entry = static_cast<NavigationEntryImpl*>(
      shell()->web_contents()->GetController().GetLastCommittedEntry());
  // TODO(arthursonzogni): This is wrong. The last committed entry was
  // renderer-initiated. See https://crbug.com/722251.
  EXPECT_FALSE(entry->is_renderer_initiated());

  // Verify that POST body was correctly passed to the server and ended up in
  // the body of the page.
  std::string body;
  EXPECT_TRUE(ExecuteScriptAndExtractString(root->child_at(0), R"(
    var body = document.getElementsByTagName('pre')[0].innerText;
    window.domAutomationController.send(body);)",
                                            &body));
  EXPECT_EQ("my_token=my_value\n", body);
}

// Verify that a remote-to-local main frame navigation doesn't overwrite
// the previous history entry.  See https://crbug.com/725716.
IN_PROC_BROWSER_TEST_F(SitePerProcessBrowserTest,
                       CrossProcessMainFrameNavigationDoesNotOverwriteHistory) {
  GURL foo_url(embedded_test_server()->GetURL("foo.com", "/title1.html"));
  GURL bar_url(embedded_test_server()->GetURL("bar.com", "/title2.html"));

  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // Open a same-site popup to keep the www.foo.com process alive.
  OpenPopup(shell(), GURL(url::kAboutBlankURL), "foo");

  // Navigate foo -> bar -> foo.
  EXPECT_TRUE(NavigateToURL(shell(), bar_url));
  EXPECT_TRUE(NavigateToURL(shell(), foo_url));

  // There should be three history entries.
  EXPECT_EQ(3, web_contents()->GetController().GetEntryCount());

  // Go back: this should go to bar.com.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }
  EXPECT_EQ(bar_url, web_contents()->GetMainFrame()->GetLastCommittedURL());

  // Go back again.  This should go to foo.com.
  {
    TestNavigationObserver back_observer(web_contents());
    web_contents()->GetController().GoBack();
    back_observer.Wait();
  }
  EXPECT_EQ(foo_url, web_contents()->GetMainFrame()->GetLastCommittedURL());
}

}  // namespace content
