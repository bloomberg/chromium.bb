// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/media_galleries_permission.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/extensions/permissions/permissions_info.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

const char MediaGalleriesPermission::kAllAutoDetectedPermission[] =
    "allAutoDetected";
const char MediaGalleriesPermission::kReadPermission[] = "read";
const char MediaGalleriesPermission::kWritePermission[] = "write";

MediaGalleriesPermission::MediaGalleriesPermission(
    const APIPermissionInfo* info)
  : SetDisjunctionPermission<MediaGalleriesPermissionData,
                             MediaGalleriesPermission>(info) {
}

MediaGalleriesPermission::~MediaGalleriesPermission() {
}

bool MediaGalleriesPermission::FromValue(const base::Value* value) {
  if (!SetDisjunctionPermission<MediaGalleriesPermissionData,
                                MediaGalleriesPermission>::FromValue(value)) {
    return false;
  }

  bool has_read = false;
  bool has_write = false;
  for (std::set<MediaGalleriesPermissionData>::const_iterator it =
      data_set_.begin(); it != data_set_.end(); ++it) {
    if (it->permission() == kReadPermission) {
      has_read = true;
    } else if (it->permission() == kWritePermission) {
      has_write = true;
    } else if (it->permission() == kAllAutoDetectedPermission) {
      continue;
    } else {
      // No other permissions, so reaching this means
      // MediaGalleriesPermissionData is probably out of sync in some way.
      // Fail so developers notice this.
      NOTREACHED();
      return false;
    }
  }

  // Read and write permissions are mutually exclusive.
  return (!has_read || !has_write);
}

PermissionMessages MediaGalleriesPermission::GetMessages() const {
  DCHECK(HasMessages());
  PermissionMessages result;

  bool has_all_auto_detected = false;
  bool has_read = false;
  bool has_write = false;

  for (std::set<MediaGalleriesPermissionData>::const_iterator it =
      data_set_.begin(); it != data_set_.end(); ++it) {
    if (it->permission() == kAllAutoDetectedPermission)
      has_all_auto_detected = true;
    else if (it->permission() == kReadPermission)
      has_read = true;
    else if (it->permission() == kWritePermission)
      has_write = true;
  }
  // FromValue() should never allow this.
  DCHECK(!has_read || !has_write);

  // If |has_all_auto_detected| is false, then Chrome will prompt the user at
  // runtime when the extension call the getMediaGalleries API.
  if (!has_all_auto_detected || !(has_read || has_write))
    return result;

  // Separate PermissionMessage IDs for read and write. Otherwise an extension
  // can silently gain new access capabilities.
  PermissionMessage::ID permission_id = has_write ?
      PermissionMessage::kMediaGalleriesAllGalleriesWrite :
      PermissionMessage::kMediaGalleriesAllGalleriesRead;
  int message_id = has_write ?
      IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE_ALL_GALLERIES :
      IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_ALL_GALLERIES;
  result.push_back(
      PermissionMessage(permission_id, l10n_util::GetStringUTF16(message_id)));
  return result;
}

}  // namespace extensions
