// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_EXTENSIONS_EXTERNAL_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_EXTENSIONS_EXTERNAL_LOADER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/extensions/external_loader.h"

class Profile;

namespace base {
class Value;
}

namespace chromeos {

// External loader for extensions to be loaded into demo mode sessions. The CRX
// files are loaded from preinstalled demo mode resources image mounted by
// image loader service (from the device's stateful partition).
// The loader reads external extensions prefs from the mounted demo resource
// image, and converts crx paths, which are expected to be relative to the
// mounted demo resources root to absolute paths that can be used by external
// extensions provider.
// NOTE: The class is expected to be used on the UI thread exclusively.
class DemoExtensionsExternalLoader : public extensions::ExternalLoader {
 public:
  // Whether demo apps should be loaded for the profile - i.e. whether the
  // profile is the primary profile in a demo session.
  static bool SupportedForProfile(Profile* profile);

  DemoExtensionsExternalLoader();

  // extensions::ExternalLoader:
  void StartLoading() override;

 protected:
  ~DemoExtensionsExternalLoader() override;

 private:
  // Starts loading the external extensions prefs. Passed as callback to
  // DemoSession::EnsureOfflineResourcesLoaded() in StartLoading() - it
  // will get called when offline demo resources have finished loading.
  void StartLoadingFromOfflineDemoResources();

  // Called when the external extensions prefs are read from the disk.
  // |prefs| - demo extensions prefs.
  void DemoExternalExtensionsPrefsLoaded(base::Optional<base::Value> prefs);

  base::WeakPtrFactory<DemoExtensionsExternalLoader> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DemoExtensionsExternalLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_EXTENSIONS_EXTERNAL_LOADER_H_
