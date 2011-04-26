// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_USER_IMAGES_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_USER_IMAGES_H_

#include "grit/theme_resources.h"

namespace chromeos {

namespace {

// Special pathes to default user images used in Local State file.
const char* kDefaultImageNames[] = {
  "default:gray",
  "default:green",
  "default:blue",
  "default:yellow",
  "default:red",
};

// Resource IDs of default user images.
const int kDefaultImageResources[] = {
  IDR_LOGIN_DEFAULT_USER,
  IDR_LOGIN_DEFAULT_USER_1,
  IDR_LOGIN_DEFAULT_USER_2,
  IDR_LOGIN_DEFAULT_USER_3,
  IDR_LOGIN_DEFAULT_USER_4
};

}  // namespace

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_USER_IMAGES_H_
