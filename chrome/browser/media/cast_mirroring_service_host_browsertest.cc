// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_mirroring_service_host.h"

#include "base/containers/flat_map.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/ui_base_features.h"

using testing::_;
using testing::InvokeWithoutArgs;

namespace mirroring {

namespace {

media::VideoCaptureParams DefaultVideoCaptureParams() {
  constexpr gfx::Size kMaxCaptureSize = gfx::Size(320, 320);
  constexpr int kMaxFramesPerSecond = 60;
  gfx::Size capture_size = kMaxCaptureSize;
  media::VideoCaptureParams params;
  params.requested_format = media::VideoCaptureFormat(
      capture_size, kMaxFramesPerSecond, media::PIXEL_FORMAT_I420);
  params.resolution_change_policy =
      media::ResolutionChangePolicy::FIXED_ASPECT_RATIO;
  return params;
}

content::DesktopMediaID BuildMediaIdForTabMirroring(
    content::WebContents* target_web_contents) {
  DCHECK(target_web_contents);
  content::DesktopMediaID media_id;
  content::RenderFrameHost* const main_frame =
      target_web_contents->GetMainFrame();
  const int process_id = main_frame->GetProcess()->GetID();
  const int frame_id = main_frame->GetRoutingID();
  media_id.type = content::DesktopMediaID::TYPE_WEB_CONTENTS;
  media_id.web_contents_id =
      content::WebContentsMediaCaptureId(process_id, frame_id, true, true);
  return media_id;
}

class MockVideoCaptureObserver final
    : public media::mojom::VideoCaptureObserver {
 public:
  explicit MockVideoCaptureObserver(media::mojom::VideoCaptureHostPtr host)
      : host_(std::move(host)), binding_(this) {}
  MOCK_METHOD1(OnBufferCreatedCall, void(int buffer_id));
  MOCK_METHOD1(OnBufferReadyCall, void(int buffer_id));
  MOCK_METHOD1(OnBufferDestroyedCall, void(int buffer_id));
  MOCK_METHOD1(OnStateChanged, void(media::mojom::VideoCaptureState state));

  // media::mojom::VideoCaptureObserver implementation.
  void OnNewBuffer(int32_t buffer_id,
                   media::mojom::VideoBufferHandlePtr buffer_handle) override {
    EXPECT_EQ(buffers_.find(buffer_id), buffers_.end());
    EXPECT_EQ(frame_infos_.find(buffer_id), frame_infos_.end());
    buffers_[buffer_id] = std::move(buffer_handle);
    OnBufferCreatedCall(buffer_id);
  }

  void OnBufferReady(int32_t buffer_id,
                     media::mojom::VideoFrameInfoPtr info) override {
    EXPECT_TRUE(buffers_.find(buffer_id) != buffers_.end());
    EXPECT_EQ(frame_infos_.find(buffer_id), frame_infos_.end());
    frame_infos_[buffer_id] = std::move(info);
    OnBufferReadyCall(buffer_id);
  }

  void OnBufferDestroyed(int32_t buffer_id) override {
    // The consumer should have finished consuming the buffer before it is being
    // destroyed.
    EXPECT_TRUE(frame_infos_.find(buffer_id) == frame_infos_.end());
    const auto iter = buffers_.find(buffer_id);
    EXPECT_TRUE(iter != buffers_.end());
    buffers_.erase(iter);
    OnBufferDestroyedCall(buffer_id);
  }

  void Start() {
    media::mojom::VideoCaptureObserverPtr observer;
    binding_.Bind(mojo::MakeRequest(&observer));
    host_->Start(0, 0, DefaultVideoCaptureParams(), std::move(observer));
  }

  void Stop() { host_->Stop(0); }

  void RequestRefreshFrame() { host_->RequestRefreshFrame(0); }

 private:
  media::mojom::VideoCaptureHostPtr host_;
  mojo::Binding<media::mojom::VideoCaptureObserver> binding_;
  base::flat_map<int, media::mojom::VideoBufferHandlePtr> buffers_;
  base::flat_map<int, media::mojom::VideoFrameInfoPtr> frame_infos_;

  DISALLOW_COPY_AND_ASSIGN(MockVideoCaptureObserver);
};

}  // namespace

class CastMirroringServiceHostBrowserTest : public InProcessBrowserTest,
                                            public SessionObserver,
                                            public CastMessageChannel {
 public:
  CastMirroringServiceHostBrowserTest() {}
  ~CastMirroringServiceHostBrowserTest() override {}

 protected:
  // Starts a tab mirroring session.
  void StartTabMirroring() {
    constexpr int kSessionID = 245;
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(web_contents);
    host_ = std::make_unique<CastMirroringServiceHost>(
        BuildMediaIdForTabMirroring(web_contents));
    host_->Start(kSessionID, CastSinkInfo(), this, this);
  }

  void GetVideoCaptureHost() {
    media::mojom::VideoCaptureHostPtr video_capture_host;
    static_cast<ResourceProvider*>(host_.get())
        ->GetVideoCaptureHost(mojo::MakeRequest(&video_capture_host));
    video_frame_receiver_ = std::make_unique<MockVideoCaptureObserver>(
        std::move(video_capture_host));
  }

  void StartVideoCapturing() {
    base::RunLoop run_loop;
    EXPECT_CALL(*video_frame_receiver_,
                OnStateChanged(media::mojom::VideoCaptureState::STARTED))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    video_frame_receiver_->Start();
    run_loop.Run();
  }

  void StopMirroring() {
    base::RunLoop run_loop;
    EXPECT_CALL(*video_frame_receiver_,
                OnStateChanged(media::mojom::VideoCaptureState::ENDED))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    video_frame_receiver_->Stop();
    run_loop.Run();
    host_.reset();
  }

  void RequestRefreshFrame() {
    base::RunLoop run_loop;
    EXPECT_CALL(*video_frame_receiver_, OnBufferReadyCall(_))
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    video_frame_receiver_->RequestRefreshFrame();
    run_loop.Run();
  }

  // InProcessBrowserTest override.
  void SetUp() override {
#if defined(OS_CHROMEOS)
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kMash, features::kMashDeprecated});
#endif
    InProcessBrowserTest::SetUp();
  }

 private:
#if defined(OS_CHROMEOS)
  base::test::ScopedFeatureList scoped_feature_list_;
#endif

  // SessionObserver mocks.
  MOCK_METHOD1(OnError, void(SessionError));
  MOCK_METHOD0(DidStart, void());
  MOCK_METHOD0(DidStop, void());

  // CastMessageChannel mocks.
  MOCK_METHOD1(Send, void(const CastMessage&));

  std::unique_ptr<CastMirroringServiceHost> host_;
  std::unique_ptr<MockVideoCaptureObserver> video_frame_receiver_;

  DISALLOW_COPY_AND_ASSIGN(CastMirroringServiceHostBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CastMirroringServiceHostBrowserTest, CaptureTabVideo) {
  StartTabMirroring();
  GetVideoCaptureHost();
  StartVideoCapturing();
  RequestRefreshFrame();
  StopMirroring();
}

}  // namespace mirroring
