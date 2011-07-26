// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sensors/sensors_provider.h"

#include "content/browser/sensors/sensors_provider_impl.h"

namespace sensors {

Provider* Provider::GetInstance() {
  return ProviderImpl::GetInstance();
}

Provider::Provider() {
}

Provider::~Provider() {
}

}  // namespace sensors
