// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/remote_channel_impl_for_test.h"

#include "cc/test/proxy_impl_for_test.h"

namespace cc {

scoped_ptr<RemoteChannelImplForTest> RemoteChannelImplForTest::Create(
    TestHooks* test_hooks,
    LayerTreeHost* layer_tree_host,
    RemoteProtoChannel* remote_proto_channel,
    TaskRunnerProvider* task_runner_provider) {
  return make_scoped_ptr(new RemoteChannelImplForTest(
      test_hooks, layer_tree_host, remote_proto_channel, task_runner_provider));
}

RemoteChannelImplForTest::RemoteChannelImplForTest(
    TestHooks* test_hooks,
    LayerTreeHost* layer_tree_host,
    RemoteProtoChannel* remote_proto_channel,
    TaskRunnerProvider* task_runner_provider)
    : RemoteChannelImpl(layer_tree_host,
                        remote_proto_channel,
                        task_runner_provider),
      test_hooks_(test_hooks),
      proxy_impl_for_test_(nullptr) {}

scoped_ptr<ProxyImpl> RemoteChannelImplForTest::CreateProxyImpl(
    ChannelImpl* channel_impl,
    LayerTreeHost* layer_tree_host,
    TaskRunnerProvider* task_runner_provider,
    scoped_ptr<BeginFrameSource> external_begin_frame_source) {
  scoped_ptr<ProxyImplForTest> proxy_impl = ProxyImplForTest::Create(
      test_hooks_, channel_impl, layer_tree_host, task_runner_provider,
      std::move(external_begin_frame_source));
  proxy_impl_for_test_ = proxy_impl.get();
  return std::move(proxy_impl);
}

}  // namespace cc
