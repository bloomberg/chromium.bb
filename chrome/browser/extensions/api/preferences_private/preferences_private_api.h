// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PREFERENCES_PRIVATE_PREFERENCES_PRIVATE_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_PREFERENCES_PRIVATE_PREFERENCES_PRIVATE_API_H_

#include "base/basictypes.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction
    : public ChromeAsyncExtensionFunction, public ProfileSyncServiceObserver {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "preferencesPrivate.getSyncCategoriesWithoutPassphrase",
      PREFERENCESPRIVATE_GETSYNCCATEGORIESWITHOUTPASSPHRASE)

  PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction();

 protected:
  virtual ~PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction();

 private:
  // ProfileSyncServiceObserver:
  virtual void OnStateChanged() OVERRIDE;

  // ExtensionFunction:
  virtual bool RunAsync() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(
      PreferencesPrivateGetSyncCategoriesWithoutPassphraseFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PREFERENCES_PRIVATE_PREFERENCES_PRIVATE_API_H_
