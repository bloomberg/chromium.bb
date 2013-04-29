// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_OBSERVER_H_
#define COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_OBSERVER_H_

#include "components/webdata/common/webdata_export.h"
#include "sql/init_status.h"

// WebDatabase loads asynchronously on the DB thread. Clients on the UI thread
// can use this interface to be notified when the load is complete, or if it
// fails.
class WEBDATA_EXPORT WebDatabaseObserver {
 public:
  // Called when DB has been loaded successfully.
  virtual void WebDatabaseLoaded() = 0;
  // Called when load failed. |status| contains error code.
  virtual void WebDatabaseLoadFailed(sql::InitStatus status) {};

 protected:
  virtual ~WebDatabaseObserver() {}
};


#endif  // COMPONENTS_WEBDATA_COMMON_WEB_DATABASE_OBSERVER_H_
