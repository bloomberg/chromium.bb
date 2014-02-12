// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_CONTEXT_PROVIDER_FACTORY_H_
#define CONTENT_TEST_TEST_CONTEXT_PROVIDER_FACTORY_H_

#include "base/memory/ref_counted.h"

namespace cc {
class ContextProvider;
}

namespace webkit {
namespace gpu {
class ContextProviderInProcess;
}
}

namespace content {

class TestContextProviderFactory {
 public:
  // The returned pointer is static and should not be deleted by the caller.
  static TestContextProviderFactory* GetInstance();

  scoped_refptr<cc::ContextProvider> OffscreenContextProviderForMainThread();

 private:
  TestContextProviderFactory();
  ~TestContextProviderFactory();

  scoped_refptr<webkit::gpu::ContextProviderInProcess> main_thread_;

  DISALLOW_COPY_AND_ASSIGN(TestContextProviderFactory);
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_CONTEXT_PROVIDER_FACTORY_H_
