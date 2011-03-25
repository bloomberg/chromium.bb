// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_INFO_H_
#define CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_INFO_H_
#pragma once

#include <map>
#include <string>

#include "chrome/common/extensions/extension.h"

class GURL;

// A pending extension is an extension that hasn't been installed yet
// and is intended to be installed in the next auto-update cycle.  The
// update URL of a pending extension may be blank, in which case a
// default one is assumed.
class PendingExtensionInfo {
 public:
  typedef bool (*ShouldAllowInstallPredicate)(const Extension&);

  PendingExtensionInfo(
      const GURL& update_url,
      ShouldAllowInstallPredicate should_allow_install,
      bool is_from_sync,
      bool install_silently,
      bool enable_on_install,
      bool enable_incognito_on_install,
      Extension::Location install_source);

  // Required for STL container membership.  Should not be used.
  PendingExtensionInfo();

  const GURL& update_url() const { return update_url_; }

  // ShouldAllowInstall() returns the result of running constructor argument
  // |should_allow_install| on an extension. After an extension is unpacked,
  // this function is run. If it returns true, the extension is installed.
  // If not, the extension is discarded. This allows creators of
  // PendingExtensionInfo objects to ensure that extensions meet some criteria
  // that can only be tested once the extension is unpacked.
  bool ShouldAllowInstall(const Extension& extension) const {
    return should_allow_install_(extension);
  }
  bool is_from_sync() const { return is_from_sync_; }
  bool install_silently() const { return install_silently_; }
  bool enable_on_install() const { return enable_on_install_; }
  bool enable_incognito_on_install() const {
    return enable_incognito_on_install_;
  }
  Extension::Location install_source() const { return install_source_; }

 private:
  GURL update_url_;

  // When the extension is about to be installed, this function is
  // called.  If this function returns true, the install proceeds.  If
  // this function returns false, the install is aborted.
  ShouldAllowInstallPredicate should_allow_install_;

  bool is_from_sync_;  // This update check was initiated from sync.
  bool install_silently_;
  bool enable_on_install_;
  bool enable_incognito_on_install_;
  Extension::Location install_source_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, AddPendingExtensionFromSync);
};

// A PendingExtensionMap is a map from IDs of pending extensions to
// their info.
typedef std::map<std::string, PendingExtensionInfo> PendingExtensionMap;

#endif  // CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_INFO_H_
