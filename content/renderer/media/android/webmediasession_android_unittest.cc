// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/webmediasession_android.h"

#include "base/memory/scoped_ptr.h"
#include "content/renderer/media/android/renderer_media_session_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class WebMediaSessionTest : public testing::Test {
 public:
  bool SessionManagerHasSession(RendererMediaSessionManager* session_manager,
                                WebMediaSessionAndroid* session) {
    for (auto& iter : session_manager->sessions_) {
      if (iter.second == session)
        return true;
    }
    return false;
  }

  bool IsSessionManagerEmpty(RendererMediaSessionManager* session_manager) {
    return session_manager->sessions_.empty();
  }
};

TEST_F(WebMediaSessionTest, TestRegistration) {
  scoped_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));
  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));
  {
    scoped_ptr<WebMediaSessionAndroid> session(
        new WebMediaSessionAndroid(session_manager.get()));
    EXPECT_TRUE(SessionManagerHasSession(session_manager.get(), session.get()));
  }
  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));
}

TEST_F(WebMediaSessionTest, TestMultipleRegistration) {
  scoped_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));
  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));

  {
    scoped_ptr<WebMediaSessionAndroid> session1(
        new WebMediaSessionAndroid(session_manager.get()));
    EXPECT_TRUE(
        SessionManagerHasSession(session_manager.get(), session1.get()));

    {
      scoped_ptr<WebMediaSessionAndroid> session2(
          new WebMediaSessionAndroid(session_manager.get()));
      EXPECT_TRUE(
          SessionManagerHasSession(session_manager.get(), session2.get()));
    }

    EXPECT_TRUE(
        SessionManagerHasSession(session_manager.get(), session1.get()));
  }

  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));
}

TEST_F(WebMediaSessionTest, TestMultipleRegistrationOutOfOrder) {
  scoped_ptr<RendererMediaSessionManager> session_manager(
      new RendererMediaSessionManager(nullptr));
  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));

  WebMediaSessionAndroid* session1 =
      new WebMediaSessionAndroid(session_manager.get());
  EXPECT_TRUE(SessionManagerHasSession(session_manager.get(), session1));

  WebMediaSessionAndroid* session2 =
      new WebMediaSessionAndroid(session_manager.get());
  EXPECT_TRUE(SessionManagerHasSession(session_manager.get(), session2));

  delete session1;
  EXPECT_TRUE(SessionManagerHasSession(session_manager.get(), session2));

  delete session2;
  EXPECT_TRUE(IsSessionManagerEmpty(session_manager.get()));
}

}  // namespace content
