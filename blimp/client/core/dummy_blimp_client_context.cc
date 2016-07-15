// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/dummy_blimp_client_context.h"

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "blimp/client/public/blimp_client_context_delegate.h"

#if defined(OS_ANDROID)
#include "blimp/client/core/android/dummy_blimp_client_context_android.h"
#endif  // OS_ANDROID

namespace blimp {
namespace client {

namespace {

#if defined(OS_ANDROID)
const char kDummyBlimpClientContextAndroidKey[] =
    "dummy_blimp_client_context_android";
#endif  // OS_ANDROID
}

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/blimp_client_context_impl.cc should be linked in to
// any binary using BlimpClientContext::Create.
// static
BlimpClientContext* BlimpClientContext::Create() {
  return new DummyBlimpClientContext();
}

DummyBlimpClientContext::DummyBlimpClientContext() : BlimpClientContext() {}

DummyBlimpClientContext::~DummyBlimpClientContext() {}

#if defined(OS_ANDROID)

base::android::ScopedJavaLocalRef<jobject>
DummyBlimpClientContext::GetJavaObject() {
  return GetDummyBlimpClientContextAndroid()->GetJavaObject();
}

DummyBlimpClientContextAndroid*
DummyBlimpClientContext::GetDummyBlimpClientContextAndroid() {
  DummyBlimpClientContextAndroid* dummy_blimp_client_contents_android =
      static_cast<DummyBlimpClientContextAndroid*>(
          GetUserData(kDummyBlimpClientContextAndroidKey));
  if (!dummy_blimp_client_contents_android) {
    dummy_blimp_client_contents_android = new DummyBlimpClientContextAndroid();
    SetUserData(kDummyBlimpClientContextAndroidKey,
                dummy_blimp_client_contents_android);
  }
  return dummy_blimp_client_contents_android;
}

#endif  // defined(OS_ANDROID)

void DummyBlimpClientContext::SetDelegate(
    BlimpClientContextDelegate* delegate) {}

std::unique_ptr<BlimpContents> DummyBlimpClientContext::CreateBlimpContents() {
  return nullptr;
}

}  // namespace client
}  // namespace blimp
