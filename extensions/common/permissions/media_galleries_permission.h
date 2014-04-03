// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_PERMISSIONS_MEDIA_GALLERIES_PERMISSION_H_
#define EXTENSIONS_COMMON_PERMISSIONS_MEDIA_GALLERIES_PERMISSION_H_

#include "base/basictypes.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/media_galleries_permission_data.h"
#include "extensions/common/permissions/set_disjunction_permission.h"

namespace extensions {

// Media Galleries permissions are as follows:
//   <media-galleries-permission-pattern>
//             := <access> | <access> 'allAutoDetected' | 'allAutoDetected' |
//                <access> 'scan' | 'scan'
//   <access>  := 'read' | 'read' <access> | 'read' <secondary-access>
//   <secondary-access>
//             := 'delete' | 'delete' <secondary-access> |
//                'delete' <tertiary-access>
//   <tertiary-access>
//             := 'copyTo' | 'copyTo' <tertiary-access>
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
  // permissions do not contain unknown values.
  virtual bool FromValue(
      const base::Value* value,
      std::string* error,
      std::vector<std::string>* unhandled_permissions) OVERRIDE;

  // APIPermission overrides.
  virtual PermissionMessages GetMessages() const OVERRIDE;

  // Permission strings.
  static const char kAllAutoDetectedPermission[];
  static const char kScanPermission[];
  static const char kReadPermission[];
  static const char kCopyToPermission[];
  static const char kDeletePermission[];
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_PERMISSIONS_MEDIA_GALLERIES_PERMISSION_H_
