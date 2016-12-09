// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_API_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_API_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/chrome_extension_function_details.h"
#include "chrome/common/extensions/api/quick_unlock_private.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "extensions/browser/extension_function.h"

namespace chromeos {
class ExtendedAuthenticator;
}

namespace extensions {

class QuickUnlockPrivateGetAvailableModesFunction
    : public UIThreadExtensionFunction {
 public:
  QuickUnlockPrivateGetAvailableModesFunction();
  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.getAvailableModes",
                             QUICKUNLOCKPRIVATE_GETAVAILABLEMODES);

 protected:
  ~QuickUnlockPrivateGetAvailableModesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockPrivateGetAvailableModesFunction);
};

class QuickUnlockPrivateGetActiveModesFunction
    : public UIThreadExtensionFunction {
 public:
  QuickUnlockPrivateGetActiveModesFunction();
  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.getActiveModes",
                             QUICKUNLOCKPRIVATE_GETACTIVEMODES);

 protected:
  ~QuickUnlockPrivateGetActiveModesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  ChromeExtensionFunctionDetails chrome_details_;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockPrivateGetActiveModesFunction);
};

class QuickUnlockPrivateCheckCredentialFunction
    : public UIThreadExtensionFunction {
 public:
  QuickUnlockPrivateCheckCredentialFunction();
  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.checkCredential",
                             QUICKUNLOCKPRIVATE_CHECKCREDENTIAL);

 protected:
  ~QuickUnlockPrivateCheckCredentialFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuickUnlockPrivateCheckCredentialFunction);
};

class QuickUnlockPrivateGetCredentialRequirementsFunction
    : public UIThreadExtensionFunction {
 public:
  QuickUnlockPrivateGetCredentialRequirementsFunction();
  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.getCredentialRequirements",
                             QUICKUNLOCKPRIVATE_GETCREDENTIALREQUIREMENTS);

 protected:
  ~QuickUnlockPrivateGetCredentialRequirementsFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(QuickUnlockPrivateGetCredentialRequirementsFunction);
};

class QuickUnlockPrivateSetModesFunction : public UIThreadExtensionFunction,
                                           public chromeos::AuthStatusConsumer {
 public:
  using AuthenticatorAllocator =
      base::Callback<chromeos::ExtendedAuthenticator*(
          chromeos::AuthStatusConsumer* auth_status_consumer)>;
  using QuickUnlockMode =
      extensions::api::quick_unlock_private::QuickUnlockMode;
  using ModesChangedEventHandler =
      base::Callback<void(const std::vector<QuickUnlockMode>&)>;

  QuickUnlockPrivateSetModesFunction();

  // Use the given |allocator| to create an ExtendedAuthenticator instance. This
  // lets tests intercept authentication calls.
  void SetAuthenticatorAllocatorForTesting(
      const AuthenticatorAllocator& allocator);

  // The given event handler will be called whenever a
  // quickUnlockPrivate.onActiveModesChanged event is raised instead of the
  // default event handling mechanism.
  void SetModesChangedEventHandlerForTesting(
      const ModesChangedEventHandler& handler);

  DECLARE_EXTENSION_FUNCTION("quickUnlockPrivate.setModes",
                             QUICKUNLOCKPRIVATE_SETMODES);

 protected:
  ~QuickUnlockPrivateSetModesFunction() override;

  // ExtensionFunction overrides.
  ResponseAction Run() override;

  // AuthStatusConsumer overrides.
  void OnAuthFailure(const chromeos::AuthFailure& error) override;
  void OnAuthSuccess(const chromeos::UserContext& user_context) override;

  void ApplyModeChange();

 private:
  void FireEvent(const std::vector<QuickUnlockMode>& modes);

  ChromeExtensionFunctionDetails chrome_details_;
  scoped_refptr<chromeos::ExtendedAuthenticator> extended_authenticator_;
  std::unique_ptr<api::quick_unlock_private::SetModes::Params> params_;

  AuthenticatorAllocator authenticator_allocator_;
  ModesChangedEventHandler modes_changed_handler_;

  DISALLOW_COPY_AND_ASSIGN(QuickUnlockPrivateSetModesFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_QUICK_UNLOCK_PRIVATE_QUICK_UNLOCK_PRIVATE_API_H_
