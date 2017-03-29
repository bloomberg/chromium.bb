// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_resize_lock.h"

#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/test/fake_compositor_lock.h"

namespace content {
namespace {

class FakeCompositorResizeLockClient
    : public CompositorResizeLockClient,
      public NON_EXPORTED_BASE(ui::CompositorLockDelegate) {
 public:
  FakeCompositorResizeLockClient() : weak_ptr_factory_(this) {}

  std::unique_ptr<ui::CompositorLock> GetCompositorLock(
      ui::CompositorLockClient* client) override {
    created_ = true;
    compositor_lock_ =
        new ui::FakeCompositorLock(client, weak_ptr_factory_.GetWeakPtr());
    return base::WrapUnique(compositor_lock_);
  }

  // CompositorResizeLockClient implementation.
  void CompositorResizeLockEnded() override {
    // This informs when the CompositorResizeLock ends for the client to
    // release anything else it is holding.
    ended_ = true;
  }

  // CompositorLockDelegate implementation.
  void RemoveCompositorLock(ui::CompositorLock* lock) override {
    // This is where the ui::Compositor would be physically unlocked.
    unlocked_ = true;
  }

  void CauseTimeout() { compositor_lock_->TimeoutLock(); }

  bool created() const { return created_; }
  bool unlocked() const { return unlocked_; }
  bool ended() const { return ended_; }

 private:
  bool created_ = false;
  bool unlocked_ = false;
  bool ended_ = false;
  ui::FakeCompositorLock* compositor_lock_ = nullptr;
  base::WeakPtrFactory<FakeCompositorResizeLockClient> weak_ptr_factory_;
};

TEST(CompositorResizeLockTest, EndWithoutLock) {
  FakeCompositorResizeLockClient resize_client;
  gfx::Size resize_to(10, 11);

  {
    CompositorResizeLock resize_lock(&resize_client, resize_to);
    EXPECT_FALSE(resize_client.created());
    EXPECT_FALSE(resize_client.unlocked());
    EXPECT_FALSE(resize_client.ended());
  }
  // The compositor was never locked.
  EXPECT_FALSE(resize_client.unlocked());
  // The resize lock tells the client when it is destroyed.
  EXPECT_TRUE(resize_client.ended());
}

TEST(CompositorResizeLockTest, EndAfterLock) {
  FakeCompositorResizeLockClient resize_client;
  gfx::Size resize_to(10, 11);

  {
    CompositorResizeLock resize_lock(&resize_client, resize_to);
    EXPECT_FALSE(resize_client.created());
    EXPECT_FALSE(resize_client.ended());

    resize_lock.Lock();
    EXPECT_TRUE(resize_client.created());
    EXPECT_FALSE(resize_client.ended());
  }
  // The resize lock unlocks the compositor when it ends.
  EXPECT_TRUE(resize_client.unlocked());
  // The resize lock tells the client when it ends.
  EXPECT_TRUE(resize_client.ended());
}

TEST(CompositorResizeLockTest, EndAfterUnlock) {
  FakeCompositorResizeLockClient resize_client;
  gfx::Size resize_to(10, 11);

  {
    CompositorResizeLock resize_lock(&resize_client, resize_to);
    EXPECT_FALSE(resize_client.created());
    EXPECT_FALSE(resize_client.ended());

    resize_lock.Lock();
    EXPECT_TRUE(resize_client.created());
    EXPECT_FALSE(resize_client.ended());

    // Unlocking the compositor but keeping the resize lock.
    resize_lock.UnlockCompositor();
    EXPECT_TRUE(resize_client.unlocked());
    EXPECT_FALSE(resize_client.ended());
  }
  // The resize lock tells the client when it ends.
  EXPECT_TRUE(resize_client.ended());
}

TEST(CompositorResizeLockTest, EndAfterTimeout) {
  FakeCompositorResizeLockClient resize_client;
  gfx::Size resize_to(10, 11);

  {
    CompositorResizeLock resize_lock(&resize_client, resize_to);
    EXPECT_FALSE(resize_client.created());
    EXPECT_FALSE(resize_client.ended());

    resize_lock.Lock();
    EXPECT_TRUE(resize_client.created());
    EXPECT_FALSE(resize_client.ended());

    // A timeout tells the client that the lock ended.
    resize_client.CauseTimeout();
    EXPECT_TRUE(resize_client.unlocked());
    EXPECT_TRUE(resize_client.ended());
  }
}

}  // namespace
}  // namespace content
