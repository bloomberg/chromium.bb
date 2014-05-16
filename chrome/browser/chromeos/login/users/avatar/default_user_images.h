// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_DEFAULT_USER_IMAGES_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_DEFAULT_USER_IMAGES_H_

#include <cstddef>  // for size_t
#include <string>

#include "base/strings/string16.h"

namespace gfx {
class ImageSkia;
}

namespace chromeos {

// Returns path to default user image with specified index.
// The path is used in Local State to distinguish default images.
// This function is obsolete and is preserved only for compatibility with older
// profiles which don't user separate image index and path.
std::string GetDefaultImagePath(int index);

// Checks if given path is one of the default ones. If it is, returns true
// and its index through |image_id|. If not, returns false.
bool IsDefaultImagePath(const std::string& path, int* image_id);

// Returns URL to default user image with specified index.
std::string GetDefaultImageUrl(int index);

// Checks if the given URL points to one of the default images. If it is,
// returns true and its index through |image_id|. If not, returns false.
bool IsDefaultImageUrl(const std::string& url, int* image_id);

// Returns bitmap of default user image with specified index.
const gfx::ImageSkia& GetDefaultImage(int index);

// Returns a description of a default user image with specified index.
base::string16 GetDefaultImageDescription(int index);

// Resource IDs of default user images.
extern const int kDefaultImageResourceIDs[];

// String IDs of author names for default user images.
extern const int kDefaultImageAuthorIDs[];

// String IDs of websites for default user images.
extern const int kDefaultImageWebsiteIDs[];

// Number of default images.
extern const int kDefaultImagesCount;

// The starting index of default images available for selection. Note that
// existing users may have images with smaller indices.
extern const int kFirstDefaultImageIndex;

/// Histogram values. ////////////////////////////////////////////////////////

// Histogram value for user image taken from file.
extern const int kHistogramImageFromFile;

// Histogram value for user image taken from camera.
extern const int kHistogramImageFromCamera;

// Histogram value a previously used image from camera/file.
extern const int kHistogramImageOld;

// Histogram value for user image from G+ profile.
extern const int kHistogramImageFromProfile;

// Histogram value for user video (animated avatar) from camera.
extern const int kHistogramVideoFromCamera;

// Histogram value for user video from file.
extern const int kHistogramVideoFromFile;

// Number of possible histogram values for user images.
extern const int kHistogramImagesCount;

// Returns the histogram value corresponding to the given default image index.
int GetDefaultImageHistogramValue(int index);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_AVATAR_DEFAULT_USER_IMAGES_H_
