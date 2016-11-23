// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/avda_codec_allocator.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
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

class MockTickClock : public base::TickClock {
 public:
  MockTickClock() {
    // Don't start with the null time.
    Advance(1000);
  }
  ~MockTickClock() override{};
  base::TimeTicks NowTicks() override {
    base::AutoLock auto_lock(lock_);
    return now_;
  }

  // Handy utility.
  void Advance(int msec) {
    base::AutoLock auto_lock(lock_);
    now_ += base::TimeDelta::FromMilliseconds(msec);
  }

 private:
  base::Lock lock_;
  base::TimeTicks now_;
};

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
  AVDACodecAllocatorTest() : allocator_thread_("AllocatorThread") {}
  ~AVDACodecAllocatorTest() override {}

 protected:
  void SetUp() override {
    // Start the main thread for the allocator.  This would normally be the GPU
    // main thread.
    ASSERT_TRUE(allocator_thread_.Start());

    // AVDACodecAllocator likes to post tasks to the current thread.

    test_information_.reset(new AVDACodecAllocator::TestInformation());
    test_information_->tick_clock_.reset(new MockTickClock());
    test_information_->stop_event_.reset(new base::WaitableEvent(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED));

    // Allocate the allocator on the appropriate thread.
    allocator_ = PostAndWait(
        FROM_HERE, base::Bind(
                       [](AVDACodecAllocator::TestInformation* test_info) {
                         return new AVDACodecAllocator(test_info);
                       },
                       test_information_.get()));
    allocator2_ = new AVDACodecAllocator(test_information_.get());

    // All threads should be stopped
    ASSERT_FALSE(IsThreadRunning(TaskType::AUTO_CODEC));
    ASSERT_FALSE(IsThreadRunning(TaskType::SW_CODEC));

    // Register an AVDA instance to start the allocator's threads.
    ASSERT_TRUE(StartThread(avda1_));

    // Assert that at least the AUTO_CODEC thread is started.  The other might
    // not be.
    ASSERT_TRUE(IsThreadRunning(TaskType::AUTO_CODEC));
    ASSERT_EQ(TaskType::AUTO_CODEC, TaskTypeForAllocation());
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
    test_information_->stop_event_->Wait();
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

  TaskType TaskTypeForAllocation() {
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

  // Test info that we provide to the codec allocator.
  std::unique_ptr<AVDACodecAllocator::TestInformation> test_information_;

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

TEST_F(AVDACodecAllocatorTest, TestMultiInstance) {
  // Add an avda instance.  This one must succeed immediately, since the last
  // one is still running.
  ASSERT_TRUE(StartThread(avda2_));

  // Stop the original avda instance.
  StopThread(avda1_);

  // Verify that the AUTO_CODEC thread is still running.
  ASSERT_TRUE(IsThreadRunning(TaskType::AUTO_CODEC));
  ASSERT_EQ(TaskType::AUTO_CODEC, TaskTypeForAllocation());

  // Remove the second instance and wait for it to stop.  Remember that it
  // stops after messages have been posted to the thread, so we don't know
  // how long it will take.
  StopThread(avda2_);

  // Verify that the threads have stopped.
  ASSERT_FALSE(IsThreadRunning(TaskType::AUTO_CODEC));
  ASSERT_FALSE(IsThreadRunning(TaskType::SW_CODEC));
}

TEST_F(AVDACodecAllocatorTest, TestHangThread) {
  ASSERT_EQ(TaskType::AUTO_CODEC, TaskTypeForAllocation());

  // Hang the AUTO_CODEC thread.
  base::WaitableEvent about_to_wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent wait_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  TaskRunnerFor(TaskType::AUTO_CODEC)
      ->PostTask(FROM_HERE, base::Bind(&WaitUntilRestarted,
                                       &about_to_wait_event, &wait_event));
  // Wait until the task starts, so that |allocator_| starts the hang timer.
  about_to_wait_event.Wait();

  // Verify that we've failed over after a long time has passed.
  static_cast<MockTickClock*>(test_information_->tick_clock_.get())
      ->Advance(1000);
  // Note that this should return the SW codec task type even if that thread
  // failed to start.  TaskRunnerFor() will return the current thread in that
  // case too.
  ASSERT_EQ(TaskType::SW_CODEC, TaskTypeForAllocation());

  // Un-hang the thread and wait for it to let another task run.  This will
  // notify |allocator_| that the thread is no longer hung.
  base::WaitableEvent done_waiting_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  TaskRunnerFor(TaskType::AUTO_CODEC)
      ->PostTask(FROM_HERE,
                 base::Bind(&SignalImmediately, &done_waiting_event));
  wait_event.Signal();
  done_waiting_event.Wait();

  // Verify that we've un-failed over.
  ASSERT_EQ(TaskType::AUTO_CODEC, TaskTypeForAllocation());
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
