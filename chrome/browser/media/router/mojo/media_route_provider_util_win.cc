// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_route_provider_util_win.h"

#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/firewall_manager_win.h"
#include "content/public/browser/browser_thread.h"

namespace media_router {

namespace {

void DoCanFirewallUseLocalPorts(const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::FILE);
  base::FilePath exe_path;
  bool can_use_local_ports = false;
  if (!base::PathService::Get(base::FILE_EXE, &exe_path)) {
    LOG(WARNING) << "Couldn't get path of current executable.";
    return;
  }
  auto firewall_manager = installer::FirewallManager::Create(
      BrowserDistribution::GetDistribution(), exe_path);
  if (!firewall_manager) {
    LOG(WARNING) << "Couldn't get FirewallManager instance.";
    return;
  }
  can_use_local_ports = firewall_manager->CanUseLocalPorts();
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::Bind(callback, can_use_local_ports));
}

}  // namespace

void CanFirewallUseLocalPorts(const base::Callback<void(bool)>& callback) {
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&DoCanFirewallUseLocalPorts, callback));
}

}  // namespace media_router
