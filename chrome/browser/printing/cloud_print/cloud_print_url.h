// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_URL_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_URL_H_
#pragma once

class GURL;
class Profile;

// Centralize URL management for the cloud print service.
class CloudPrintURL {
 public:
  explicit CloudPrintURL(Profile* profile) : profile_(profile) {}

  GURL GetCloudPrintServiceURL();
  GURL GetCloudPrintServiceDialogURL();
  GURL GetCloudPrintServiceManageURL();

  // These aren't derived from the service, but it makes sense to keep all the
  // URLs together, and this gives the unit tests access for testing.
  static GURL GetCloudPrintLearnMoreURL();
  static GURL GetCloudPrintTestPageURL();

 private:
  void RegisterPreferences();

  Profile* profile_;
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_URL_H_
