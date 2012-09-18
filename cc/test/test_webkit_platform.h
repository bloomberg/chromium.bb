// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_WEBKIT_PLATFORM_H_
#define CC_TEST_TEST_WEBKIT_PLATFORM_H_

#include "base/rand_util.h"
#include "base/threading/thread_local_storage.h"
#include "cc/test/test_webkit_platform.h"
#include "third_party/WebKit/Source/Platform/chromium/public/Platform.h"
#include "webkit/compositor_bindings/web_compositor_support_impl.h"
#include "webkit/glue/webthread_impl.h"

using webkit_glue::WebThreadImplForMessageLoop;

namespace cc {

class TestWebKitPlatform : public WebKit::Platform {
 public:
  TestWebKitPlatform();
  virtual ~TestWebKitPlatform();

  virtual WebKit::WebCompositorSupport* compositorSupport();

  virtual void cryptographicallyRandomValues(
      unsigned char* buffer, size_t length);

  virtual WebKit::WebThread* createThread(const char* name);

  virtual WebKit::WebThread* currentThread();

  virtual double currentTime();

  virtual double monotonicallyIncreasingTime();

 private:
  base::ThreadLocalStorage::Slot current_thread_slot_;
  webkit::WebCompositorSupportImpl compositor_support_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_WEBKIT_PLATFORM_H_
