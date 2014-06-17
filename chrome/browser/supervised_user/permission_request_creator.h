// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_H_
#define CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_H_

#include <string>

#include "base/callback_forward.h"

class PermissionRequestCreator {
 public:
  virtual ~PermissionRequestCreator() {}

  virtual void CreatePermissionRequest(const std::string& url_requested,
                                       const base::Closure& callback) = 0;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_H_
