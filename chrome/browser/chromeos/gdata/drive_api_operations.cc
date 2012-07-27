// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_api_operations.h"

namespace {

const char kDriveV2AboutURL[] = "https://www.googleapis.com/drive/v2/about";
const char kDriveV2ApplistURL[] = "https://www.googleapis.com/drive/v2/apps";

}  // namespace

// TODO(kochi): Rename to namespace drive. http://crbug.com/136371
namespace gdata {

//============================== GetAboutOperation =============================

GetAboutOperation::GetAboutOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const GetDataCallback& callback)
    : GetDataOperation(registry, profile, callback) {}

GetAboutOperation::~GetAboutOperation() {}

GURL GetAboutOperation::GetURL() const {
  return GURL(kDriveV2AboutURL);
}

//============================== GetApplistOperation ===========================

GetApplistOperation::GetApplistOperation(
    GDataOperationRegistry* registry,
    Profile* profile,
    const GetDataCallback& callback)
    : GetDataOperation(registry, profile, callback) {}

GetApplistOperation::~GetApplistOperation() {}

GURL GetApplistOperation::GetURL() const {
  return GURL(kDriveV2ApplistURL);
}

}  // namespace gdata
