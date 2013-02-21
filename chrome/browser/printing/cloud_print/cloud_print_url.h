// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_URL_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_URL_H_

#include <string>

class GURL;
class PrefRegistrySyncable;
class Profile;

// Centralize URL management for the cloud print service.
class CloudPrintURL {
 public:
  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

  explicit CloudPrintURL(Profile* profile) : profile_(profile) {}

  GURL GetCloudPrintServiceURL();
  GURL GetCloudPrintServiceDialogURL();
  GURL GetCloudPrintServiceManageURL();
  GURL GetCloudPrintServiceEnableURL(const std::string& proxy_id);
  GURL GetCloudPrintSigninURL();

  // These aren't derived from the service, but it makes sense to keep all the
  // URLs together, and this gives the unit tests access for testing.
  static GURL GetCloudPrintLearnMoreURL();
  static GURL GetCloudPrintTestPageURL();

 private:
  Profile* profile_;
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_URL_H_
