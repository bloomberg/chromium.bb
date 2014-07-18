// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_IMPL_PROXY_H_
#define CC_TEST_FAKE_IMPL_PROXY_H_

#include "cc/test/fake_proxy.h"
#include "cc/trees/single_thread_proxy.h"

namespace cc {

class FakeImplProxy : public FakeProxy {
 public:
  FakeImplProxy() : set_impl_thread_(this) {}

  explicit FakeImplProxy(
      scoped_refptr<base::SingleThreadTaskRunner> impl_task_runner)
      : FakeProxy(base::MessageLoopProxy::current(), impl_task_runner),
        set_impl_thread_(this) {}

 private:
  DebugScopedSetImplThread set_impl_thread_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_IMPL_PROXY_H_
