// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_LOADER_WIN_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_LOADER_WIN_H_

#include "base/compiler_specific.h"
#include "chrome/browser/extensions/external_loader.h"

namespace extensions {

class ExternalRegistryLoader : public ExternalLoader {
 public:
  ExternalRegistryLoader() {}

 protected:
  virtual void StartLoading() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  virtual ~ExternalRegistryLoader() {}

  void LoadOnFileThread();

  DISALLOW_COPY_AND_ASSIGN(ExternalRegistryLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_LOADER_WIN_H_
