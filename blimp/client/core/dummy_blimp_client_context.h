// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_DUMMY_BLIMP_CLIENT_CONTEXT_H_
#define BLIMP_CLIENT_CORE_DUMMY_BLIMP_CLIENT_CONTEXT_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "blimp/client/public/blimp_client_context.h"
#include "blimp/client/public/blimp_contents.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#endif  // defined(OS_ANDROID)

namespace blimp {
namespace client {

#if defined(OS_ANDROID)
class DummyBlimpClientContextAndroid;
#endif  // defined(OS_ANDROID)

// A dummy implementation of the BlimpClientContext.
class DummyBlimpClientContext : public BlimpClientContext,
                                base::SupportsUserData {
 public:
  DummyBlimpClientContext();
  ~DummyBlimpClientContext() override;

#if defined(OS_ANDROID)
  DummyBlimpClientContextAndroid* GetDummyBlimpClientContextAndroid();
#endif  // defined(OS_ANDROID)

// BlimpClientContext implementation.
#if defined(OS_ANDROID)
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;
#endif  // defined(OS_ANDROID)
  void SetDelegate(BlimpClientContextDelegate* delegate) override;
  std::unique_ptr<BlimpContents> CreateBlimpContents() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyBlimpClientContext);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_DUMMY_BLIMP_CLIENT_CONTEXT_H_
