// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/threaded_channel_for_test.h"

#include "cc/test/proxy_impl_for_test.h"

namespace cc {

scoped_ptr<ThreadedChannel> ThreadedChannelForTest::Create(
    TestHooks* test_hooks,
    ProxyMain* proxy_main,
    TaskRunnerProvider* task_runner_provider) {
  return make_scoped_ptr(
      new ThreadedChannelForTest(test_hooks, proxy_main, task_runner_provider));
}

ThreadedChannelForTest::ThreadedChannelForTest(
    TestHooks* test_hooks,
    ProxyMain* proxy_main,
    TaskRunnerProvider* task_runner_provider)
    : ThreadedChannel(proxy_main, task_runner_provider),
      test_hooks_(test_hooks) {}

scoped_ptr<ProxyImpl> ThreadedChannelForTest::CreateProxyImpl(
    ChannelImpl* channel_impl,
    LayerTreeHost* layer_tree_host,
    TaskRunnerProvider* task_runner_provider,
    scoped_ptr<BeginFrameSource> external_begin_frame_source) {
  return ProxyImplForTest::Create(test_hooks_, channel_impl, layer_tree_host,
                                  task_runner_provider,
                                  std::move(external_begin_frame_source));
}

}  // namespace cc
