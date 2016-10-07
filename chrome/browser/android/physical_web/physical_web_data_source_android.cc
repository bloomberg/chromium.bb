// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/physical_web/physical_web_data_source_android.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"

PhysicalWebDataSourceAndroid::PhysicalWebDataSourceAndroid() {
}

PhysicalWebDataSourceAndroid::~PhysicalWebDataSourceAndroid() {
}

void PhysicalWebDataSourceAndroid::StartDiscovery(
    bool network_request_enabled) {
}

void PhysicalWebDataSourceAndroid::StopDiscovery() {
}

std::unique_ptr<base::ListValue> PhysicalWebDataSourceAndroid::GetMetadata() {
  return base::MakeUnique<base::ListValue>();
}

bool PhysicalWebDataSourceAndroid::HasUnresolvedDiscoveries() {
  return false;
}

void PhysicalWebDataSourceAndroid::RegisterListener(
    PhysicalWebListener* listener) {
}

void PhysicalWebDataSourceAndroid::UnregisterListener(
    PhysicalWebListener* listener) {
}
