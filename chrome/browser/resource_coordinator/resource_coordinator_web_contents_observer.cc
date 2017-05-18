// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/resource_coordinator_web_contents_observer.h"

#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(ResourceCoordinatorWebContentsObserver);

ResourceCoordinatorWebContentsObserver::ResourceCoordinatorWebContentsObserver(
    content::WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  tab_resource_coordinator_ =
      base::MakeUnique<resource_coordinator::ResourceCoordinatorInterface>(
          content::ServiceManagerConnection::GetForProcess()->GetConnector(),
          resource_coordinator::CoordinationUnitType::kWebContents);
}

ResourceCoordinatorWebContentsObserver::
    ~ResourceCoordinatorWebContentsObserver() = default;

// static
bool ResourceCoordinatorWebContentsObserver::IsEnabled() {
  return base::FeatureList::IsEnabled(features::kGlobalResourceCoordinator);
}

void ResourceCoordinatorWebContentsObserver::WasShown() {
  tab_resource_coordinator_->SendEvent(
      resource_coordinator::EventType::kOnWebContentsShown);
}

void ResourceCoordinatorWebContentsObserver::WasHidden() {
  tab_resource_coordinator_->SendEvent(
      resource_coordinator::EventType::kOnWebContentsHidden);
}
