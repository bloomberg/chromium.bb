// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_MAP_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_MAP_H_
#pragma once

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/time.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/extensions/extension_constants.h"
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
  void AddExtension(const Extension* extension,
                    base::Time install_time,
                    bool incognito_enabled);

  // Callback for when an extension is unloaded.
  void RemoveExtension(const std::string& extension_id,
                       const extension_misc::UnloadedExtensionReason reason);

  // Returns the time the extension was installed, or base::Time() if not found.
  base::Time GetInstallTime(const std::string& extension_id) const;

  // Returns true if the user has allowed this extension to run in incognito
  // mode.
  bool IsIncognitoEnabled(const std::string& extension_id) const;

  // Returns true if the given extension can see events and data from another
  // sub-profile (incognito to original profile, or vice versa).
  bool CanCrossIncognito(const Extension* extension);

  // Registers a RenderProcessHost with |host_id| as hosting an extension.
  void BindingsEnabledForProcess(int host_id);

  // Unregisters the RenderProcessHost with |host_id|.
  void BindingsDisabledForProcess(int host_id);

  // True if this process host is hosting an extension.
  bool AreBindingsEnabledForProcess(int host_id) const;

 private:
  // Extra dynamic data related to an extension.
  struct ExtraData;
  // Map of extension_id to ExtraData.
  typedef std::map<std::string, ExtraData> ExtraDataMap;

  ExtensionSet extensions_;
  ExtensionSet disabled_extensions_;

  // Extra data associated with enabled extensions.
  ExtraDataMap extra_data_;

  // The set of process ids that have extension bindings enabled.
  std::set<int> extension_bindings_process_ids_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_MAP_H_
