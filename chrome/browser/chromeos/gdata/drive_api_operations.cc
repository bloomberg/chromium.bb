// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/drive_api_operations.h"

#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "chrome/common/net/url_util.h"

namespace {

const char kDriveV2AboutURL[] = "https://www.googleapis.com/drive/v2/about";
const char kDriveV2ApplistURL[] = "https://www.googleapis.com/drive/v2/apps";
const char kDriveV2ChangelistURL[] =
    "https://www.googleapis.com/drive/v2/changes";

const char kDriveV2FilelistURL[] = "https://www.googleapis.com/drive/v2/files";
const char kDriveV2FileURLFormat[] =
    "https://www.googleapis.com/drive/v2/files/%s";

}  // namespace

// TODO(kochi): Rename to namespace drive. http://crbug.com/136371
namespace gdata {

//============================== GetAboutOperation =============================

GetAboutOperation::GetAboutOperation(OperationRegistry* registry,
                                     const GetDataCallback& callback)
    : GetDataOperation(registry, callback) {}

GetAboutOperation::~GetAboutOperation() {}

GURL GetAboutOperation::GetURL() const {
  return GURL(kDriveV2AboutURL);
}

//============================== GetApplistOperation ===========================

GetApplistOperation::GetApplistOperation(OperationRegistry* registry,
                                         const GetDataCallback& callback)
    : GetDataOperation(registry, callback) {}

GetApplistOperation::~GetApplistOperation() {}

GURL GetApplistOperation::GetURL() const {
  return GURL(kDriveV2ApplistURL);
}

//============================ GetChangelistOperation ==========================

GetChangelistOperation::GetChangelistOperation(
    OperationRegistry* registry,
    const GURL& url,
    int64 start_changestamp,
    const GetDataCallback& callback)
    : GetDataOperation(registry, callback),
      url_(kDriveV2ChangelistURL),
      start_changestamp_(start_changestamp) {
  if (!url.is_empty())
    url_ = url;
}

GetChangelistOperation::~GetChangelistOperation() {}

GURL GetChangelistOperation::GetURL() const {
  if (start_changestamp_)
    return chrome_common_net::AppendOrReplaceQueryParameter(
        url_, "startChangeId", base::Int64ToString(start_changestamp_));
  return url_;
}

//============================= GetFlielistOperation ===========================

GetFilelistOperation::GetFilelistOperation(
    OperationRegistry* registry,
    const GURL& url,
    const std::string& search_string,
    const GetDataCallback& callback)
    : GetDataOperation(registry, callback),
      url_(kDriveV2FilelistURL),
      search_string_(search_string) {
  if (!url.is_empty())
    url_ = url;
}

GetFilelistOperation::~GetFilelistOperation() {}

GURL GetFilelistOperation::GetURL() const {
  if (!search_string_.empty()) {
    return chrome_common_net::AppendOrReplaceQueryParameter(
        url_, "q", search_string_);
  }
  return url_;
}

//=============================== GetFlieOperation =============================

GetFileOperation::GetFileOperation(
    OperationRegistry* registry,
    const std::string& file_id,
    const GetDataCallback& callback)
    : GetDataOperation(registry, callback),
      file_id_(file_id) {}

GetFileOperation::~GetFileOperation() {}

GURL GetFileOperation::GetURL() const {
  return GURL(base::StringPrintf(kDriveV2FileURLFormat, file_id_.c_str()));
}

}  // namespace gdata
