// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_

#include <stddef.h>

#include <string>

#include "base/strings/string16.h"
#include "chromeos/chromeos_export.h"

namespace gfx {
class ImageSkia;
}

namespace chromeos {
namespace default_user_image {

// Returns URL to default user image with specified index.
CHROMEOS_EXPORT std::string GetDefaultImageUrl(int index);

// Checks if the given URL points to one of the default images. If it is,
// returns true and its index through |image_id|. If not, returns false.
CHROMEOS_EXPORT bool IsDefaultImageUrl(const std::string& url, int* image_id);

// Returns bitmap of default user image with specified index.
CHROMEOS_EXPORT const gfx::ImageSkia& GetDefaultImage(int index);

// Returns a description of a default user image with specified index.
CHROMEOS_EXPORT base::string16 GetDefaultImageDescription(int index);

// Resource IDs of default user images.
CHROMEOS_EXPORT extern const int kDefaultImageResourceIDs[];

// String IDs of author names for default user images.
CHROMEOS_EXPORT extern const int kDefaultImageAuthorIDs[];

// String IDs of websites for default user images.
CHROMEOS_EXPORT extern const int kDefaultImageWebsiteIDs[];

// Number of default images.
CHROMEOS_EXPORT extern const int kDefaultImagesCount;

// The starting index of default images available for selection. Note that
// existing users may have images with smaller indices.
CHROMEOS_EXPORT extern const int kFirstDefaultImageIndex;

/// Histogram values. ////////////////////////////////////////////////////////

// Histogram value for user image taken from file.
CHROMEOS_EXPORT extern const int kHistogramImageFromFile;

// Histogram value for user image taken from camera.
CHROMEOS_EXPORT extern const int kHistogramImageFromCamera;

// Histogram value a previously used image from camera/file.
CHROMEOS_EXPORT extern const int kHistogramImageOld;

// Histogram value for user image from G+ profile.
CHROMEOS_EXPORT extern const int kHistogramImageFromProfile;

// Histogram value for user video (animated avatar) from camera.
CHROMEOS_EXPORT extern const int kHistogramVideoFromCamera;

// Histogram value for user video from file.
CHROMEOS_EXPORT extern const int kHistogramVideoFromFile;

// Number of possible histogram values for user images.
CHROMEOS_EXPORT extern const int kHistogramImagesCount;

// Returns the histogram value corresponding to the given default image index.
CHROMEOS_EXPORT int GetDefaultImageHistogramValue(int index);

}  // namespace default_user_image
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USERS_DEFAULT_USER_IMAGE_DEFAULT_USER_IMAGES_H_
