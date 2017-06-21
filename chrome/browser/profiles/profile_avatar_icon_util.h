// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "third_party/skia/include/core/SkColor.h"

namespace base {
class FilePath;
class ListValue;
}

namespace gfx {
class Image;
}

class GURL;
class SkBitmap;

namespace profiles {

// Avatar access.
extern const char kGAIAPictureFileName[];
extern const char kHighResAvatarFolderName[];

// Avatar formatting.
extern const int kAvatarIconWidth;
extern const int kAvatarIconHeight;
extern const SkColor kAvatarTutorialBackgroundColor;
extern const SkColor kAvatarTutorialContentTextColor;
extern const SkColor kAvatarBubbleAccountsBackgroundColor;
extern const SkColor kAvatarBubbleGaiaBackgroundColor;
extern const SkColor kUserManagerBackgroundColor;

// Avatar shape.
enum AvatarShape {
  SHAPE_CIRCLE,  // Only available for desktop platforms
  SHAPE_SQUARE,
};

// Returns a version of |image| of a specific size. Note that no checks are
// done on the width/height so make sure they're reasonable values; in the
// range of 16-256 is probably best.
gfx::Image GetSizedAvatarIcon(const gfx::Image& image,
                              bool is_rectangle,
                              int width,
                              int height,
                              AvatarShape shape);

gfx::Image GetSizedAvatarIcon(const gfx::Image& image,
                              bool is_rectangle,
                              int width,
                              int height);

// Returns a version of |image| suitable for use in menus.
gfx::Image GetAvatarIconForMenu(const gfx::Image& image,
                                bool is_rectangle);

// Returns a version of |image| suitable for use in WebUI.
gfx::Image GetAvatarIconForWebUI(const gfx::Image& image,
                                 bool is_rectangle);

// Returns a version of |image| suitable for use in title bars. The returned
// image is scaled to fit |dst_width| and |dst_height|.
gfx::Image GetAvatarIconForTitleBar(const gfx::Image& image,
                                    bool is_rectangle,
                                    int dst_width,
                                    int dst_height);

// Returns a bitmap with a couple of columns shaved off so it is more square,
// so that when resized to a square aspect ratio it looks pretty.
SkBitmap GetAvatarIconAsSquare(const SkBitmap& source_bitmap, int scale_factor);

// Gets the number of default avatar icons that exist.
size_t GetDefaultAvatarIconCount();

// Gets the number of generic avatar icons that exist.
size_t GetGenericAvatarIconCount();

// Gets the index for the (grey silhouette) avatar used as a placeholder.
size_t GetPlaceholderAvatarIndex();

// Gets the resource ID of the placeholder avatar icon.
int GetPlaceholderAvatarIconResourceID();

// Returns a URL for the placeholder avatar icon.
std::string GetPlaceholderAvatarIconUrl();

// Gets the resource ID of the default avatar icon at |index|.
int GetDefaultAvatarIconResourceIDAtIndex(size_t index);

// Gets the resource filename of the default avatar icon at |index|.
const char* GetDefaultAvatarIconFileNameAtIndex(size_t index);

// Gets the resource ID of the default avatar label at |index|.
int GetDefaultAvatarLabelResourceIDAtIndex(size_t index);

// Gets the full path of the high res avatar icon at |index|.
base::FilePath GetPathOfHighResAvatarAtIndex(size_t index);

// Returns a URL for the default avatar icon with specified index.
std::string GetDefaultAvatarIconUrl(size_t index);

// Checks if |index| is a valid avatar icon index
bool IsDefaultAvatarIconIndex(size_t index);

// Checks if the given URL points to one of the default avatar icons. If it
// is, returns true and its index through |icon_index|. If not, returns false.
bool IsDefaultAvatarIconUrl(const std::string& icon_url, size_t *icon_index);

// Given an image URL this function builds a new URL, appending passed
// |image_size| and |no_silhouette| parameters. |old_url| must be valid.
// For example, if |image_size| was set to 256, |no_silhouette| was set to
// true and |old_url| was either:
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/photo.jpg
//   or
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s64-c-ns/photo.jpg
// then return value would be:
//   https://example.com/--Abc/AAAAAAAAAAI/AAAAAAAAACQ/Efg/s256-c-ns/photo.jpg
GURL GetImageURLWithOptions(const GURL& old_url,
                            int image_size,
                            bool no_silhouette);

// Returns a list of dictionaries containing the default profile avatar icons as
// well as avatar labels used for accessibility purposes. The list is ordered
// according to the avatars' default order.
std::unique_ptr<base::ListValue> GetDefaultProfileAvatarIconsAndLabels();

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_
