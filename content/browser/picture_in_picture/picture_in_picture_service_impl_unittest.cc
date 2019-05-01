// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/picture_in_picture/picture_in_picture_service_impl.h"

#include <memory>
#include <utility>

#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/browser/overlay_window.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/interface_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class PictureInPictureDelegate : public WebContentsDelegate {
 public:
  PictureInPictureDelegate() = default;

  MOCK_METHOD3(EnterPictureInPicture,
               gfx::Size(WebContents*,
                         const viz::SurfaceId&,
                         const gfx::Size&));

 private:
  DISALLOW_COPY_AND_ASSIGN(PictureInPictureDelegate);
};

class TestOverlayWindow : public OverlayWindow {
 public:
  TestOverlayWindow() = default;
  ~TestOverlayWindow() override {}

  static std::unique_ptr<OverlayWindow> Create(
      PictureInPictureWindowController* controller) {
    return std::unique_ptr<OverlayWindow>(new TestOverlayWindow());
  }

  bool IsActive() const override { return false; }
  void Close() override {}
  void ShowInactive() override {}
  void Hide() override {}
  bool IsVisible() const override { return false; }
  bool IsAlwaysOnTop() const override { return false; }
  ui::Layer* GetLayer() override { return nullptr; }
  gfx::Rect GetBounds() const override { return gfx::Rect(); }
  void UpdateVideoSize(const gfx::Size& natural_size) override {}
  void SetPlaybackState(PlaybackState playback_state) override {}
  void SetAlwaysHidePlayPauseButton(bool is_visible) override {}
  void SetMutedState(MutedState muted_state) override {}
  void SetSkipAdButtonVisibility(bool is_visible) override {}
  void SetNextTrackButtonVisibility(bool is_visible) override {}
  void SetPreviousTrackButtonVisibility(bool is_visible) override {}
  void SetSurfaceId(const viz::SurfaceId& surface_id) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestOverlayWindow);
};

class PictureInPictureTestBrowserClient : public TestContentBrowserClient {
 public:
  PictureInPictureTestBrowserClient() = default;
  ~PictureInPictureTestBrowserClient() override = default;

  std::unique_ptr<OverlayWindow> CreateWindowForPictureInPicture(
      PictureInPictureWindowController* controller) override {
    return TestOverlayWindow::Create(controller);
  }
};

class PictureInPictureServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    // WebUIControllerFactory::RegisterFactory(
    //     ContentWebUIControllerFactory::GetInstance());

    SetBrowserClientForTesting(&browser_client_);

    TestRenderFrameHost* render_frame_host = contents()->GetMainFrame();
    render_frame_host->InitializeRenderFrameIfNeeded();

    contents()->SetDelegate(&delegate_);

    blink::mojom::PictureInPictureServiceRequest request;
    service_impl_ = PictureInPictureServiceImpl::CreateForTesting(
        render_frame_host, std::move(request));
  }

  void TearDown() override {
    // WebUIControllerFactory::UnregisterFactoryForTesting(
    //     ContentWebUIControllerFactory::GetInstance());
    RenderViewHostImplTestHarness::TearDown();
  }

  PictureInPictureServiceImpl& service() { return *service_impl_; }

  PictureInPictureDelegate& delegate() { return delegate_; }

 private:
  PictureInPictureTestBrowserClient browser_client_;
  PictureInPictureDelegate delegate_;
  // Will be deleted when the frame is destroyed.
  PictureInPictureServiceImpl* service_impl_;
};

TEST_F(PictureInPictureServiceImplTest, EnterPictureInPicture) {
  const int kPlayerVideoOnlyId = 30;

  // If Picture-in-Picture was never triggered, the media player id would not be
  // set.
  EXPECT_FALSE(service().player_id().has_value());

  viz::SurfaceId surface_id =
      viz::SurfaceId(viz::FrameSinkId(1, 1),
                     viz::LocalSurfaceId(
                         11, base::UnguessableToken::Deserialize(0x111111, 0)));

  EXPECT_CALL(delegate(),
              EnterPictureInPicture(contents(), surface_id, gfx::Size(42, 42)));

  service().StartSession(kPlayerVideoOnlyId, surface_id, gfx::Size(42, 42),
                         true /* show_play_pause_button */,
                         true /* show_mute_button */, base::DoNothing());
  EXPECT_TRUE(service().player_id().has_value());
  EXPECT_EQ(kPlayerVideoOnlyId, service().player_id()->delegate_id);

  // Picture-in-Picture media player id should not be reset when the media is
  // destroyed (e.g. video stops playing). This allows the Picture-in-Picture
  // window to continue to control the media.
  contents()->GetMainFrame()->OnMessageReceived(
      MediaPlayerDelegateHostMsg_OnMediaDestroyed(
          contents()->GetMainFrame()->GetRoutingID(), kPlayerVideoOnlyId));
  EXPECT_TRUE(service().player_id().has_value());
}

}  // namespace content
