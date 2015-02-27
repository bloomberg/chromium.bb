// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <limits>

#include "base/run_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/browser/deferred_start_render_host.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/browser/load_monitoring_extension_host_queue.h"
#include "extensions/browser/serial_extension_host_queue.h"

namespace extensions {

namespace {

class StubDeferredStartRenderHost : public DeferredStartRenderHost {
 private:
  void AddDeferredStartRenderHostObserver(
      DeferredStartRenderHostObserver* observer) override {}
  void RemoveDeferredStartRenderHostObserver(
      DeferredStartRenderHostObserver* observer) override {}
  void CreateRenderViewNow() override {}
};

const size_t g_invalid_size_t = std::numeric_limits<size_t>::max();

}  // namespace

class LoadMonitoringExtensionHostQueueTest : public ExtensionsTest {
 public:
  LoadMonitoringExtensionHostQueueTest()
      : finished_(false),
        // Arbitrary choice of an invalid size_t.
        num_queued_(g_invalid_size_t),
        num_loaded_(g_invalid_size_t),
        max_in_queue_(g_invalid_size_t),
        max_active_loading_(g_invalid_size_t) {}

  void SetUp() override {
    queue_.reset(new LoadMonitoringExtensionHostQueue(
        // Use a SerialExtensionHostQueue because it's simple.
        scoped_ptr<ExtensionHostQueue>(new SerialExtensionHostQueue()),
        base::TimeDelta(),  // no delay, easier to test
        base::Bind(&LoadMonitoringExtensionHostQueueTest::Finished,
                   base::Unretained(this))));
  }

 protected:
  // Creates a new DeferredStartRenderHost. Ownership is held by this class,
  // not passed to caller.
  DeferredStartRenderHost* CreateHost() {
    StubDeferredStartRenderHost* stub = new StubDeferredStartRenderHost();
    stubs_.push_back(stub);
    return stub;
  }

  // Our single LoadMonitoringExtensionHostQueue instance.
  LoadMonitoringExtensionHostQueue* queue() { return queue_.get(); }

  // Returns true if the queue has finished monitoring.
  bool finished() { return finished_; }

  // These are available after the queue has finished (in which case finished()
  // will return true).
  size_t num_queued() { return num_queued_; }
  size_t num_loaded() { return num_loaded_; }
  size_t max_in_queue() { return max_in_queue_; }
  size_t max_active_loading() { return max_active_loading_; }

 private:
  // Callback when queue has finished monitoring.
  void Finished(size_t num_queued,
                size_t num_loaded,
                size_t max_in_queue,
                size_t max_active_loading) {
    finished_ = true;
    num_queued_ = num_queued;
    num_loaded_ = num_loaded;
    max_in_queue_ = max_in_queue;
    max_active_loading_ = max_active_loading;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<LoadMonitoringExtensionHostQueue> queue_;
  ScopedVector<StubDeferredStartRenderHost> stubs_;

  // Set after the queue has finished monitoring.
  bool finished_;
  size_t num_queued_;
  size_t num_loaded_;
  size_t max_in_queue_;
  size_t max_active_loading_;
};

// Tests that if monitoring is never started, nor any hosts added, nothing is
// recorded.
TEST_F(LoadMonitoringExtensionHostQueueTest, NeverStarted) {
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(finished());
}

// Tests that if monitoring has started but no hosts added, it's recorded as 0.
TEST_F(LoadMonitoringExtensionHostQueueTest, NoHosts) {
  queue()->StartMonitoring();

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(finished());
  EXPECT_EQ(0u, num_queued());
  EXPECT_EQ(0u, num_loaded());
  EXPECT_EQ(0u, max_in_queue());
  EXPECT_EQ(0u, max_active_loading());
}

// Tests that adding a host starts monitoring.
TEST_F(LoadMonitoringExtensionHostQueueTest, AddOneHost) {
  queue()->Add(CreateHost());

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(finished());
  EXPECT_EQ(1u, num_queued());
  EXPECT_EQ(0u, num_loaded());
  EXPECT_EQ(1u, max_in_queue());
  EXPECT_EQ(0u, max_active_loading());
}

// Tests that a host added and removed is still recorded, but not as a load
// finished.
TEST_F(LoadMonitoringExtensionHostQueueTest, AddAndRemoveOneHost) {
  DeferredStartRenderHost* host = CreateHost();
  queue()->Add(host);
  queue()->Remove(host);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(finished());
  EXPECT_EQ(1u, num_queued());
  EXPECT_EQ(0u, num_loaded());
  EXPECT_EQ(1u, max_in_queue());
  EXPECT_EQ(0u, max_active_loading());
}

// Tests adding and starting a single host.
TEST_F(LoadMonitoringExtensionHostQueueTest, AddAndStartOneHost) {
  DeferredStartRenderHost* host = CreateHost();
  queue()->Add(host);
  queue()->OnDeferredStartRenderHostDidStartLoading(host);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(finished());
  EXPECT_EQ(1u, num_queued());
  EXPECT_EQ(0u, num_loaded());
  EXPECT_EQ(1u, max_in_queue());
  EXPECT_EQ(1u, max_active_loading());

  // Sanity check: stopping/destroying at this point doesn't crash.
  queue()->OnDeferredStartRenderHostDidStopLoading(host);
  queue()->OnDeferredStartRenderHostDestroyed(host);
}

// Tests adding and destroying a single host without starting it.
TEST_F(LoadMonitoringExtensionHostQueueTest, AddAndDestroyOneHost) {
  DeferredStartRenderHost* host = CreateHost();
  queue()->Add(host);
  queue()->OnDeferredStartRenderHostDestroyed(host);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(finished());
  EXPECT_EQ(1u, num_queued());
  EXPECT_EQ(0u, num_loaded());
  EXPECT_EQ(1u, max_in_queue());
  EXPECT_EQ(0u, max_active_loading());

  // Sanity check: stopping/destroying at this point doesn't crash.
  queue()->OnDeferredStartRenderHostDidStopLoading(host);
  queue()->OnDeferredStartRenderHostDestroyed(host);
}

// Tests adding, starting, and stopping a single host.
TEST_F(LoadMonitoringExtensionHostQueueTest, AddAndStartAndStopOneHost) {
  DeferredStartRenderHost* host = CreateHost();
  queue()->Add(host);
  queue()->OnDeferredStartRenderHostDidStartLoading(host);
  queue()->OnDeferredStartRenderHostDidStopLoading(host);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(finished());
  EXPECT_EQ(1u, num_queued());
  EXPECT_EQ(1u, num_loaded());
  EXPECT_EQ(1u, max_in_queue());
  EXPECT_EQ(1u, max_active_loading());

  // Sanity check: destroying at this point doesn't crash.
  queue()->OnDeferredStartRenderHostDestroyed(host);
}

// Tests adding, starting, and destroying (i.e. an implicit stop) a single host.
TEST_F(LoadMonitoringExtensionHostQueueTest, AddAndStartAndDestroyOneHost) {
  DeferredStartRenderHost* host = CreateHost();
  queue()->Add(host);
  queue()->OnDeferredStartRenderHostDidStartLoading(host);
  queue()->OnDeferredStartRenderHostDestroyed(host);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(finished());
  EXPECT_EQ(1u, num_queued());
  EXPECT_EQ(1u, num_loaded());
  EXPECT_EQ(1u, max_in_queue());
  EXPECT_EQ(1u, max_active_loading());
}

// Tests adding, starting, and removing a single host.
TEST_F(LoadMonitoringExtensionHostQueueTest, AddAndStartAndRemoveOneHost) {
  DeferredStartRenderHost* host = CreateHost();
  queue()->Add(host);
  queue()->OnDeferredStartRenderHostDidStartLoading(host);
  queue()->Remove(host);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(finished());
  EXPECT_EQ(1u, num_queued());
  EXPECT_EQ(0u, num_loaded());
  EXPECT_EQ(1u, max_in_queue());
  EXPECT_EQ(1u, max_active_loading());
}

// Tests monitoring a sequence of hosts.
TEST_F(LoadMonitoringExtensionHostQueueTest, Sequence) {
  // Scenario:
  //
  // 6 hosts will be added, only 5 will start loading, with a maximum of 4 in
  // the queue and 3 loading at any time. Only 2 will finish.
  DeferredStartRenderHost* host1 = CreateHost();
  DeferredStartRenderHost* host2 = CreateHost();
  DeferredStartRenderHost* host3 = CreateHost();
  DeferredStartRenderHost* host4 = CreateHost();
  DeferredStartRenderHost* host5 = CreateHost();
  DeferredStartRenderHost* host6 = CreateHost();

  queue()->Add(host1);
  queue()->Add(host2);
  queue()->Add(host3);

  queue()->OnDeferredStartRenderHostDidStartLoading(host1);
  queue()->OnDeferredStartRenderHostDidStartLoading(host2);
  queue()->OnDeferredStartRenderHostDidStopLoading(host1);

  queue()->Add(host4);
  queue()->Add(host5);
  queue()->Add(host6);

  queue()->OnDeferredStartRenderHostDidStartLoading(host3);
  queue()->OnDeferredStartRenderHostDidStartLoading(host4);
  queue()->OnDeferredStartRenderHostDidStopLoading(host4);
  queue()->OnDeferredStartRenderHostDidStartLoading(host5);

  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(finished());
  EXPECT_EQ(6u, num_queued());
  EXPECT_EQ(2u, num_loaded());
  EXPECT_EQ(4u, max_in_queue());
  EXPECT_EQ(3u, max_active_loading());

  // Sanity check: complete a realistic sequence by stopping and/or destroying
  // all of the hosts. It shouldn't crash.
  queue()->OnDeferredStartRenderHostDestroyed(host1);
  queue()->OnDeferredStartRenderHostDidStopLoading(host2);
  queue()->OnDeferredStartRenderHostDestroyed(host2);
  queue()->OnDeferredStartRenderHostDidStopLoading(host3);
  queue()->OnDeferredStartRenderHostDestroyed(host3);
  queue()->OnDeferredStartRenderHostDestroyed(host4);
  queue()->OnDeferredStartRenderHostDestroyed(host5);  // never stopped
  queue()->OnDeferredStartRenderHostDestroyed(host6);  // never started/stopped
}

}  // namespace extensions
