// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_H_

#include <list>

#include "base/macros.h"
#include "components/content_settings/core/common/content_settings_types.h"

class PermissionContextBase;
class Profile;
class KeyedServiceBaseFactory;

namespace content {
enum class PermissionType;
};  // namespace content

class PermissionContext {
 public:
  // Helper method returning the PermissionContextBase object associated with
  // the given ContentSettingsType.
  // This can return nullptr if the the permission type has no associated
  // context.
  static PermissionContextBase* Get(
      Profile* profile,
      content::PermissionType content_settings_type);

  // Return all the factories related to PermissionContext. These are the
  // factories used by ::Get() to create a PermissionContextBase.
  // This is meant to be used by callers of ::Get() that need to depend on these
  // factories.
  static const std::list<KeyedServiceBaseFactory*>& GetFactories();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PermissionContext);
};

#endif // CHROME_BROWSER_PERMISSIONS_PERMISSION_CONTEXT_H_
