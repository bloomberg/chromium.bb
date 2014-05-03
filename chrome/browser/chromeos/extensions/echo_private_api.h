// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/ui/echo_dialog_listener.h"
#include "chrome/browser/extensions/chrome_extension_function.h"

class PrefRegistrySimple;

namespace chromeos {

class EchoDialogView;

// Namespace to register the EchoCheckedOffers field in Local State.
namespace echo_offer {

void RegisterPrefs(PrefRegistrySimple* registry);

}  // namespace echo_offer

}  // namespace chromeos

class EchoPrivateGetRegistrationCodeFunction
    : public ChromeSyncExtensionFunction {
 public:
  EchoPrivateGetRegistrationCodeFunction();

 protected:
  virtual ~EchoPrivateGetRegistrationCodeFunction();
  virtual bool RunSync() OVERRIDE;

 private:
  void GetRegistrationCode(const std::string& type);
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getRegistrationCode",
                             ECHOPRIVATE_GETREGISTRATIONCODE)
};

class EchoPrivateGetOobeTimestampFunction
    : public ChromeAsyncExtensionFunction {
 public:
  EchoPrivateGetOobeTimestampFunction();

 protected:
  virtual ~EchoPrivateGetOobeTimestampFunction();
  virtual bool RunAsync() OVERRIDE;

 private:
  bool GetOobeTimestampOnFileThread();
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getOobeTimestamp",
                             ECHOPRIVATE_GETOOBETIMESTAMP)
};

class EchoPrivateSetOfferInfoFunction : public ChromeSyncExtensionFunction {
 public:
  EchoPrivateSetOfferInfoFunction();

 protected:
  virtual ~EchoPrivateSetOfferInfoFunction();
  virtual bool RunSync() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION("echoPrivate.setOfferInfo",
                             ECHOPRIVATE_SETOFFERINFO)
};

class EchoPrivateGetOfferInfoFunction : public ChromeSyncExtensionFunction {
 public:
  EchoPrivateGetOfferInfoFunction();

 protected:
  virtual ~EchoPrivateGetOfferInfoFunction();
  virtual bool RunSync() OVERRIDE;

 private:
  DECLARE_EXTENSION_FUNCTION("echoPrivate.getOfferInfo",
                             ECHOPRIVATE_GETOFFERINFO)
};

// The function first checks if offers redeeming is allowed by the device
// policy. It should then show a dialog that, depending on the check result,
// either asks user's consent to verify the device's eligibility for the offer,
// or informs the user that the offers redeeming is disabled.
// It returns whether the user consent was given.
class EchoPrivateGetUserConsentFunction : public ChromeAsyncExtensionFunction,
                                          public chromeos::EchoDialogListener {
 public:
  // Type for the dialog shown callback used in tests.
  typedef base::Callback<void(chromeos::EchoDialogView* dialog)>
          DialogShownTestCallback;

  EchoPrivateGetUserConsentFunction();

  // Creates the function with non-null dialog_shown_callback_.
  // To be used in tests.
  static scoped_refptr<EchoPrivateGetUserConsentFunction> CreateForTest(
      const DialogShownTestCallback& dialog_shown_callback);

 protected:
  virtual ~EchoPrivateGetUserConsentFunction();

  virtual bool RunAsync() OVERRIDE;

 private:
  // chromeos::EchoDialogListener overrides.
  virtual void OnAccept() OVERRIDE;
  virtual void OnCancel() OVERRIDE;
  virtual void OnMoreInfoLinkClicked() OVERRIDE;

  // Checks whether "allow redeem ChromeOS registration offers" setting is
  // disabled in cros settings. It may be asynchronous if the needed settings
  // provider is not yet trusted.
  // Upon completion |OnRedeemOffersAllowed| is called.
  void CheckRedeemOffersAllowed();
  void OnRedeemOffersAllowedChecked(bool is_allowed);

  // Sets result and calls SendResponse.
  void Finalize(bool consent);

  // Result of |CheckRedeemOffersAllowed()|.
  bool redeem_offers_allowed_;
  // Callback used in tests. Called after an echo dialog is shown.
  DialogShownTestCallback dialog_shown_callback_;

  DECLARE_EXTENSION_FUNCTION("echoPrivate.getUserConsent",
                             ECHOPRIVATE_GETUSERCONSENT)
};

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_ECHO_PRIVATE_API_H_
