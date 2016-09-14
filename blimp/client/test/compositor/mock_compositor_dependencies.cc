// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/test/compositor/mock_compositor_dependencies.h"

#include "cc/test/test_in_process_context_provider.h"

namespace blimp {
namespace client {

void MockCompositorDependencies::GetContextProviders(
    const ContextProviderCallback& callback) {
  callback.Run(
      make_scoped_refptr(new cc::TestInProcessContextProvider(nullptr)),
      make_scoped_refptr(new cc::TestInProcessContextProvider(nullptr)));
}

}  // namespace client
}  // namespace blimp
