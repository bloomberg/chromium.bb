// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_PERMISSIONS_MEDIA_GALLERIES_PERMISSION_H_
#define CHROME_COMMON_EXTENSIONS_PERMISSIONS_MEDIA_GALLERIES_PERMISSION_H_

#include "base/basictypes.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/media_galleries_permission_data.h"
#include "chrome/common/extensions/permissions/set_disjunction_permission.h"

namespace extensions {

// Media Galleries permissions are as follows:
//   <media-galleries-permission-pattern>
//             := <access> | <access> 'allAutoDetected' | 'allAutoDetected'
//   <access>  := 'read' | 'write'
class MediaGalleriesPermission
  : public SetDisjunctionPermission<MediaGalleriesPermissionData,
                                    MediaGalleriesPermission> {
 public:
  struct CheckParam : public APIPermission::CheckParam {
    explicit CheckParam(const std::string& permission)
      : permission(permission) {}
    const std::string permission;
  };

  explicit MediaGalleriesPermission(const APIPermissionInfo* info);
  virtual ~MediaGalleriesPermission();

  // SetDisjunctionPermission overrides.
  // MediaGalleriesPermission does additional checks to make sure the
  // permissions do not have multiple access values.
  virtual bool FromValue(const base::Value* value) OVERRIDE;

  // APIPermission overrides.
  virtual PermissionMessages GetMessages() const OVERRIDE;

  // Permission strings.
  static const char kAllAutoDetectedPermission[];
  static const char kReadPermission[];
  static const char kWritePermission[];
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_PERMISSIONS_MEDIA_GALLERIES_PERMISSION_H_
