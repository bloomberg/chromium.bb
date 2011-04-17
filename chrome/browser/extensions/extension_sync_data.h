// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_
#pragma once

#include <string>

#include "base/version.h"
#include "googleurl/src/gurl.h"

// A struct that encapsulates the synced properties of an Extension.
struct ExtensionSyncData {
  ExtensionSyncData();
  ~ExtensionSyncData();

  std::string id;

  // Version-independent properties (i.e., used even when the
  // version of the currently-installed extension doesn't match
  // |version|).
  bool uninstalled;
  bool enabled;
  bool incognito_enabled;

  // Version-dependent properties (i.e., should be used only when the
  // version of the currenty-installed extension matches |version|).
  Version version;
  GURL update_url;
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SYNC_DATA_H_
