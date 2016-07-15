// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/client/core/blimp_client_context_impl.h"

#include "base/memory/ptr_util.h"
#include "base/supports_user_data.h"
#include "blimp/client/core/blimp_contents_impl.h"
#include "blimp/client/public/blimp_client_context_delegate.h"

#if defined(OS_ANDROID)
#include "blimp/client/core/android/blimp_client_context_impl_android.h"
#endif  // OS_ANDROID

namespace blimp {
namespace client {

namespace {

#if defined(OS_ANDROID)
const char kBlimpClientContextImplAndroidKey[] =
    "blimp_client_context_impl_android";
#endif  // OS_ANDROID
}

// This function is declared in //blimp/client/public/blimp_client_context.h,
// and either this function or the one in
// //blimp/client/core/dummy_blimp_client_context.cc should be linked in to
// any binary using BlimpClientContext::Create.
// static
BlimpClientContext* BlimpClientContext::Create() {
  return new BlimpClientContextImpl();
}

BlimpClientContextImpl::BlimpClientContextImpl() : BlimpClientContext() {}

BlimpClientContextImpl::~BlimpClientContextImpl() {}

#if defined(OS_ANDROID)

base::android::ScopedJavaLocalRef<jobject>
BlimpClientContextImpl::GetJavaObject() {
  return GetBlimpClientContextImplAndroid()->GetJavaObject();
}

BlimpClientContextImplAndroid*
BlimpClientContextImpl::GetBlimpClientContextImplAndroid() {
  BlimpClientContextImplAndroid* blimp_client_contents_impl_android =
      static_cast<BlimpClientContextImplAndroid*>(
          GetUserData(kBlimpClientContextImplAndroidKey));
  if (!blimp_client_contents_impl_android) {
    blimp_client_contents_impl_android =
        new BlimpClientContextImplAndroid(this);
    SetUserData(kBlimpClientContextImplAndroidKey,
                blimp_client_contents_impl_android);
  }
  return blimp_client_contents_impl_android;
}

#endif  // defined(OS_ANDROID)

void BlimpClientContextImpl::SetDelegate(BlimpClientContextDelegate* delegate) {
  delegate_ = delegate;
}

std::unique_ptr<BlimpContents> BlimpClientContextImpl::CreateBlimpContents() {
  std::unique_ptr<BlimpContents> blimp_contents =
      base::WrapUnique(new BlimpContentsImpl);
  delegate_->AttachBlimpContentsHelpers(blimp_contents.get());
  return blimp_contents;
}

}  // namespace client
}  // namespace blimp
