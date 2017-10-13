// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/chrome_browser_main_extra_parts_resource_coordinator.h"

#include "base/process/process.h"
#include "chrome/browser/resource_coordinator/browser_child_process_watcher.h"
#include "content/public/common/service_manager_connection.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_features.h"

ChromeBrowserMainExtraPartsResourceCoordinator::
    ChromeBrowserMainExtraPartsResourceCoordinator() = default;
ChromeBrowserMainExtraPartsResourceCoordinator::
    ~ChromeBrowserMainExtraPartsResourceCoordinator() = default;

void ChromeBrowserMainExtraPartsResourceCoordinator::
    ServiceManagerConnectionStarted(
        content::ServiceManagerConnection* connection) {
  if (!resource_coordinator::IsResourceCoordinatorEnabled())
    return;

  process_resource_coordinator_ =
      base::MakeUnique<resource_coordinator::ResourceCoordinatorInterface>(
          connection->GetConnector(),
          resource_coordinator::CoordinationUnitType::kProcess);

  process_resource_coordinator_->SetProperty(
      resource_coordinator::mojom::PropertyType::kPID,
      base::Process::Current().Pid());

  process_resource_coordinator_->SetProperty(
      resource_coordinator::mojom::PropertyType::kLaunchTime,
      base::Time::Now().ToTimeT());

  browser_child_process_watcher_ =
      base::MakeUnique<resource_coordinator::BrowserChildProcessWatcher>();
}
