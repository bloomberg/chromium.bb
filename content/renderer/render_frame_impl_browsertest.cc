// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/common/frame_messages.h"
#include "content/common/frame_owner_properties.h"
#include "content/common/renderer.mojom.h"
#include "content/common/view_messages.h"
#include "content/public/common/previews_state.h"
#include "content/public/renderer/document_state.h"
#include "content/public/test/frame_load_waiter.h"
#include "content/public/test/render_view_test.h"
#include "content/public/test/test_utils.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/navigation_state_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/test/fake_compositor_dependencies.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebEffectiveConnectionType.h"
#include "third_party/WebKit/public/platform/WebRuntimeFeatures.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebHistoryItem.h"
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/native_theme/native_theme_features.h"

using blink::WebString;
using blink::WebURLRequest;

namespace {
const int32_t kSubframeRouteId = 20;
const int32_t kSubframeWidgetRouteId = 21;
const int32_t kFrameProxyRouteId = 22;
}  // namespace

namespace content {

// RenderFrameImplTest creates a RenderFrameImpl that is a child of the
// main frame, and has its own RenderWidget. This behaves like an out
// of process frame even though it is in the same process as its parent.
class RenderFrameImplTest : public RenderViewTest {
 public:
  ~RenderFrameImplTest() override {}

  void SetUp() override {
    blink::WebRuntimeFeatures::EnableOverlayScrollbars(
        ui::IsOverlayScrollbarEnabled());
    RenderViewTest::SetUp();
    EXPECT_TRUE(GetMainRenderFrame()->is_main_frame_);

    mojom::CreateFrameWidgetParams widget_params;
    widget_params.routing_id = kSubframeWidgetRouteId;
    widget_params.hidden = false;

    IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

    LoadHTML("Parent frame <iframe name='frame'></iframe>");

    FrameReplicationState frame_replication_state;
    frame_replication_state.name = "frame";
    frame_replication_state.unique_name = "frame-uniqueName";

    RenderFrameImpl::FromWebFrame(
        view_->GetMainRenderFrame()->GetWebFrame()->FirstChild())
        ->OnSwapOut(kFrameProxyRouteId, false, frame_replication_state);

    RenderFrameImpl::CreateFrame(
        kSubframeRouteId, MSG_ROUTING_NONE, MSG_ROUTING_NONE,
        kFrameProxyRouteId, MSG_ROUTING_NONE, base::UnguessableToken::Create(),
        frame_replication_state, &compositor_deps_, widget_params,
        FrameOwnerProperties());

    frame_ = RenderFrameImpl::FromRoutingID(kSubframeRouteId);
    EXPECT_FALSE(frame_->is_main_frame_);
  }

  void TearDown() override {
#if defined(LEAK_SANITIZER)
     // Do this before shutting down V8 in RenderViewTest::TearDown().
     // http://crbug.com/328552
     __lsan_do_leak_check();
#endif
     RenderViewTest::TearDown();
  }

  void SetPreviewsState(RenderFrameImpl* frame, PreviewsState previews_state) {
    frame->previews_state_ = previews_state;
  }

  void SetEffectionConnectionType(RenderFrameImpl* frame,
                                  blink::WebEffectiveConnectionType type) {
    frame->effective_connection_type_ = type;
  }

  RenderFrameImpl* GetMainRenderFrame() {
    return static_cast<RenderFrameImpl*>(view_->GetMainRenderFrame());
  }

  RenderFrameImpl* frame() { return frame_; }

  content::RenderWidget* frame_widget() const {
    return frame_->render_widget_.get();
  }

#if defined(OS_ANDROID)
  void ReceiveOverlayRoutingToken(const base::UnguessableToken& token) {
    overlay_routing_token_ = token;
  }

  base::Optional<base::UnguessableToken> overlay_routing_token_;
#endif

 private:
  RenderFrameImpl* frame_;
  FakeCompositorDependencies compositor_deps_;
};

class RenderFrameTestObserver : public RenderFrameObserver {
 public:
  explicit RenderFrameTestObserver(RenderFrame* render_frame)
      : RenderFrameObserver(render_frame), visible_(false) {}

  ~RenderFrameTestObserver() override {}

  // RenderFrameObserver implementation.
  void WasShown() override { visible_ = true; }
  void WasHidden() override { visible_ = false; }
  void OnDestruct() override { delete this; }

  bool visible() { return visible_; }

 private:
  bool visible_;
};

// Verify that a frame with a RenderFrameProxy as a parent has its own
// RenderWidget.
TEST_F(RenderFrameImplTest, SubframeWidget) {
  EXPECT_TRUE(frame_widget());
  EXPECT_NE(frame_widget(), static_cast<RenderViewImpl*>(view_)->GetWidget());
}

// Verify a subframe RenderWidget properly processes its viewport being
// resized.
TEST_F(RenderFrameImplTest, FrameResize) {
  ResizeParams resize_params;
  gfx::Size size(200, 200);
  resize_params.screen_info = ScreenInfo();
  resize_params.new_size = size;
  resize_params.physical_backing_size = size;
  resize_params.visible_viewport_size = size;
  resize_params.top_controls_height = 0.f;
  resize_params.browser_controls_shrink_blink_size = false;
  resize_params.is_fullscreen_granted = false;

  ViewMsg_Resize resize_message(0, resize_params);
  frame_widget()->OnMessageReceived(resize_message);

  EXPECT_EQ(frame_widget()->GetWebWidget()->Size(), blink::WebSize(size));
  EXPECT_EQ(view_->GetWebView()->Size(), blink::WebSize(size));
}

// Verify a subframe RenderWidget properly processes a WasShown message.
TEST_F(RenderFrameImplTest, FrameWasShown) {
  RenderFrameTestObserver observer(frame());

  ViewMsg_WasShown was_shown_message(0, true, ui::LatencyInfo());
  frame_widget()->OnMessageReceived(was_shown_message);

  EXPECT_FALSE(frame_widget()->is_hidden());
  EXPECT_TRUE(observer.visible());
}

// Ensure that a RenderFrameImpl does not crash if the RenderView receives
// a WasShown message after the frame's widget has been closed.
TEST_F(RenderFrameImplTest, FrameWasShownAfterWidgetClose) {
  RenderFrameTestObserver observer(frame());

  ViewMsg_Close close_message(0);
  frame_widget()->OnMessageReceived(close_message);

  ViewMsg_WasShown was_shown_message(0, true, ui::LatencyInfo());
  static_cast<RenderViewImpl*>(view_)->OnMessageReceived(was_shown_message);

  // This test is primarily checking that this case does not crash, but
  // observers should still be notified.
  EXPECT_TRUE(observer.visible());
}

// Test that LoFi state only updates for new main frame documents. Subframes
// inherit from the main frame and should not change at commit time.
TEST_F(RenderFrameImplTest, LoFiNotUpdatedOnSubframeCommits) {
  SetPreviewsState(GetMainRenderFrame(), SERVER_LOFI_ON);
  SetPreviewsState(frame(), SERVER_LOFI_ON);
  EXPECT_EQ(SERVER_LOFI_ON, GetMainRenderFrame()->GetPreviewsState());
  EXPECT_EQ(SERVER_LOFI_ON, frame()->GetPreviewsState());

  blink::WebHistoryItem item;
  item.Initialize();

  // The main frame's and subframe's LoFi states should stay the same on
  // navigations within the page.
  frame()->DidNavigateWithinPage(item, blink::kWebStandardCommit, true);
  EXPECT_EQ(SERVER_LOFI_ON, frame()->GetPreviewsState());
  GetMainRenderFrame()->DidNavigateWithinPage(item, blink::kWebStandardCommit,
                                              true);
  EXPECT_EQ(SERVER_LOFI_ON, GetMainRenderFrame()->GetPreviewsState());

  // The subframe's LoFi state should not be reset on commit.
  DocumentState* document_state = DocumentState::FromDocumentLoader(
      frame()->GetWebFrame()->GetDocumentLoader());
  static_cast<NavigationStateImpl*>(document_state->navigation_state())
      ->set_was_within_same_document(false);

  frame()->DidCommitProvisionalLoad(item, blink::kWebStandardCommit);
  EXPECT_EQ(SERVER_LOFI_ON, frame()->GetPreviewsState());

  // The main frame's LoFi state should be reset to off on commit.
  document_state = DocumentState::FromDocumentLoader(
      GetMainRenderFrame()->GetWebFrame()->GetDocumentLoader());
  static_cast<NavigationStateImpl*>(document_state->navigation_state())
      ->set_was_within_same_document(false);

  // Calling didCommitProvisionalLoad is not representative of a full navigation
  // but serves the purpose of testing the LoFi state logic.
  GetMainRenderFrame()->DidCommitProvisionalLoad(item,
                                                 blink::kWebStandardCommit);
  EXPECT_EQ(PREVIEWS_OFF, GetMainRenderFrame()->GetPreviewsState());
  // The subframe would be deleted here after a cross-document navigation. It
  // happens to be left around in this test because this does not simulate the
  // frame detach.
}

// Test that effective connection type only updates for new main frame
// documents.
TEST_F(RenderFrameImplTest, EffectiveConnectionType) {
  EXPECT_EQ(blink::WebEffectiveConnectionType::kTypeUnknown,
            frame()->GetEffectiveConnectionType());
  EXPECT_EQ(blink::WebEffectiveConnectionType::kTypeUnknown,
            GetMainRenderFrame()->GetEffectiveConnectionType());

  const struct {
    blink::WebEffectiveConnectionType type;
  } tests[] = {{blink::WebEffectiveConnectionType::kTypeUnknown},
               {blink::WebEffectiveConnectionType::kType2G},
               {blink::WebEffectiveConnectionType::kType4G}};

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SetEffectionConnectionType(GetMainRenderFrame(), tests[i].type);
    SetEffectionConnectionType(frame(), tests[i].type);

    EXPECT_EQ(tests[i].type, frame()->GetEffectiveConnectionType());
    EXPECT_EQ(tests[i].type,
              GetMainRenderFrame()->GetEffectiveConnectionType());

    blink::WebHistoryItem item;
    item.Initialize();

    // The main frame's and subframe's effective connection type should stay the
    // same on navigations within the page.
    frame()->DidNavigateWithinPage(item, blink::kWebStandardCommit, true);
    EXPECT_EQ(tests[i].type, frame()->GetEffectiveConnectionType());
    GetMainRenderFrame()->DidNavigateWithinPage(item, blink::kWebStandardCommit,
                                                true);
    EXPECT_EQ(tests[i].type, frame()->GetEffectiveConnectionType());

    // The subframe's effective connection type should not be reset on commit.
    DocumentState* document_state = DocumentState::FromDocumentLoader(
        frame()->GetWebFrame()->GetDocumentLoader());
    static_cast<NavigationStateImpl*>(document_state->navigation_state())
        ->set_was_within_same_document(false);

    frame()->DidCommitProvisionalLoad(item, blink::kWebStandardCommit);
    EXPECT_EQ(tests[i].type, frame()->GetEffectiveConnectionType());

    // The main frame's effective connection type should be reset on commit.
    document_state = DocumentState::FromDocumentLoader(
        GetMainRenderFrame()->GetWebFrame()->GetDocumentLoader());
    static_cast<NavigationStateImpl*>(document_state->navigation_state())
        ->set_was_within_same_document(false);

    GetMainRenderFrame()->DidCommitProvisionalLoad(item,
                                                   blink::kWebStandardCommit);
    EXPECT_EQ(blink::WebEffectiveConnectionType::kTypeUnknown,
              GetMainRenderFrame()->GetEffectiveConnectionType());

    // The subframe would be deleted here after a cross-document navigation.
    // It happens to be left around in this test because this does not simulate
    // the frame detach.
  }
}

TEST_F(RenderFrameImplTest, SaveImageFromDataURL) {
  const IPC::Message* msg1 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_SaveImageFromDataURL::ID);
  EXPECT_FALSE(msg1);
  render_thread_->sink().ClearMessages();

  const std::string image_data_url =
      "data:image/gif;base64,R0lGODlhAQABAIAAAAUEBAAAACwAAAAAAQABAAACAkQBADs=";

  frame()->SaveImageFromDataURL(WebString::FromUTF8(image_data_url));
  base::RunLoop().RunUntilIdle();
  const IPC::Message* msg2 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_SaveImageFromDataURL::ID);
  EXPECT_TRUE(msg2);

  FrameHostMsg_SaveImageFromDataURL::Param param1;
  FrameHostMsg_SaveImageFromDataURL::Read(msg2, &param1);
  EXPECT_EQ(std::get<2>(param1), image_data_url);

  base::RunLoop().RunUntilIdle();
  render_thread_->sink().ClearMessages();

  const std::string large_data_url(1024 * 1024 * 20 - 1, 'd');

  frame()->SaveImageFromDataURL(WebString::FromUTF8(large_data_url));
  base::RunLoop().RunUntilIdle();
  const IPC::Message* msg3 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_SaveImageFromDataURL::ID);
  EXPECT_TRUE(msg3);

  FrameHostMsg_SaveImageFromDataURL::Param param2;
  FrameHostMsg_SaveImageFromDataURL::Read(msg3, &param2);
  EXPECT_EQ(std::get<2>(param2), large_data_url);

  base::RunLoop().RunUntilIdle();
  render_thread_->sink().ClearMessages();

  const std::string exceeded_data_url(1024 * 1024 * 20 + 1, 'd');

  frame()->SaveImageFromDataURL(WebString::FromUTF8(exceeded_data_url));
  base::RunLoop().RunUntilIdle();
  const IPC::Message* msg4 = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_SaveImageFromDataURL::ID);
  EXPECT_FALSE(msg4);
}

TEST_F(RenderFrameImplTest, ZoomLimit) {
  const double kMinZoomLevel = ZoomFactorToZoomLevel(kMinimumZoomFactor);
  const double kMaxZoomLevel = ZoomFactorToZoomLevel(kMaximumZoomFactor);

  // Verifies navigation to a URL with preset zoom level indeed sets the level.
  // Regression test for http://crbug.com/139559, where the level was not
  // properly set when it is out of the default zoom limits of WebView.
  CommonNavigationParams common_params;
  common_params.url = GURL("data:text/html,min_zoomlimit_test");
  common_params.navigation_type = FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT;
  GetMainRenderFrame()->SetHostZoomLevel(common_params.url, kMinZoomLevel);
  GetMainRenderFrame()->NavigateInternal(
      common_params, StartNavigationParams(), RequestNavigationParams(),
      std::unique_ptr<StreamOverrideParameters>());
  base::RunLoop().RunUntilIdle();
  EXPECT_DOUBLE_EQ(kMinZoomLevel, view_->GetWebView()->ZoomLevel());

  // It should work even when the zoom limit is temporarily changed in the page.
  view_->GetWebView()->ZoomLimitsChanged(ZoomFactorToZoomLevel(1.0),
                                         ZoomFactorToZoomLevel(1.0));
  common_params.url = GURL("data:text/html,max_zoomlimit_test");
  GetMainRenderFrame()->SetHostZoomLevel(common_params.url, kMaxZoomLevel);
  GetMainRenderFrame()->NavigateInternal(
      common_params, StartNavigationParams(), RequestNavigationParams(),
      std::unique_ptr<StreamOverrideParameters>());
  base::RunLoop().RunUntilIdle();
  EXPECT_DOUBLE_EQ(kMaxZoomLevel, view_->GetWebView()->ZoomLevel());
}

// Regression test for crbug.com/692557. It shouldn't crash if we inititate a
// text finding, and then delete the frame immediately before the text finding
// returns any text match.
TEST_F(RenderFrameImplTest, NoCrashWhenDeletingFrameDuringFind) {
  blink::WebFindOptions options;
  options.force = true;
  FrameMsg_Find find_message(0, 1, base::ASCIIToUTF16("foo"), options);
  frame()->OnMessageReceived(find_message);

  FrameMsg_Delete delete_message(0);
  frame()->OnMessageReceived(delete_message);
}

#if defined(OS_ANDROID)
// Verify that RFI defers token requests if the token hasn't arrived yet.
TEST_F(RenderFrameImplTest, TestOverlayRoutingTokenSendsLater) {
  ASSERT_FALSE(overlay_routing_token_.has_value());

  frame()->RequestOverlayRoutingToken(
      base::Bind(&RenderFrameImplTest::ReceiveOverlayRoutingToken,
                 base::Unretained(this)));
  ASSERT_FALSE(overlay_routing_token_.has_value());

  // The host should receive a request for it sent to the frame.
  const IPC::Message* msg = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_RequestOverlayRoutingToken::ID);
  EXPECT_TRUE(msg);

  // Send a token.
  base::UnguessableToken token = base::UnguessableToken::Create();
  FrameMsg_SetOverlayRoutingToken token_message(0, token);
  frame()->OnMessageReceived(token_message);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(overlay_routing_token_.has_value());
  ASSERT_EQ(overlay_routing_token_.value(), token);
}

// Verify that RFI sends tokens if they're already available.
TEST_F(RenderFrameImplTest, TestOverlayRoutingTokenSendsNow) {
  ASSERT_FALSE(overlay_routing_token_.has_value());
  base::UnguessableToken token = base::UnguessableToken::Create();
  FrameMsg_SetOverlayRoutingToken token_message(0, token);
  frame()->OnMessageReceived(token_message);

  // The frame now has a token.  We don't care if it sends the token before
  // returning or posts a message.
  base::RunLoop().RunUntilIdle();
  frame()->RequestOverlayRoutingToken(
      base::Bind(&RenderFrameImplTest::ReceiveOverlayRoutingToken,
                 base::Unretained(this)));
  ASSERT_TRUE(overlay_routing_token_.has_value());
  ASSERT_EQ(overlay_routing_token_.value(), token);

  // Since the token already arrived, a request for it shouldn't be sent.
  const IPC::Message* msg = render_thread_->sink().GetFirstMessageMatching(
      FrameHostMsg_RequestOverlayRoutingToken::ID);
  EXPECT_FALSE(msg);
}
#endif

TEST_F(RenderFrameImplTest, PreviewsStateAfterWillSendRequest) {
  const struct {
    PreviewsState frame_previews_state;
    WebURLRequest::PreviewsState initial_request_previews_state;
    WebURLRequest::PreviewsState expected_final_request_previews_state;
  } tests[] = {
      // With no previews enabled for the frame, no previews should be
      // activated.
      {PREVIEWS_UNSPECIFIED, WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kPreviewsOff},

      // If the request already has a previews state set, then it shouldn't be
      // overridden.
      {SERVER_LOFI_ON, WebURLRequest::kPreviewsNoTransform,
       WebURLRequest::kPreviewsNoTransform},
      {SERVER_LOFI_ON, WebURLRequest::kPreviewsOff,
       WebURLRequest::kPreviewsOff},

      // Server Lo-Fi and Server Lite Pages should be enabled for the request
      // when they're enabled for the frame.
      {SERVER_LOFI_ON, WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kServerLoFiOn},
      {SERVER_LITE_PAGE_ON, WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kServerLitePageOn},
      {SERVER_LITE_PAGE_ON | SERVER_LOFI_ON,
       WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kServerLitePageOn | WebURLRequest::kServerLoFiOn},

      // The CLIENT_LOFI_ON frame flag should be ignored at this point in the
      // request.
      {CLIENT_LOFI_ON, WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kPreviewsOff},
      {SERVER_LOFI_ON | CLIENT_LOFI_ON, WebURLRequest::kPreviewsUnspecified,
       WebURLRequest::kServerLoFiOn},

      // A request that's using Client Lo-Fi should continue using Client Lo-Fi.
      {SERVER_LOFI_ON | CLIENT_LOFI_ON, WebURLRequest::kClientLoFiOn,
       WebURLRequest::kClientLoFiOn},
      {CLIENT_LOFI_ON, WebURLRequest::kClientLoFiOn,
       WebURLRequest::kClientLoFiOn},
      {SERVER_LITE_PAGE_ON, WebURLRequest::kClientLoFiOn,
       WebURLRequest::kClientLoFiOn},
  };

  for (const auto& test : tests) {
    SetPreviewsState(frame(), test.frame_previews_state);

    WebURLRequest request;
    request.SetURL(GURL("http://example.com"));
    request.SetPreviewsState(test.initial_request_previews_state);

    frame()->WillSendRequest(request);

    EXPECT_EQ(test.expected_final_request_previews_state,
              request.GetPreviewsState())
        << (&test - tests);
  }
}

TEST_F(RenderFrameImplTest, IsClientLoFiActiveForFrame) {
  const struct {
    PreviewsState frame_previews_state;
    bool expected_is_client_lo_fi_active_for_frame;
  } tests[] = {
      // With no previews enabled for the frame, no previews should be
      // activated.
      {PREVIEWS_UNSPECIFIED, false},

      // Server Lo-Fi should not make Client Lo-Fi active.
      {SERVER_LOFI_ON, false},

      // PREVIEWS_NO_TRANSFORM and PREVIEWS_OFF should
      // take precedence over Client Lo-Fi.
      {CLIENT_LOFI_ON | PREVIEWS_NO_TRANSFORM, false},
      {CLIENT_LOFI_ON | PREVIEWS_OFF, false},

      // Otherwise, if Client Lo-Fi is enabled on its own or with
      // SERVER_LOFI_ON, then it is active for the frame.
      {CLIENT_LOFI_ON, true},
      {CLIENT_LOFI_ON | SERVER_LOFI_ON, true},
  };

  for (const auto& test : tests) {
    SetPreviewsState(frame(), test.frame_previews_state);

    EXPECT_EQ(test.expected_is_client_lo_fi_active_for_frame,
              frame()->IsClientLoFiActiveForFrame())
        << (&test - tests);
  }
}

TEST_F(RenderFrameImplTest, ShouldUseClientLoFiForRequest) {
  const struct {
    PreviewsState frame_previews_state;
    bool is_https;
    WebURLRequest::PreviewsState initial_request_previews_state;
    bool expected_should_use_client_lo_fi_for_request;
  } tests[] = {
      // With no previews enabled for the frame, no previews should be
      // activated.
      {PREVIEWS_UNSPECIFIED, false, WebURLRequest::kPreviewsUnspecified, false},

      // If the request already has a previews state set, then Client Lo-Fi
      // should only be used if the request already has that bit set.
      {CLIENT_LOFI_ON, false, WebURLRequest::kServerLoFiOn, false},
      {PREVIEWS_UNSPECIFIED, false, WebURLRequest::kClientLoFiOn, true},
      {CLIENT_LOFI_ON, false, WebURLRequest::kClientLoFiOn, true},
      {CLIENT_LOFI_ON | SERVER_LITE_PAGE_ON, true,
       WebURLRequest::kPreviewsUnspecified, true},
      {CLIENT_LOFI_ON | SERVER_LITE_PAGE_ON, false,
       WebURLRequest::kPreviewsUnspecified, true},

      // If Client Lo-Fi isn't enabled for the frame, then it shouldn't be used
      // for any requests.
      {SERVER_LOFI_ON, true, WebURLRequest::kPreviewsUnspecified, false},

      // PREVIEWS_NO_TRANSFORM and PREVIEWS_OFF should take precedence
      // over Client Lo-Fi.
      {CLIENT_LOFI_ON | PREVIEWS_NO_TRANSFORM, false,
       WebURLRequest::kPreviewsUnspecified, false},
      {CLIENT_LOFI_ON | PREVIEWS_OFF, false,
       WebURLRequest::kPreviewsUnspecified, false},

      // If both Server Lo-Fi and Client Lo-Fi are enabled for the frame, then
      // only https:// requests should use Client Lo-Fi.
      {CLIENT_LOFI_ON | SERVER_LOFI_ON, false,
       WebURLRequest::kPreviewsUnspecified, false},
      {CLIENT_LOFI_ON | SERVER_LOFI_ON, true,
       WebURLRequest::kPreviewsUnspecified, true},

      // Otherwise, if Client Lo-Fi is enabled on its own, then requests should
      // use Client Lo-Fi.
      {CLIENT_LOFI_ON, false, WebURLRequest::kPreviewsUnspecified, true},
      {CLIENT_LOFI_ON, true, WebURLRequest::kPreviewsUnspecified, true},
  };

  for (const auto& test : tests) {
    SetPreviewsState(frame(), test.frame_previews_state);

    WebURLRequest request;
    request.SetURL(
        GURL(test.is_https ? "https://example.com" : "http://example.com"));
    request.SetPreviewsState(test.initial_request_previews_state);

    EXPECT_EQ(test.expected_should_use_client_lo_fi_for_request,
              frame()->ShouldUseClientLoFiForRequest(request))
        << (&test - tests);
  }
}

}  // namespace content
