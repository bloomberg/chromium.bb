// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/reporting_permissions_checker.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_manager.h"
#include "content/public/browser/permission_type.h"
#include "url/gurl.h"
#include "url/origin.h"

ReportingPermissionsChecker::ReportingPermissionsChecker(Profile* profile)
    : profile_(profile), const_weak_factory_(this) {}

ReportingPermissionsChecker::~ReportingPermissionsChecker() = default;

void ReportingPermissionsChecker::FilterReportingOrigins(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) const {
  content::BrowserThread::PostTask(
      content::BrowserThread::UI, FROM_HERE,
      base::BindOnce(
          &ReportingPermissionsChecker::FilterReportingOriginsInUIThread,
          const_weak_factory_.GetWeakPtr(), std::move(origins),
          std::move(result_callback)));
}

void ReportingPermissionsChecker::FilterReportingOriginsInUIThread(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) const {
  content::PermissionManager* permission_manager =
      profile_->GetPermissionManager();

  if (permission_manager) {
    for (auto it = origins.begin(); it != origins.end();) {
      GURL origin = it->GetURL();
      bool allowed = permission_manager->GetPermissionStatus(
                         content::PermissionType::BACKGROUND_SYNC, origin,
                         origin) == blink::mojom::PermissionStatus::GRANTED;
      if (!allowed) {
        origins.erase(it++);
      } else {
        ++it;
      }
    }
  }

  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(std::move(result_callback), std::move(origins)));
}
