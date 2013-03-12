// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/extension_function.h"

class EchoPrivateGetRegistrationCodeFunction : public SyncExtensionFunction {
 public:
  EchoPrivateGetRegistrationCodeFunction();

 protected:
  virtual ~EchoPrivateGetRegistrationCodeFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  void GetRegistrationCode(const std::string& type);
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getRegistrationCode",
                             ECHOPRIVATE_GETREGISTRATIONCODE)
};

class EchoPrivateGetOobeTimestampFunction : public AsyncExtensionFunction {
 public:
  EchoPrivateGetOobeTimestampFunction();

 protected:
  virtual ~EchoPrivateGetOobeTimestampFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  bool GetOobeTimestampOnFileThread();
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getOobeTimestamp",
                             ECHOPRIVATE_GETOOBETIMESTAMP)
};

// TODO(tbarzic): Remove this once echo.getUserConsent function is up and
// running.
class EchoPrivateCheckAllowRedeemOffersFunction
    : public AsyncExtensionFunction {
 public:
  EchoPrivateCheckAllowRedeemOffersFunction();

 protected:
  virtual ~EchoPrivateCheckAllowRedeemOffersFunction();
  virtual bool RunImpl() OVERRIDE;

 private:
  void CheckAllowRedeemOffers();
  DECLARE_EXTENSION_FUNCTION("echoPrivate.checkAllowRedeemOffers",
                             ECHOPRIVATE_CHECKALLOWREDEEMOFFERS)
};

// The function first checks if offers redeeming is allowed by the device
// policy. It should then show a dialog that, depending on the check result,
// either asks user's consent to verify the device's eligibility for the offer,
// or informs the user that the offers redeeming is disabled.
// It returns whether the user consent was given.
//
// NOTE: Currently only the first part is implemented, and its result is kept in
// |redeem_offers_allowed_|. The function itself always returns false.
class EchoPrivateGetUserConsentFunction : public AsyncExtensionFunction {
 public:
  EchoPrivateGetUserConsentFunction();

  bool redeem_offers_allowed() const { return redeem_offers_allowed_; }

 protected:
  virtual ~EchoPrivateGetUserConsentFunction();

  virtual bool RunImpl() OVERRIDE;

 private:
  // Checks whether "allow redeem ChromeOS registration offers" setting is
  // disabled in cros settings. It may be asynchronous if the needed settings
  // provider is not yet trusted.
  // Upon completion |OnRedeemOffersAllowed| is called.
  void CheckRedeemOffersAllowed();
  void OnRedeemOffersAllowedChecked(bool is_allowed);

  // Used only for test, and only until the rest of functionality is
  // implemented.
  bool redeem_offers_allowed_;

  DECLARE_EXTENSION_FUNCTION("echoPrivate.getUserConsent",
                             ECHOPRIVATE_GETUSERCONSENT)
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_
