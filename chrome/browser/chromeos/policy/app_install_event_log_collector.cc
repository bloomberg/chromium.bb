// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/app_install_event_log_collector.h"

namespace policy {

AppInstallEventLogCollector::AppInstallEventLogCollector(
    Delegate* delegate,
    Profile* profile,
    const std::set<std::string>& pending_packages) {}

AppInstallEventLogCollector::~AppInstallEventLogCollector() = default;

void AppInstallEventLogCollector::OnPendingPackagesChanged(
    const std::set<std::string>& added,
    const std::set<std::string>& removed) {}

}  // namespace policy
