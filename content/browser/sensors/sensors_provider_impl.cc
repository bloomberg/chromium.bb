// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sensors/sensors_provider_impl.h"

#include "base/memory/singleton.h"
#include "content/public/browser/sensors_listener.h"

namespace content {

// static
SensorsProvider* SensorsProvider::GetInstance() {
  return SensorsProviderImpl::GetInstance();
}

// static
SensorsProviderImpl* SensorsProviderImpl::GetInstance() {
  return Singleton<SensorsProviderImpl>::get();
}

void SensorsProviderImpl::AddListener(SensorsListener* listener) {
  listeners_->AddObserver(listener);
}

void SensorsProviderImpl::RemoveListener(SensorsListener* listener) {
  listeners_->RemoveObserver(listener);
}

void SensorsProviderImpl::ScreenOrientationChanged(ScreenOrientation change) {
  listeners_->Notify(&SensorsListener::OnScreenOrientationChanged, change);
}

SensorsProviderImpl::SensorsProviderImpl()
    : listeners_(new ListenerList()) {
}

SensorsProviderImpl::~SensorsProviderImpl() {
}

}  // namespace content
