// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_LOADER_WIN_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_LOADER_WIN_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/win/registry.h"
#include "chrome/browser/extensions/external_loader.h"

namespace extensions {

class ExternalRegistryLoader : public ExternalLoader {
 public:
  ExternalRegistryLoader() {}

 protected:
  void StartLoading() override;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  ~ExternalRegistryLoader() override {}

  scoped_ptr<base::DictionaryValue> LoadPrefsOnFileThread();
  void LoadOnFileThread();
  void CompleteLoadAndStartWatchingRegistry();
  void UpdatePrefsOnFileThread();
  void OnRegistryKeyChanged(base::win::RegKey* key);

  base::win::RegKey hklm_key_;
  base::win::RegKey hkcu_key_;

  DISALLOW_COPY_AND_ASSIGN(ExternalRegistryLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_LOADER_WIN_H_
