// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PERMISSION_MANAGER_H_
#define CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PERMISSION_MANAGER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/shell/browser/shell_permission_manager.h"

namespace content {

class LayoutTestPermissionManager : public ShellPermissionManager {
 public:
  LayoutTestPermissionManager();
  ~LayoutTestPermissionManager() override;

  // ShellPermissionManager overrides.
  void RequestPermission(
      PermissionType permission,
      WebContents* web_contents,
      int request_id,
      const GURL& requesting_origin,
      bool user_gesture,
      const base::Callback<void(PermissionStatus)>& callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(LayoutTestPermissionManager);
};

}  // namespace content

#endif // CONTENT_SHELL_BROWSER_LAYOUT_TEST_LAYOUT_TEST_PERMISSION_MANAGER_H_
