// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_H_
#define BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_H_

#include <memory>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "blimp/client/public/blimp_client_context_delegate.h"
#include "blimp/client/public/contents/blimp_contents.h"
#include "components/keyed_service/core/keyed_service.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif  // defined(OS_ANDROID)

namespace base {
class SupportsUserData;
}

namespace blimp {
namespace client {

// BlimpClientContext is the core class for the Blimp client. It provides hooks
// for creating BlimpContents and other features that are per
// BrowserContext/Profile.
class BlimpClientContext : public KeyedService {
 public:
#if defined(OS_ANDROID)
  // Returns a Java object of the type BlimpClientContext for the given
  // BlimpClientContext.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      BlimpClientContext* blimp_client_context);
#endif  // defined(OS_ANDROID)

  // Creates a BlimpClientContext. The implementation of this function
  // depends on whether the core or dummy implementation of Blimp has been
  // linked in.
  // The |io_thread_task_runner| must be the task runner to use for IO
  // operations.
  // The |file_thread_task_runner| must be the task runner to use for file
  // operations.
  static BlimpClientContext* Create(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner);

  // The delegate provides all the required functionality from the embedder.
  virtual void SetDelegate(BlimpClientContextDelegate* delegate) = 0;

  // Creates a new BlimpContents.
  virtual std::unique_ptr<BlimpContents> CreateBlimpContents() = 0;

  // Start authentication flow and connection to engine.
  virtual void Connect() = 0;

 protected:
  BlimpClientContext() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BlimpClientContext);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_PUBLIC_BLIMP_CLIENT_CONTEXT_H_
