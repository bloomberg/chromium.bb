// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_INFO_H_
#define CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_INFO_H_
#pragma once

#include "base/version.h"
#include "chrome/common/extensions/extension.h"
#include "googleurl/src/gurl.h"

// A pending extension is an extension that hasn't been installed yet
// and is intended to be installed in the next auto-update cycle.  The
// update URL of a pending extension may be blank, in which case a
// default one is assumed.
// TODO(skerner): Make this class an implementation detail of
// PendingExtensionManager, and remove all other users.
class PendingExtensionInfo {
 public:
  typedef bool (*ShouldAllowInstallPredicate)(const Extension&);

  PendingExtensionInfo(
      const GURL& update_url,
      const Version& version,
      ShouldAllowInstallPredicate should_allow_install,
      bool is_from_sync,
      bool install_silently,
      Extension::Location install_source);

  // Required for STL container membership.  Should not be used directly.
  PendingExtensionInfo();

  const GURL& update_url() const { return update_url_; }
  const Version& version() const { return version_; }

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
  Extension::Location install_source() const { return install_source_; }

 private:
  GURL update_url_;
  Version version_;

  // When the extension is about to be installed, this function is
  // called.  If this function returns true, the install proceeds.  If
  // this function returns false, the install is aborted.
  ShouldAllowInstallPredicate should_allow_install_;

  bool is_from_sync_;  // This update check was initiated from sync.
  bool install_silently_;
  Extension::Location install_source_;

  FRIEND_TEST_ALL_PREFIXES(ExtensionServiceTest, AddPendingExtensionFromSync);
};

#endif  // CHROME_BROWSER_EXTENSIONS_PENDING_EXTENSION_INFO_H_
