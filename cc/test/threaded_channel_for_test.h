// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_THREADED_CHANNEL_FOR_TEST_H_
#define CC_TEST_THREADED_CHANNEL_FOR_TEST_H_

#include "base/macros.h"
#include "cc/test/test_hooks.h"
#include "cc/trees/threaded_channel.h"

namespace cc {
class ProxyImplForTest;

// ThreadedChannel that notifies |test_hooks| of internal actions by ProxyImpl.
class ThreadedChannelForTest : public ThreadedChannel {
 public:
  static scoped_ptr<ThreadedChannelForTest> Create(
      TestHooks* test_hooks,
      ProxyMain* proxy_main,
      TaskRunnerProvider* task_runner_provider);

  ProxyImplForTest* proxy_impl_for_test() { return proxy_impl_for_test_; }

 private:
  ThreadedChannelForTest(TestHooks* test_hooks,
                         ProxyMain* proxy_main,
                         TaskRunnerProvider* task_runner_provider);

  scoped_ptr<ProxyImpl> CreateProxyImpl(
      ChannelImpl* channel_impl,
      LayerTreeHost* layer_tree_host,
      TaskRunnerProvider* task_runner_provider,
      scoped_ptr<BeginFrameSource> external_begin_frame_source) override;

  TestHooks* test_hooks_;
  ProxyImplForTest* proxy_impl_for_test_;
};

}  // namespace cc

#endif  // CC_TEST_THREADED_CHANNEL_FOR_TEST_H_
