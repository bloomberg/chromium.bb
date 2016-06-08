// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/permission_reporter.h"
#include "chrome/common/safe_browsing/permission_report.pb.h"
#include "content/public/browser/permission_type.h"

using content::PermissionType;

namespace safe_browsing {

namespace {
// URL to upload permission action reports.
const char kPermissionActionReportingUploadUrl[] =
    "http://safebrowsing.googleusercontent.com/safebrowsing/clientreport/"
    "permission-action";
}  // namespace

PermissionReporter::PermissionReporter(
    net::URLRequestContext* request_context) {}

PermissionReporter::~PermissionReporter() {}

void PermissionReporter::SendReport(const GURL& origin,
                                    content::PermissionType permission,
                                    PermissionAction action) {
  // TODO(stefanocs): Implement SendReport function.
  ALLOW_UNUSED_LOCAL(kPermissionActionReportingUploadUrl);
}

// static
bool PermissionReporter::BuildReport(const GURL& origin,
                                     PermissionType permission,
                                     PermissionAction action,
                                     std::string* output) {
  // TODO(stefanocs): Implement BuildReport function.
  return true;
}

}  // namespace safe_browsing
