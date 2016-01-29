// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_REMOTE_CHANNEL_IMPL_FOR_TEST_H_
#define CC_TEST_REMOTE_CHANNEL_IMPL_FOR_TEST_H_

#include "base/macros.h"
#include "cc/test/proxy_impl_for_test.h"
#include "cc/test/test_hooks.h"
#include "cc/trees/remote_channel_impl.h"

namespace cc {

class RemoteChannelImplForTest : public RemoteChannelImpl {
 public:
  static scoped_ptr<RemoteChannelImplForTest> Create(
      TestHooks* test_hooks,
      LayerTreeHost* layer_tree_host,
      RemoteProtoChannel* remote_proto_channel,
      TaskRunnerProvider* task_runner_provider);

  ProxyImplForTest* proxy_impl_for_test() const { return proxy_impl_for_test_; }

 private:
  RemoteChannelImplForTest(TestHooks* test_hooks,
                           LayerTreeHost* layer_tree_host,
                           RemoteProtoChannel* remote_proto_channel,
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

#endif  // CC_TEST_REMOTE_CHANNEL_IMPL_FOR_TEST_H_
