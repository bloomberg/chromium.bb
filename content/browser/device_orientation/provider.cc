// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/provider.h"

#include "base/logging.h"
#include "content/browser/device_orientation/data_fetcher.h"
#include "content/browser/device_orientation/provider_impl.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_MACOSX)
#include "content/browser/device_orientation/accelerometer_mac.h"
#elif defined(OS_ANDROID)
#include "content/browser/device_orientation/data_fetcher_impl_android.h"
#endif

using content::BrowserThread;

namespace device_orientation {

Provider* Provider::GetInstance() {
  if (!instance_) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    const ProviderImpl::DataFetcherFactory default_factories[] = {
#if defined(OS_MACOSX)
      AccelerometerMac::Create,
#elif defined(OS_ANDROID)
      DataFetcherImplAndroid::Create,
#endif
      NULL
    };

    instance_ = new ProviderImpl(default_factories);
  }
  return instance_;
}

void Provider::SetInstanceForTests(Provider* provider) {
  DCHECK(!instance_);
  instance_ = provider;
}

Provider* Provider::GetInstanceForTests() {
  return instance_;
}

Provider::Provider() {
}

Provider::~Provider() {
  DCHECK(instance_ == this);
  instance_ = NULL;
}

Provider* Provider::instance_ = NULL;

} //  namespace device_orientation
