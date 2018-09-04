// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file extends generic BrowserDistribution class to declare Google Chrome
// specific implementation.

#ifndef CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
#define CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_

#include "chrome/installer/util/browser_distribution.h"

class GoogleChromeDistribution : public BrowserDistribution {
 public:
  base::string16 GetPublisherName() override;

 protected:
  // Disallow construction from others.
  GoogleChromeDistribution();

 private:
  friend class BrowserDistribution;
};

#endif  // CHROME_INSTALLER_UTIL_GOOGLE_CHROME_DISTRIBUTION_H_
