// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list.h"
#include "components/physical_web/data_source/physical_web_data_source.h"
#include "components/physical_web/data_source/physical_web_data_source_impl.h"
#include "components/physical_web/data_source/physical_web_listener.h"

namespace physical_web {

PhysicalWebDataSourceImpl::PhysicalWebDataSourceImpl() {}

PhysicalWebDataSourceImpl::~PhysicalWebDataSourceImpl() {}

void PhysicalWebDataSourceImpl::RegisterListener(
    PhysicalWebListener* physical_web_listener) {
  observer_list_.AddObserver(physical_web_listener);
}

void PhysicalWebDataSourceImpl::UnregisterListener(
    PhysicalWebListener* physical_web_listener) {
  observer_list_.RemoveObserver(physical_web_listener);
}

void PhysicalWebDataSourceImpl::NotifyOnFound(const GURL& url) {
  for (PhysicalWebListener& observer : observer_list_)
    observer.OnFound(url);
}

void PhysicalWebDataSourceImpl::NotifyOnLost(const GURL& url) {
  for (PhysicalWebListener& observer : observer_list_)
    observer.OnLost(url);
}

void PhysicalWebDataSourceImpl::NotifyOnDistanceChanged(
    const GURL& url,
    double distance_estimate) {
  for (PhysicalWebListener& observer : observer_list_)
    observer.OnDistanceChanged(url, distance_estimate);
}

}  // namespace physical_web
