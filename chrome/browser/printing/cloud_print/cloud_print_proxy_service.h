// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_

#include <string>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/profile.h"


class Profile;

// Layer between the browser user interface and the cloud print proxy code
// running in the service process.
class CloudPrintProxyService {
 public:
  explicit CloudPrintProxyService(Profile* profile);
  virtual ~CloudPrintProxyService();

  // Initializes the object. This should be called every time an object of this
  // class is constructed.
  void Initialize();

  // Enables/disables cloud printing for the user
  virtual void EnableForUser(const std::string& auth_token);
  virtual void DisableForUser();

 protected:
  void Shutdown();

  DISALLOW_COPY_AND_ASSIGN(CloudPrintProxyService);
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_H_

