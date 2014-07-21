// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_MANAGER_USER_IMAGE_DEFAULT_USER_IMAGES_H_
#define COMPONENTS_USER_MANAGER_USER_IMAGE_DEFAULT_USER_IMAGES_H_

#include <cstddef>  // for size_t
#include <string>

#include "base/strings/string16.h"
#include "components/user_manager/user_manager_export.h"

namespace gfx {
class ImageSkia;
}

namespace user_manager {

// Returns URL to default user image with specified index.
USER_MANAGER_EXPORT std::string GetDefaultImageUrl(int index);

// Checks if the given URL points to one of the default images. If it is,
// returns true and its index through |image_id|. If not, returns false.
USER_MANAGER_EXPORT bool IsDefaultImageUrl(const std::string& url,
                                           int* image_id);

// Returns bitmap of default user image with specified index.
USER_MANAGER_EXPORT const gfx::ImageSkia& GetDefaultImage(int index);

// Returns a description of a default user image with specified index.
USER_MANAGER_EXPORT base::string16 GetDefaultImageDescription(int index);

// Resource IDs of default user images.
USER_MANAGER_EXPORT extern const int kDefaultImageResourceIDs[];

// String IDs of author names for default user images.
USER_MANAGER_EXPORT extern const int kDefaultImageAuthorIDs[];

// String IDs of websites for default user images.
USER_MANAGER_EXPORT extern const int kDefaultImageWebsiteIDs[];

// Number of default images.
USER_MANAGER_EXPORT extern const int kDefaultImagesCount;

// The starting index of default images available for selection. Note that
// existing users may have images with smaller indices.
USER_MANAGER_EXPORT extern const int kFirstDefaultImageIndex;

/// Histogram values. ////////////////////////////////////////////////////////

// Histogram value for user image taken from file.
USER_MANAGER_EXPORT extern const int kHistogramImageFromFile;

// Histogram value for user image taken from camera.
USER_MANAGER_EXPORT extern const int kHistogramImageFromCamera;

// Histogram value a previously used image from camera/file.
USER_MANAGER_EXPORT extern const int kHistogramImageOld;

// Histogram value for user image from G+ profile.
USER_MANAGER_EXPORT extern const int kHistogramImageFromProfile;

// Histogram value for user video (animated avatar) from camera.
USER_MANAGER_EXPORT extern const int kHistogramVideoFromCamera;

// Histogram value for user video from file.
USER_MANAGER_EXPORT extern const int kHistogramVideoFromFile;

// Number of possible histogram values for user images.
USER_MANAGER_EXPORT extern const int kHistogramImagesCount;

// Returns the histogram value corresponding to the given default image index.
USER_MANAGER_EXPORT int GetDefaultImageHistogramValue(int index);

}  // namespace user_manager

#endif  // COMPONENTS_USER_MANAGER_USER_IMAGE_DEFAULT_USER_IMAGES_H_
