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
                    Profile* profile,
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
                      Profile* profile,
                      const GetDataCallback& callback);
  virtual ~GetApplistOperation();

 protected:
  // Overridden from GetDataOperation.
  virtual GURL GetURL() const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GetApplistOperation);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_OPERATIONS_H_
