// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_DATABASE_PERMISSION_REQUEST_H_
#define CHROME_BROWSER_RENDERER_HOST_DATABASE_PERMISSION_REQUEST_H_

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"

class Task;

// This class is fully threadsafe.
class DatabasePermissionRequest
    : public base::RefCountedThreadSafe<DatabasePermissionRequest> {
 public:
  DatabasePermissionRequest(const string16& origin,
                            const string16& database_name,
                            Task* on_allow,
                            Task* on_block);
  ~DatabasePermissionRequest();

  const string16& origin() const { return origin_; }
  const string16& database_name() const { return database_name_; }

  // Start the permission request process.
  void RequestPermission();

 private:
  // Called on the UI thread to pop up UI to request permission to open the
  // database.
  void RequestPermissionUI();

  // Info to provide the user while asking for permission.
  const string16 origin_;
  const string16 database_name_;

  // Set on IO, possibly release()ed on UI, destroyed on IO or UI.
  scoped_ptr<Task> on_allow_;
  scoped_ptr<Task> on_block_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(DatabasePermissionRequest);
};

#endif  // CHROME_BROWSER_RENDERER_HOST_DATABASE_PERMISSION_REQUEST_H_
