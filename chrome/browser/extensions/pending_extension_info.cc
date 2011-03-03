// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/pending_extension_info.h"

PendingExtensionInfo::PendingExtensionInfo(
    const GURL& update_url,
    ShouldAllowInstallPredicate should_allow_install,
    bool is_from_sync,
    bool install_silently,
    bool enable_on_install,
    bool enable_incognito_on_install,
    Extension::Location install_source)
    : update_url_(update_url),
      should_allow_install_(should_allow_install),
      is_from_sync_(is_from_sync),
      install_silently_(install_silently),
      enable_on_install_(enable_on_install),
      enable_incognito_on_install_(enable_incognito_on_install),
      install_source_(install_source) {}

PendingExtensionInfo::PendingExtensionInfo()
    : update_url_(),
      should_allow_install_(NULL),
      is_from_sync_(true),
      install_silently_(false),
      enable_on_install_(false),
      enable_incognito_on_install_(false),
      install_source_(Extension::INVALID) {}
