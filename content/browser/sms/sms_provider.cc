// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "content/browser/sms/sms_provider.h"
#if defined(OS_ANDROID)
#include "content/browser/sms/sms_provider_android.h"
#else
#include "content/browser/sms/sms_provider_desktop.h"
#endif

namespace content {

// static
std::unique_ptr<SmsProvider> SmsProvider::Create() {
#if defined(OS_ANDROID)
  return std::make_unique<SmsProviderAndroid>();
#else
  return std::make_unique<SmsProviderDesktop>();
#endif
}

}  // namespace content
