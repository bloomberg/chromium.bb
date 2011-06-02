// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_MAP_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_MAP_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"

class Extension;

// Contains extension data that needs to be accessed on the IO thread. It can
// be created/destroyed on any thread, but all other methods must be called on
// the IO thread.
class ExtensionInfoMap : public base::RefCountedThreadSafe<ExtensionInfoMap> {
 public:
  ExtensionInfoMap();
  ~ExtensionInfoMap();

  const ExtensionSet& extensions() const { return extensions_; }
  const ExtensionSet& disabled_extensions() const {
    return disabled_extensions_;
  }

  // Callback for when new extensions are loaded.
  void AddExtension(const Extension* extension);

  // Callback for when an extension is unloaded.
  void RemoveExtension(const std::string& id,
                       const UnloadedExtensionInfo::Reason reason);

 private:
  ExtensionSet extensions_;
  ExtensionSet disabled_extensions_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_MAP_H_
