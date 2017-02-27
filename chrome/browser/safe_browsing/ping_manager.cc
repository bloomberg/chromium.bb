// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/ping_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/safe_browsing/notification_image_reporter.h"
#include "chrome/browser/safe_browsing/permission_reporter.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context_getter.h"
#include "third_party/skia/include/core/SkBitmap.h"

using content::BrowserThread;

namespace safe_browsing {

// SafeBrowsingPingManager implementation ----------------------------------

// static
std::unique_ptr<SafeBrowsingPingManager> SafeBrowsingPingManager::Create(
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return base::WrapUnique(
      new SafeBrowsingPingManager(request_context_getter, config));
}

SafeBrowsingPingManager::SafeBrowsingPingManager(
    net::URLRequestContextGetter* request_context_getter,
    const SafeBrowsingProtocolConfig& config)
    : BasePingManager(request_context_getter, config) {
  if (request_context_getter) {
    permission_reporter_ = base::MakeUnique<PermissionReporter>(
        request_context_getter->GetURLRequestContext());
    notification_image_reporter_ = base::MakeUnique<NotificationImageReporter>(
        request_context_getter->GetURLRequestContext());
  }
}

SafeBrowsingPingManager::~SafeBrowsingPingManager() {
}

void SafeBrowsingPingManager::ReportPermissionAction(
    const PermissionReportInfo& report_info) {
  permission_reporter_->SendReport(report_info);
}

void SafeBrowsingPingManager::ReportNotificationImage(
    Profile* profile,
    const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
    const GURL& origin,
    const SkBitmap& image) {
  notification_image_reporter_->ReportNotificationImageOnIO(
      profile, database_manager, origin, image);
}

}  // namespace safe_browsing
