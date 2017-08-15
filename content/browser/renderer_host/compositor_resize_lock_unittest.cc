// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/compositor_resize_lock.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/test/fake_compositor_lock.h"

namespace content {
namespace {

class FakeCompositorResizeLockClient : public CompositorResizeLockClient,
                                       public ui::CompositorLockDelegate {
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

class CallbackClient : public FakeCompositorResizeLockClient {
 public:
  CallbackClient() = default;

  void CompositorResizeLockEnded() override {
    std::move(resize_lock_ended_).Run();
  }

  void set_resize_lock_ended(base::OnceClosure c) {
    resize_lock_ended_ = std::move(c);
  }

 private:
  base::OnceClosure resize_lock_ended_;
};

TEST(CompositorResizeLockTest, TimeoutSetBeforeClientTold) {
  CallbackClient resize_client;
  gfx::Size resize_to(10, 11);

  {
    CompositorResizeLock resize_lock(&resize_client, resize_to);
    resize_lock.Lock();

    // When the resize lock times out, it should report that before telling
    // the client about that.
    bool saw_resize_lock_end = false;
    auto resize_lock_ended = [](CompositorResizeLock* resize_lock, bool* saw) {
      EXPECT_TRUE(resize_lock->timed_out());
      *saw = true;
    };
    resize_client.set_resize_lock_ended(
        base::BindOnce(resize_lock_ended, &resize_lock, &saw_resize_lock_end));

    // A timeout tells the client that the lock ended.
    resize_client.CauseTimeout();
    EXPECT_TRUE(saw_resize_lock_end);
  }
}

}  // namespace
}  // namespace content
