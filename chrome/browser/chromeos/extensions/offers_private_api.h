// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_OFFERS_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_OFFERS_PRIVATE_API_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"

class GetCouponCodeFunction : public SyncExtensionFunction {
 public:
  GetCouponCodeFunction();

 protected:
  virtual ~GetCouponCodeFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION_NAME("offersPrivate.getCouponCode");
};

class GetUserConsentFunction : public AsyncExtensionFunction {
 public:
  GetUserConsentFunction();

  // Called when user's consent response is known.
  void SetConsent(bool is_consent);

 protected:
  virtual ~GetUserConsentFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  // Show user consent confirmation bar.
  void ShowConsentInfoBar();

  DECLARE_EXTENSION_FUNCTION_NAME("offersPrivate.getUserConsent");
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_OFFERS_PRIVATE_API_H_
