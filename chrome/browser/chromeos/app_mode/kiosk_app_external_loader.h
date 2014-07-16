// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_EXTERNAL_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_EXTERNAL_LOADER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_loader.h"

namespace chromeos {

// A custom extensions::ExternalLoader that is created by KioskAppManager and
// used for install kiosk app extensions.
class KioskAppExternalLoader
    : public extensions::ExternalLoader,
      public base::SupportsWeakPtr<KioskAppExternalLoader> {
 public:
  KioskAppExternalLoader();

  // Sets current kiosk app extensions to be loaded.
  void SetCurrentAppExtensions(scoped_ptr<base::DictionaryValue> prefs);

  // extensions::ExternalLoader overrides:
  virtual void StartLoading() OVERRIDE;

 private:
  virtual ~KioskAppExternalLoader();

  base::DictionaryValue kiosk_apps_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppExternalLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_EXTERNAL_LOADER_H_
