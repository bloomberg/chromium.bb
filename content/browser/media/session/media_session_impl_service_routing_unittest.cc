// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/session/media_session_impl.h"

#include <map>
#include <memory>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/media/session/media_session_player_observer.h"
#include "content/browser/media/session/media_session_service_impl.h"
#include "content/browser/media/session/mock_media_session_observer.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "media/base/media_content_type.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Eq;

namespace content {

namespace {

static const int kPlayerId = 0;

class MockMediaSessionServiceImpl : public MediaSessionServiceImpl {
 public:
  explicit MockMediaSessionServiceImpl(RenderFrameHost* rfh)
      : MediaSessionServiceImpl(rfh) {}
  ~MockMediaSessionServiceImpl() override = default;
};

class MockMediaSessionPlayerObserver : public MediaSessionPlayerObserver {
 public:
  explicit MockMediaSessionPlayerObserver(RenderFrameHost* rfh)
      : render_frame_host_(rfh) {}

  ~MockMediaSessionPlayerObserver() override = default;

  void OnSuspend(int player_id) override {}
  void OnResume(int player_id) override {}
  void OnSetVolumeMultiplier(int player_id, double volume_multiplier) override {
  }
  RenderFrameHost* GetRenderFrameHost() const override {
    return render_frame_host_;
  }

 private:
  RenderFrameHost* render_frame_host_;
};

}  // anonymous namespace

class MediaSessionImplServiceRoutingTest
    : public RenderViewHostImplTestHarness {
 public:
  MediaSessionImplServiceRoutingTest() = default;
  ~MediaSessionImplServiceRoutingTest() override = default;

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();

    contents()->GetMainFrame()->InitializeRenderFrameIfNeeded();
    mock_media_session_observer_.reset(
        new MockMediaSessionObserver(MediaSessionImpl::Get(contents())));
    main_frame_ = contents()->GetMainFrame();
    sub_frame_ = main_frame_->AppendChild("sub_frame");

    player_in_main_frame_.reset(
        new MockMediaSessionPlayerObserver(main_frame_));
    player_in_sub_frame_.reset(new MockMediaSessionPlayerObserver(sub_frame_));
  }

  void TearDown() override {
    mock_media_session_observer_.reset();
    services_.clear();

    RenderViewHostImplTestHarness::TearDown();
  }

 protected:
  MockMediaSessionObserver* mock_media_session_observer() {
    return mock_media_session_observer_.get();
  }

  void CreateServiceForFrame(TestRenderFrameHost* frame) {
    services_[frame] = base::MakeUnique<MockMediaSessionServiceImpl>(frame);
  }

  void DestroyServiceForFrame(TestRenderFrameHost* frame) {
    services_.erase(frame);
  }

  void StartPlayerForFrame(TestRenderFrameHost* frame) {
    players_[frame] = base::MakeUnique<MockMediaSessionPlayerObserver>(frame);
    MediaSessionImpl::Get(contents())
        ->AddPlayer(players_[frame].get(), kPlayerId,
                    media::MediaContentType::Persistent);
  }

  void ClearPlayersForFrame(TestRenderFrameHost* frame) {
    if (!players_.count(frame))
      return;

    MediaSessionImpl::Get(contents())
        ->RemovePlayer(players_[frame].get(), kPlayerId);
  }

  MediaSessionServiceImpl* ComputeServiceForRouting() {
    return MediaSessionImpl::Get(contents())->ComputeServiceForRouting();
  }

  TestRenderFrameHost* main_frame_;
  TestRenderFrameHost* sub_frame_;

  std::unique_ptr<MockMediaSessionPlayerObserver> player_in_main_frame_;
  std::unique_ptr<MockMediaSessionPlayerObserver> player_in_sub_frame_;

  std::unique_ptr<MockMediaSessionObserver> mock_media_session_observer_;

  using ServiceMap = std::map<TestRenderFrameHost*,
                              std::unique_ptr<MockMediaSessionServiceImpl>>;
  ServiceMap services_;

  using PlayerMap = std::map<TestRenderFrameHost*,
                             std::unique_ptr<MockMediaSessionPlayerObserver>>;
  PlayerMap players_;
};

TEST_F(MediaSessionImplServiceRoutingTest, NoFrameProducesAudio) {
  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlyMainFrameProducesAudioButHasNoService) {
  StartPlayerForFrame(main_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlySubFrameProducesAudioButHasNoService) {
  StartPlayerForFrame(sub_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlyMainFrameProducesAudioButHasDestroyedService) {
  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);
  DestroyServiceForFrame(main_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlySubFrameProducesAudioButHasDestroyedService) {
  CreateServiceForFrame(sub_frame_);
  StartPlayerForFrame(sub_frame_);
  DestroyServiceForFrame(sub_frame_);

  ASSERT_EQ(nullptr, ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlyMainFrameProducesAudioAndServiceIsCreatedAfterwards) {
  StartPlayerForFrame(main_frame_);
  CreateServiceForFrame(main_frame_);

  ASSERT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       OnlySubFrameProducesAudioAndServiceIsCreatedAfterwards) {
  StartPlayerForFrame(sub_frame_);
  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(services_[sub_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       BothFrameProducesAudioButOnlySubFrameHasService) {
  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(services_[sub_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest, PreferTopMostFrame) {
  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  ASSERT_EQ(services_[main_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       RoutedServiceUpdatedAfterRemovingPlayer) {
  StartPlayerForFrame(main_frame_);
  StartPlayerForFrame(sub_frame_);

  CreateServiceForFrame(main_frame_);
  CreateServiceForFrame(sub_frame_);

  ClearPlayersForFrame(main_frame_);

  ASSERT_EQ(services_[sub_frame_].get(), ComputeServiceForRouting());
}

TEST_F(MediaSessionImplServiceRoutingTest,
       DontNotifyMetadataAndActionsChangeWhenUncontrollable) {
  EXPECT_CALL(*mock_media_session_observer(), MediaSessionMetadataChanged(_))
      .Times(0);
  EXPECT_CALL(*mock_media_session_observer(), MediaSessionActionsChanged(_))
      .Times(0);

  CreateServiceForFrame(main_frame_);

  services_[main_frame_]->SetMetadata(MediaMetadata());
  services_[main_frame_]->EnableAction(blink::mojom::MediaSessionAction::PLAY);
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyMetadataAndActionsChangeWhenControllable) {
  MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");

  std::set<blink::mojom::MediaSessionAction> empty_actions;
  std::set<blink::mojom::MediaSessionAction> expected_actions;
  expected_actions.insert(blink::mojom::MediaSessionAction::PLAY);

  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionMetadataChanged(Eq(base::nullopt)))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionActionsChanged(Eq(empty_actions)))
      .Times(AnyNumber());

  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionMetadataChanged(Eq(expected_metadata)))
      .Times(1);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionActionsChanged(Eq(expected_actions)))
      .Times(1);

  CreateServiceForFrame(main_frame_);
  StartPlayerForFrame(main_frame_);

  services_[main_frame_]->SetMetadata(expected_metadata);
  services_[main_frame_]->EnableAction(blink::mojom::MediaSessionAction::PLAY);
}

TEST_F(MediaSessionImplServiceRoutingTest,
       NotifyMetadataAndActionsChangeWhenTurningControllable) {
  MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");

  std::set<blink::mojom::MediaSessionAction> expected_actions;
  expected_actions.insert(blink::mojom::MediaSessionAction::PLAY);

  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionMetadataChanged(Eq(expected_metadata)))
      .Times(1);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionActionsChanged(Eq(expected_actions)))
      .Times(1);

  CreateServiceForFrame(main_frame_);

  services_[main_frame_]->SetMetadata(expected_metadata);
  services_[main_frame_]->EnableAction(blink::mojom::MediaSessionAction::PLAY);

  StartPlayerForFrame(main_frame_);
}

TEST_F(MediaSessionImplServiceRoutingTest,
       DontNotifyMetadataAndActionsChangeWhenTurningUncontrollable) {
  MediaMetadata expected_metadata;
  expected_metadata.title = base::ASCIIToUTF16("title");
  expected_metadata.artist = base::ASCIIToUTF16("artist");
  expected_metadata.album = base::ASCIIToUTF16("album");

  std::set<blink::mojom::MediaSessionAction> empty_actions;
  std::set<blink::mojom::MediaSessionAction> expected_actions;
  expected_actions.insert(blink::mojom::MediaSessionAction::PLAY);

  EXPECT_CALL(*mock_media_session_observer(), MediaSessionMetadataChanged(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_media_session_observer(), MediaSessionActionsChanged(_))
      .Times(AnyNumber());
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionMetadataChanged(Eq(base::nullopt)))
      .Times(0);
  EXPECT_CALL(*mock_media_session_observer(),
              MediaSessionActionsChanged(Eq(empty_actions)))
      .Times(0);

  CreateServiceForFrame(main_frame_);

  services_[main_frame_]->SetMetadata(expected_metadata);
  services_[main_frame_]->EnableAction(blink::mojom::MediaSessionAction::PLAY);

  StartPlayerForFrame(main_frame_);
  ClearPlayersForFrame(main_frame_);
}

}  // namespace content
