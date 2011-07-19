// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once
#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_USER_IMAGES_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_USER_IMAGES_H_

#include <cstddef>  // for size_t
#include <string>

namespace chromeos {

// Returns path to default user image with specified index.
// The path is used in Local State to distinguish default images.
std::string GetDefaultImagePath(int index);

// Checks if given path is one of the default ones. If it is, returns true
// and its index through |image_id|. If not, returns false.
bool IsDefaultImagePath(const std::string& path, int* image_id);

// Returns URL to default user image with specifided index.
std::string GetDefaultImageUrl(int index);

// Checks if the given URL points to one of the default images. If it is,
// returns true and its index through |image_id|. If not, returns false.
bool IsDefaultImageUrl(const std::string url, int* image_id);

// Resource IDs of default user images.
extern const int kDefaultImageResources[];

// Number of default images.
extern const int kDefaultImagesCount;

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_USER_IMAGES_H_
