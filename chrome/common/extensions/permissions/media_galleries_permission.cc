// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/media_galleries_permission.h"

#include <set>
#include <string>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/permissions/permissions_info.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

const char MediaGalleriesPermission::kAllAutoDetectedPermission[] =
    "allAutoDetected";
const char MediaGalleriesPermission::kReadPermission[] = "read";
const char MediaGalleriesPermission::kCopyToPermission[] = "copyTo";

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
  bool has_copy_to = false;
  for (std::set<MediaGalleriesPermissionData>::const_iterator it =
      data_set_.begin(); it != data_set_.end(); ++it) {
    if (it->permission() == kAllAutoDetectedPermission) {
      continue;
    }
    if (it->permission() == kReadPermission) {
      has_read = true;
      continue;
    }
    if (it->permission() == kCopyToPermission) {
      has_copy_to = true;
      continue;
    }

    // No other permissions, so reaching this means
    // MediaGalleriesPermissionData is probably out of sync in some way.
    // Fail so developers notice this.
    NOTREACHED();
    return false;
  }

  return has_read || !has_copy_to;
}

PermissionMessages MediaGalleriesPermission::GetMessages() const {
  DCHECK(HasMessages());
  PermissionMessages result;

  bool has_all_auto_detected = false;
  bool has_read = false;
  bool has_copy_to = false;

  for (std::set<MediaGalleriesPermissionData>::const_iterator it =
      data_set_.begin(); it != data_set_.end(); ++it) {
    if (it->permission() == kAllAutoDetectedPermission)
      has_all_auto_detected = true;
    else if (it->permission() == kReadPermission)
      has_read = true;
    else if (it->permission() == kCopyToPermission)
      has_copy_to = true;
  }

  if (has_copy_to && !has_read) {
    NOTREACHED();
    return result;
  }

  // If |has_all_auto_detected| is false, then Chrome will prompt the user at
  // runtime when the extension call the getMediaGalleries API.
  if (!has_all_auto_detected || !(has_read || has_copy_to))
    return result;

  // Separate PermissionMessage IDs for read and copyTo. Otherwise an extension
  // can silently gain new access capabilities.
  if (has_read) {
    result.push_back(PermissionMessage(
        PermissionMessage::kMediaGalleriesAllGalleriesRead,
        l10n_util::GetStringUTF16(
            IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ)));
    if (has_copy_to) {
      result.push_back(PermissionMessage(
          PermissionMessage::kMediaGalleriesAllGalleriesCopyTo,
          l10n_util::GetStringUTF16(
              IDS_EXTENSION_PROMPT_WARNING_MEDIA_GALLERIES_READ_WRITE)));
    }
  }
  return result;
}

}  // namespace extensions
