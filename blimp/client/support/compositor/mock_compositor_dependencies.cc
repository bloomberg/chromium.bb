// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/support/compositor/mock_compositor_dependencies.h"

#include "cc/test/test_in_process_context_provider.h"

namespace blimp {
namespace client {

void MockCompositorDependencies::GetContextProvider(
    const ContextProviderCallback& callback) {
  scoped_refptr<cc::ContextProvider> provider =
      make_scoped_refptr(new cc::TestInProcessContextProvider(nullptr));
  callback.Run(provider);
}

}  // namespace client
}  // namespace blimp
