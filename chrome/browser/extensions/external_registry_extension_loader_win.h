// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_EXTENSION_LOADER_WIN_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_EXTENSION_LOADER_WIN_H_
#pragma once

#include "chrome/browser/extensions/external_extension_loader.h"

class ExternalRegistryExtensionLoader : public ExternalExtensionLoader {
 public:
  ExternalRegistryExtensionLoader() {}

 protected:
  virtual void StartLoading();

 private:
  friend class base::RefCountedThreadSafe<ExternalExtensionLoader>;

  virtual ~ExternalRegistryExtensionLoader() {}

  void LoadOnFileThread();

  DISALLOW_COPY_AND_ASSIGN(ExternalRegistryExtensionLoader);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_REGISTRY_EXTENSION_LOADER_WIN_H_
