// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_MAP_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_MAP_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/ref_counted.h"
#include "chrome/common/extensions/extension_extent.h"
#include "googleurl/src/gurl.h"

class Extension;

// Contains extension data that needs to be accessed on the IO thread. It can
// be created/destroyed on any thread, but all other methods must be called on
// the IO thread.
//
// TODO(mpcomplete): consider simplifying this class to return the StaticData
// object itself, since most methods are simple property accessors.
class ExtensionInfoMap : public base::RefCountedThreadSafe<ExtensionInfoMap> {
 public:
  ExtensionInfoMap();
  ~ExtensionInfoMap();

  // Callback for when new extensions are loaded.
  void AddExtension(const Extension* extension);

  // Callback for when an extension is unloaded.
  void RemoveExtension(const std::string& id);

  // Gets the name for the specified extension.
  std::string GetNameForExtension(const std::string& id) const;

  // Gets the path to the directory for the specified extension.
  FilePath GetPathForExtension(const std::string& id) const;

  // Returns true if the specified extension exists and has a non-empty web
  // extent.
  bool ExtensionHasWebExtent(const std::string& id) const;

  // Returns true if the specified extension exists and can load in incognito
  // contexts.
  bool ExtensionCanLoadInIncognito(const std::string& id) const;

  // Returns an empty string if the extension with |id| doesn't have a default
  // locale.
  std::string GetDefaultLocaleForExtension(const std::string& id) const;

  // Gets the effective host permissions for the extension with |id|.
  ExtensionExtent
      GetEffectiveHostPermissionsForExtension(const std::string& id) const;

  // Determine whether a URL has access to the specified extension permission.
  bool CheckURLAccessToExtensionPermission(const GURL& url,
                                           const char* permission_name) const;

  // Returns true if the specified URL references the icon for an extension.
  bool URLIsForExtensionIcon(const GURL& url) const;

 private:
  // Map of extension info by extension id.
  typedef std::map<std::string, scoped_refptr<const Extension> > Map;

  Map extension_info_;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_INFO_MAP_H_
