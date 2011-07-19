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
const char kDefaultUrlPrefix[] = "chrome://theme/IDR_LOGIN_DEFAULT_USER_";
const char kFirstDefaultUrl[] = "chrome://theme/IDR_LOGIN_DEFAULT_USER";

const char* kOldDefaultImageNames[] = {
  "default:gray",
  "default:green",
  "default:blue",
  "default:yellow",
  "default:red",
};

// Returns a string consisting of the prefix specified and the index of the
// image if its valid.
std::string GetDefaultImageString(int index, const std::string& prefix) {
  if (index < 0 || index >= kDefaultImagesCount) {
    NOTREACHED();
    return std::string();
  }
  return StringPrintf("%s%d", prefix.c_str(), index);
}

// Returns true if the string specified consists of the prefix and one of
// the default images indices. Returns the index of the image in |image_id|
// variable.
bool IsDefaultImageString(const std::string& s,
                          const std::string& prefix,
                          int* image_id) {
  DCHECK(image_id);
  if (!StartsWithASCII(s, prefix, true))
    return false;

  int image_index = -1;
  if (base::StringToInt(s.begin() + prefix.length(),
                        s.end(),
                        &image_index)) {
    if (image_index < 0 || image_index >= kDefaultImagesCount)
      return false;
    *image_id = image_index;
    return true;
  }

  return false;
}
}  // namespace

std::string GetDefaultImagePath(int index) {
  return GetDefaultImageString(index, kDefaultPathPrefix);
}

bool IsDefaultImagePath(const std::string& path, int* image_id) {
  if (IsDefaultImageString(path, kDefaultPathPrefix, image_id))
    return true;

  // Check old default image names for back-compatibility.
  for (size_t i = 0; i < arraysize(kOldDefaultImageNames); ++i) {
    if (path == kOldDefaultImageNames[i]) {
      *image_id = static_cast<int>(i);
      return true;
    }
  }
  return false;
}

std::string GetDefaultImageUrl(int index) {
  if (index == 0)
    return kFirstDefaultUrl;
  return GetDefaultImageString(index, kDefaultUrlPrefix);
}

bool IsDefaultImageUrl(const std::string url, int* image_id) {
  if (url == kFirstDefaultUrl) {
    *image_id = 0;
    return true;
  }
  return IsDefaultImageString(url, kDefaultUrlPrefix, image_id);
}

// Resource IDs of default user images.
const int kDefaultImageResources[] = {
  IDR_LOGIN_DEFAULT_USER,
  IDR_LOGIN_DEFAULT_USER_1,
  IDR_LOGIN_DEFAULT_USER_2,
  IDR_LOGIN_DEFAULT_USER_3,
  IDR_LOGIN_DEFAULT_USER_4,
  IDR_LOGIN_DEFAULT_USER_5,
  IDR_LOGIN_DEFAULT_USER_6,
  IDR_LOGIN_DEFAULT_USER_7,
  IDR_LOGIN_DEFAULT_USER_8,
  IDR_LOGIN_DEFAULT_USER_9,
  IDR_LOGIN_DEFAULT_USER_10,
  IDR_LOGIN_DEFAULT_USER_11,
  IDR_LOGIN_DEFAULT_USER_12,
  IDR_LOGIN_DEFAULT_USER_13,
  IDR_LOGIN_DEFAULT_USER_14,
  IDR_LOGIN_DEFAULT_USER_15,
  IDR_LOGIN_DEFAULT_USER_16,
  IDR_LOGIN_DEFAULT_USER_17,
  IDR_LOGIN_DEFAULT_USER_18,
};

const int kDefaultImagesCount = arraysize(kDefaultImageResources);

}  // namespace chromeos

