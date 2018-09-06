// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares a class that contains various method related to branding.

#ifndef CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_

#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"

class AppRegistrationData;

class BrowserDistribution {
 public:
  virtual ~BrowserDistribution();

  static BrowserDistribution* GetDistribution();

  // Getter and adaptors for the underlying |app_reg_data_|.
  const AppRegistrationData& GetAppRegistrationData() const;
  base::string16 GetStateKey() const;
  base::string16 GetStateMediumKey() const;

 protected:
  explicit BrowserDistribution(
      std::unique_ptr<AppRegistrationData> app_reg_data);

  template<class DistributionClass>
  static BrowserDistribution* GetOrCreateBrowserDistribution(
      BrowserDistribution** dist);

  std::unique_ptr<AppRegistrationData> app_reg_data_;

 private:
  BrowserDistribution();

  DISALLOW_COPY_AND_ASSIGN(BrowserDistribution);
};

#endif  // CHROME_INSTALLER_UTIL_BROWSER_DISTRIBUTION_H_
