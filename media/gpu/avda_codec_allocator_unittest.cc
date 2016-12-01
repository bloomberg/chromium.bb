// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_codec_allocator.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/tick_clock.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::NiceMock;
using testing::_;

namespace media {
namespace {
template <typename ReturnType>
void RunAndSignalTask(base::WaitableEvent* event,
                      ReturnType* return_value,
                      const base::Callback<ReturnType(void)>& cb) {
  *return_value = cb.Run();
  event->Signal();
}

void WaitUntilRestarted(base::WaitableEvent* about_to_wait_event,
                        base::WaitableEvent* wait_event) {
  // Notify somebody that we've started.
  if (about_to_wait_event)
    about_to_wait_event->Signal();
  wait_event->Wait();
}

void SignalImmediately(base::WaitableEvent* event) {
  event->Signal();
}
}

class MockClient : public AVDACodecAllocatorClient {
 public:
  MOCK_METHOD1(OnSurfaceAvailable, void(bool success));
  MOCK_METHOD0(OnSurfaceDestroyed, void());

  // Gmock doesn't let us mock methods taking move-only types.
  MOCK_METHOD1(OnCodecConfiguredMock, void(VideoCodecBridge* media_codec));
  void OnCodecConfigured(
      std::unique_ptr<VideoCodecBridge> media_codec) override {
    OnCodecConfiguredMock(media_codec.get());
  }
};

class AVDACodecAllocatorTest : public testing::Test {
 public:
  AVDACodecAllocatorTest()
      : allocator_thread_("AllocatorThread"),
        stop_event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                    base::WaitableEvent::InitialState::NOT_SIGNALED) {
    // Don't start the clock at null.
    tick_clock_.Advance(base::TimeDelta::FromSeconds(1));
  }

  ~AVDACodecAllocatorTest() override {}

 protected:
  void SetUp() override {
    // Start the main thread for the allocator.  This would normally be the GPU
    // main thread.
    ASSERT_TRUE(allocator_thread_.Start());

    // Create the first allocator on the allocator thread.
    allocator_ = PostAndWait(
        FROM_HERE, base::Bind(
                       [](base::TickClock* clock, base::WaitableEvent* event) {
                         return new AVDACodecAllocator(clock, event);
                       },
                       &tick_clock_, &stop_event_));
    allocator2_ = new AVDACodecAllocator();
  }

  void TearDown() override {
    // Don't leave any threads hung, or this will hang too.
    // It would be nice if we could let a unique ptr handle this, but the
    // destructor is private.  We also have to destroy it on the right thread.
    PostAndWait(FROM_HERE, base::Bind(
                               [](AVDACodecAllocator* allocator) {
                                 delete allocator;
                                 return true;
                               },
                               allocator_));

    allocator_thread_.Stop();
    delete allocator2_;
  }

 protected:

  // Start / stop the threads for |avda| on the right thread.
  bool StartThread(AVDACodecAllocatorClient* avda) {
    return PostAndWait(FROM_HERE, base::Bind(
                                      [](AVDACodecAllocator* allocator,
                                         AVDACodecAllocatorClient* avda) {
                                        return allocator->StartThread(avda);
                                      },
                                      allocator_, avda));
  }

  void StopThread(AVDACodecAllocatorClient* avda) {
    // Note that we also wait for the stop event, so that we know that the
    // stop has completed.  It's async with respect to the allocator thread.
    PostAndWait(FROM_HERE, base::Bind(
                               [](AVDACodecAllocator* allocator,
                                  AVDACodecAllocatorClient* avda) {
                                 allocator->StopThread(avda);
                                 return true;
                               },
                               allocator_, avda));
    // Note that we don't do this on the allocator thread, since that's the
    // thread that will signal it.
    stop_event_.Wait();
  }

  // Return the running state of |task_type|, doing the necessary thread hops.
  bool IsThreadRunning(TaskType task_type) {
    return PostAndWait(
        FROM_HERE,
        base::Bind(
            [](AVDACodecAllocator* allocator, TaskType task_type) {
              return allocator->GetThreadForTesting(task_type).IsRunning();
            },
            allocator_, task_type));
  }

  base::Optional<TaskType> TaskTypeForAllocation() {
    return PostAndWait(FROM_HERE,
                       base::Bind(&AVDACodecAllocator::TaskTypeForAllocation,
                                  base::Unretained(allocator_)));
  }

  scoped_refptr<base::SingleThreadTaskRunner> TaskRunnerFor(
      TaskType task_type) {
    return PostAndWait(FROM_HERE,
                       base::Bind(&AVDACodecAllocator::TaskRunnerFor,
                                  base::Unretained(allocator_), task_type));
  }

  // Post |cb| to the allocator thread, and wait for a response.  Note that we
  // don't have a specialization for void, and void won't work as written.  So,
  // be sure to return something.
  template <typename ReturnType>
  ReturnType PostAndWait(const tracked_objects::Location& from_here,
                         const base::Callback<ReturnType(void)>& cb) {
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    ReturnType return_value = ReturnType();
    allocator_thread_.task_runner()->PostTask(
        from_here,
        base::Bind(&RunAndSignalTask<ReturnType>, &event, &return_value, cb));
    event.Wait();
    return return_value;
  }

  base::Thread allocator_thread_;

  // The test params for |allocator_|.
  base::SimpleTestTickClock tick_clock_;
  base::WaitableEvent stop_event_;

  // Allocators that we own. The first is intialized to be used on the allocator
  // thread and the second one is initialized on the test thread. Each test
  // should only be using one of the two. They are not unique_ptrs because the
  // destructor is private and they need to be destructed on the right thread.
  AVDACodecAllocator* allocator_ = nullptr;
  AVDACodecAllocator* allocator2_ = nullptr;

  NiceMock<MockClient> client1_, client2_, client3_;
  NiceMock<MockClient>* avda1_ = &client1_;
  NiceMock<MockClient>* avda2_ = &client2_;
  NiceMock<MockClient>* avda3_ = &client3_;
};

TEST_F(AVDACodecAllocatorTest, ThreadsStartWhenClientsStart) {
  ASSERT_FALSE(IsThreadRunning(AUTO_CODEC));
  ASSERT_FALSE(IsThreadRunning(SW_CODEC));
  StartThread(avda1_);
  ASSERT_TRUE(IsThreadRunning(AUTO_CODEC));
  ASSERT_TRUE(IsThreadRunning(SW_CODEC));
}

TEST_F(AVDACodecAllocatorTest, ThreadsStopAfterAllClientsStop) {
  StartThread(avda1_);
  StartThread(avda2_);
  StopThread(avda1_);
  ASSERT_TRUE(IsThreadRunning(AUTO_CODEC));
  StopThread(avda2_);
  ASSERT_FALSE(IsThreadRunning(AUTO_CODEC));
  ASSERT_FALSE(IsThreadRunning(SW_CODEC));
}

TEST_F(AVDACodecAllocatorTest, TestHangThread) {
  StartThread(avda1_);
  ASSERT_EQ(AUTO_CODEC, TaskTypeForAllocation().value());

  // Hang the AUTO_CODEC thread.
  base::WaitableEvent about_to_wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  TaskRunnerFor(AUTO_CODEC)
      ->PostTask(FROM_HERE, base::Bind(&WaitUntilRestarted,
                                       &about_to_wait_event, &wait_event));
  // Wait until the task starts, so that |allocator_| starts the hang timer.
  about_to_wait_event.Wait();

  // Verify that we've failed over after a long time has passed.
  tick_clock_.Advance(base::TimeDelta::FromSeconds(1));
  ASSERT_EQ(SW_CODEC, TaskTypeForAllocation().value());

  // Un-hang the thread and wait for it to let another task run.  This will
  // notify |allocator_| that the thread is no longer hung.
  base::WaitableEvent done_waiting_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  TaskRunnerFor(AUTO_CODEC)
      ->PostTask(FROM_HERE,
                 base::Bind(&SignalImmediately, &done_waiting_event));
  wait_event.Signal();
  done_waiting_event.Wait();

  // Verify that we've un-failed over.
  ASSERT_EQ(AUTO_CODEC, TaskTypeForAllocation().value());
}

TEST_F(AVDACodecAllocatorTest, AllocatingASurfaceTextureAlwaysSucceeds) {
  ASSERT_TRUE(
      allocator2_->AllocateSurface(avda1_, SurfaceManager::kNoSurfaceID));
  ASSERT_TRUE(
      allocator2_->AllocateSurface(avda2_, SurfaceManager::kNoSurfaceID));
}

TEST_F(AVDACodecAllocatorTest, AllocatingAnOwnedSurfaceFails) {
  ASSERT_TRUE(allocator2_->AllocateSurface(avda1_, 1));
  ASSERT_FALSE(allocator2_->AllocateSurface(avda2_, 1));
}

TEST_F(AVDACodecAllocatorTest, LaterWaitersReplaceEarlierWaiters) {
  allocator2_->AllocateSurface(avda1_, 1);
  allocator2_->AllocateSurface(avda2_, 1);
  EXPECT_CALL(*avda2_, OnSurfaceAvailable(false));
  allocator2_->AllocateSurface(avda3_, 1);
}

TEST_F(AVDACodecAllocatorTest, WaitersBecomeOwnersWhenSurfacesAreReleased) {
  allocator2_->AllocateSurface(avda1_, 1);
  allocator2_->AllocateSurface(avda2_, 1);
  EXPECT_CALL(*avda2_, OnSurfaceAvailable(true));
  allocator2_->DeallocateSurface(avda1_, 1);
  // The surface should still be owned.
  ASSERT_FALSE(allocator2_->AllocateSurface(avda1_, 1));
}

TEST_F(AVDACodecAllocatorTest, DeallocatingUnownedSurfacesIsSafe) {
  allocator2_->DeallocateSurface(avda1_, 1);
  allocator2_->DeallocateSurface(avda1_, SurfaceManager::kNoSurfaceID);
}

TEST_F(AVDACodecAllocatorTest, WaitersAreRemovedIfTheyDeallocate) {
  allocator2_->AllocateSurface(avda1_, 1);
  allocator2_->AllocateSurface(avda2_, 1);
  allocator2_->DeallocateSurface(avda2_, 1);
  // avda2_ should should not receive a notification.
  EXPECT_CALL(*avda2_, OnSurfaceAvailable(_)).Times(0);
  allocator2_->DeallocateSurface(avda1_, 1);
}

TEST_F(AVDACodecAllocatorTest, OwnersAreNotifiedOnDestruction) {
  allocator2_->AllocateSurface(avda1_, 1);
  // Not notified for a surface it doesn't own.
  EXPECT_CALL(*avda1_, OnSurfaceDestroyed()).Times(0);
  allocator2_->OnSurfaceDestroyed(123);
  // But notified for a surface it does own.
  EXPECT_CALL(*avda1_, OnSurfaceDestroyed());
  allocator2_->OnSurfaceDestroyed(1);
}

TEST_F(AVDACodecAllocatorTest, WaitersAreNotifiedOnDestruction) {
  allocator2_->AllocateSurface(avda1_, 1);
  allocator2_->AllocateSurface(avda2_, 1);
  EXPECT_CALL(*avda2_, OnSurfaceAvailable(false));
  allocator2_->OnSurfaceDestroyed(1);
}

TEST_F(AVDACodecAllocatorTest, DeallocatingIsSafeDuringSurfaceDestroyed) {
  allocator2_->AllocateSurface(avda1_, 1);
  EXPECT_CALL(*avda1_, OnSurfaceDestroyed()).WillOnce(Invoke([=]() {
    allocator2_->DeallocateSurface(avda1_, 1);
  }));
  allocator2_->OnSurfaceDestroyed(1);
}

}  // namespace media
