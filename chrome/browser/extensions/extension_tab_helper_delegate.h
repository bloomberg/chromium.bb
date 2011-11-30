// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_HELPER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_HELPER_DELEGATE_H_
#pragma once

#include "base/basictypes.h"

class TabContentsWrapper;
struct WebApplicationInfo;

// Objects implement this interface to get notified about changes in the
// ExtensionTabHelper and to provide necessary functionality.
class ExtensionTabHelperDelegate {
 public:
  // Notification that a user's request to install an application has completed.
  virtual void OnDidGetApplicationInfo(TabContentsWrapper* source,
                                       int32 page_id);

  // Notification when an application programmatically requests installation.
  virtual void OnInstallApplication(TabContentsWrapper* source,
                                    const WebApplicationInfo& app_info);
 protected:
  virtual ~ExtensionTabHelperDelegate();
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_TAB_HELPER_DELEGATE_H_
