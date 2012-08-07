// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_OPERATIONS_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_OPERATIONS_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/gdata/operations_base.h"

// TODO(kochi): Rename to namespace drive. http://crbug.com/136371
namespace gdata {

//============================== GetAboutOperation =============================

// This class performs the operation for fetching About data.
class GetAboutOperation : public GetDataOperation {
 public:
  GetAboutOperation(GDataOperationRegistry* registry,
                    const GetDataCallback& callback);
  virtual ~GetAboutOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetAboutOperation);
};

//============================= GetApplistOperation ============================

// This class performs the operation for fetching Applist.
class GetApplistOperation : public GetDataOperation {
 public:
  GetApplistOperation(GDataOperationRegistry* registry,
                      const GetDataCallback& callback);
  virtual ~GetApplistOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetApplistOperation);
};

//============================ GetChangelistOperation ==========================

// This class performs the operation for fetching changelist.
class GetChangelistOperation : public GetDataOperation {
 public:
  // |start_changestamp| specifies the starting point of change list or 0 if
  // all changes are necessary.
  // |url| specifies URL for document feed fetching operation. If empty URL is
  // passed, the default URL is used and returns the first page of the result.
  // When non-first page result is requested, |url| should be specified.
  GetChangelistOperation(GDataOperationRegistry* registry,
                         const GURL& url,
                         int64 start_changestamp,
                         const GetDataCallback& callback);
  virtual ~GetChangelistOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  GURL url_;
  int64 start_changestamp_;

  DISALLOW_COPY_AND_ASSIGN(GetChangelistOperation);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_OPERATIONS_H_
