// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/default_user_images.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "grit/theme_resources.h"

namespace chromeos {

namespace {

const char kDefaultPathPrefix[] = "default:";
const int kDefaultPathPrefixLength = arraysize(kDefaultPathPrefix) - 1;

const char* kOldDefaultImageNames[] = {
  "default:gray",
  "default:green",
  "default:blue",
  "default:yellow",
  "default:red",
};

}  // namespace

std::string GetDefaultImagePath(int index) {
  if (index < 0 || index >= kDefaultImagesCount) {
    NOTREACHED();
    return std::string();
  }
  return StringPrintf("default:%d", index);
}

// Checks if given path is one of the default ones. If it is, returns true
// and its index in kDefaultImageNames through |image_id|. If not, returns
// false.
bool IsDefaultImagePath(const std::string& path, int* image_id) {
  DCHECK(image_id);
  if (!StartsWithASCII(path, kDefaultPathPrefix, true))
    return false;

  int image_index = -1;
  if (base::StringToInt(path.begin() + kDefaultPathPrefixLength,
                        path.end(),
                        &image_index)) {
    if (image_index < 0 || image_index >= kDefaultImagesCount)
      return false;
    *image_id = image_index;
    return true;
  }

  // Check old default image names for back-compatibility.
  for (size_t i = 0; i < arraysize(kOldDefaultImageNames); ++i) {
    if (path == kOldDefaultImageNames[i]) {
      *image_id = static_cast<int>(i);
      return true;
    }
  }
  return false;
}

// Resource IDs of default user images.
const int kDefaultImageResources[] = {
  IDR_LOGIN_DEFAULT_USER,
  IDR_LOGIN_DEFAULT_USER_1,
  IDR_LOGIN_DEFAULT_USER_2,
  IDR_LOGIN_DEFAULT_USER_3,
  IDR_LOGIN_DEFAULT_USER_4
};

const int kDefaultImagesCount = arraysize(kDefaultImageResources);

}  // namespace chromeos

