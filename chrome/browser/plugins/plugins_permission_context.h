// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGINS_PLUGINS_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_PLUGINS_PLUGINS_PERMISSION_CONTEXT_H_

#include "base/macros.h"
#include "chrome/browser/permissions/permission_context_base.h"

class PluginsPermissionContext : public PermissionContextBase {
 public:
  explicit PluginsPermissionContext(Profile* profile);
  ~PluginsPermissionContext() override;

 private:
  // PermissionContextBase:
  bool IsRestrictedToSecureOrigins() const override;

  DISALLOW_COPY_AND_ASSIGN(PluginsPermissionContext);
};

#endif  // CHROME_BROWSER_PLUGINS_PLUGINS_PERMISSION_CONTEXT_H_
