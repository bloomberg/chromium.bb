// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_media_blocker.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace shell {

using ::testing::_;
using ::testing::Invoke;

class MockMediaSession : public content::MediaSession {
 public:
  explicit MockMediaSession(content::MediaSession* session) {
    // Delegate the calls to the real MediaSession.
    ON_CALL(*this, Resume(_))
        .WillByDefault(Invoke(session, &MediaSession::Resume));
    ON_CALL(*this, Suspend(_))
        .WillByDefault(Invoke(session, &MediaSession::Suspend));
    ON_CALL(*this, Stop(_)).WillByDefault(Invoke(session, &MediaSession::Stop));
    ON_CALL(*this, DidReceiveAction(_))
        .WillByDefault(Invoke(session, &MediaSession::DidReceiveAction));
  }
  ~MockMediaSession() {}

  MOCK_METHOD1(Resume, void(content::MediaSession::SuspendType));
  MOCK_METHOD1(Suspend, void(content::MediaSession::SuspendType));
  MOCK_METHOD1(Stop, void(content::MediaSession::SuspendType));
  MOCK_METHOD1(DidReceiveAction, void(blink::mojom::MediaSessionAction));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockMediaSession);
};

class CastMediaBlockerTest : public content::RenderViewHostTestHarness {
 public:
  CastMediaBlockerTest() {}
  ~CastMediaBlockerTest() override {}

  void SetUp() override {
    initializer_ = base::MakeUnique<content::TestContentClientInitializer>();
    content::RenderViewHostTestHarness::SetUp();
    media_session_ = base::MakeUnique<MockMediaSession>(
        content::MediaSession::Get(web_contents()));
    media_blocker_ = base::MakeUnique<CastMediaBlocker>(media_session_.get());

    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("https://www.youtube.com"));
  }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }

  void MediaSessionChanged(bool controllable, bool suspended) {
    media_blocker_->MediaSessionStateChanged(controllable, suspended);
  }

 protected:
  std::unique_ptr<content::TestContentClientInitializer> initializer_;
  std::unique_ptr<MockMediaSession> media_session_;
  std::unique_ptr<CastMediaBlocker> media_blocker_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CastMediaBlockerTest);
};

// TODO(derekjchow): Make the tests pass on cast testbots.
// crbug.com/665118

TEST_F(CastMediaBlockerTest, DISABLED_Block_Unblock_Suspended) {
  // Testing block/unblock operations do nothing if media never plays.
  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  media_blocker_->BlockMediaLoading(true);
  media_blocker_->BlockMediaLoading(false);

  MediaSessionChanged(true, true);
  media_blocker_->BlockMediaLoading(true);
  media_blocker_->BlockMediaLoading(false);

  media_blocker_->BlockMediaLoading(true);
  MediaSessionChanged(false, true);
  media_blocker_->BlockMediaLoading(false);
}

TEST_F(CastMediaBlockerTest, DISABLED_No_Block) {
  // Tests CastMediaBlocker does nothing if block/unblock is not called.
  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);

  // Media becomes controllable/uncontrollable.
  MediaSessionChanged(true, true);
  MediaSessionChanged(false, true);

  // Media starts and stops.
  MediaSessionChanged(false, false);
  MediaSessionChanged(false, true);

  // Media starts, changes controllability and stops.
  MediaSessionChanged(false, false);
  MediaSessionChanged(true, false);
  MediaSessionChanged(false, false);
  MediaSessionChanged(false, true);

  // Media starts, changes controllability and stops.
  MediaSessionChanged(false, false);
  MediaSessionChanged(true, false);
  MediaSessionChanged(true, true);
}

TEST_F(CastMediaBlockerTest, DISABLED_Block_Before_Controllable) {
  // Tests CastMediaBlocker only suspends when controllable.
  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  media_blocker_->BlockMediaLoading(true);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());

  // Session becomes controllable
  EXPECT_CALL(*media_session_, Suspend(_)).Times(1);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(true, false);
}

TEST_F(CastMediaBlockerTest, DISABLED_Block_After_Controllable) {
  // Tests CastMediaBlocker suspends immediately on block if controllable.
  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(true, false);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());

  // Block when media is playing
  EXPECT_CALL(*media_session_, Suspend(_)).Times(1);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  media_blocker_->BlockMediaLoading(true);
  MediaSessionChanged(true, true);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());

  // Unblock
  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(1);
  media_blocker_->BlockMediaLoading(false);
}

TEST_F(CastMediaBlockerTest, DISABLED_Block_Multiple) {
  // Tests CastMediaBlocker repeatively suspends when blocked.
  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  media_blocker_->BlockMediaLoading(true);
  MediaSessionChanged(false, false);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());

  EXPECT_CALL(*media_session_, Suspend(_)).Times(1);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(true, false);
  MediaSessionChanged(true, true);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());

  EXPECT_CALL(*media_session_, Suspend(_)).Times(1);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(true, false);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());
  MediaSessionChanged(true, true);

  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(false, true);
  MediaSessionChanged(false, false);
  MediaSessionChanged(false, true);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());
}

TEST_F(CastMediaBlockerTest, DISABLED_Block_Unblock_Uncontrollable) {
  // Tests CastMediaBlocker does not suspend or resume when uncontrollable.
  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  media_blocker_->BlockMediaLoading(true);
  MediaSessionChanged(false, false);
  media_blocker_->BlockMediaLoading(false);
  media_blocker_->BlockMediaLoading(true);
  MediaSessionChanged(false, true);
  media_blocker_->BlockMediaLoading(false);
  media_blocker_->BlockMediaLoading(true);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());
}

TEST_F(CastMediaBlockerTest, DISABLED_Block_Unblock_Uncontrollable2) {
  EXPECT_CALL(*media_session_, Suspend(_)).Times(1);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(true, true);
  media_blocker_->BlockMediaLoading(true);
  MediaSessionChanged(false, true);
  MediaSessionChanged(true, true);
  MediaSessionChanged(true, false);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());

  EXPECT_CALL(*media_session_, Suspend(_)).Times(1);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(false, false);
  MediaSessionChanged(false, true);
  MediaSessionChanged(true, true);
  MediaSessionChanged(true, false);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());

  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  media_blocker_->BlockMediaLoading(false);
}

TEST_F(CastMediaBlockerTest, DISABLED_Resume_When_Controllable) {
  // Tests CastMediaBlocker will only resume after unblock when controllable.
  EXPECT_CALL(*media_session_, Suspend(_)).Times(1);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(true, false);
  media_blocker_->BlockMediaLoading(true);
  MediaSessionChanged(true, true);
  MediaSessionChanged(false, true);
  media_blocker_->BlockMediaLoading(false);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());

  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(1);
  MediaSessionChanged(true, true);
}

TEST_F(CastMediaBlockerTest, DISABLED_No_Resume) {
  // Tests CastMediaBlocker will not resume if media starts playing by itself
  // after unblock.
  EXPECT_CALL(*media_session_, Suspend(_)).Times(1);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(true, false);
  media_blocker_->BlockMediaLoading(true);
  MediaSessionChanged(true, true);
  MediaSessionChanged(false, true);
  media_blocker_->BlockMediaLoading(false);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());

  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(false, false);
}

TEST_F(CastMediaBlockerTest, DISABLED_Block_Before_Resume) {
  // Tests CastMediaBlocker does not resume if blocked again after an unblock.
  EXPECT_CALL(*media_session_, Suspend(_)).Times(1);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(true, false);
  media_blocker_->BlockMediaLoading(true);
  MediaSessionChanged(true, true);
  MediaSessionChanged(false, true);
  media_blocker_->BlockMediaLoading(false);
  testing::Mock::VerifyAndClearExpectations(media_session_.get());

  EXPECT_CALL(*media_session_, Suspend(_)).Times(0);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  media_blocker_->BlockMediaLoading(true);
  MediaSessionChanged(true, true);
}

TEST_F(CastMediaBlockerTest, DISABLED_Unblocked_Already_Playing) {
  // Tests CastMediaBlocker does not resume if unblocked and media is playing.
  EXPECT_CALL(*media_session_, Suspend(_)).Times(1);
  EXPECT_CALL(*media_session_, Resume(_)).Times(0);
  MediaSessionChanged(true, false);
  media_blocker_->BlockMediaLoading(true);
  media_blocker_->BlockMediaLoading(false);
}

}  // namespace shell
}  // namespace chromecast
