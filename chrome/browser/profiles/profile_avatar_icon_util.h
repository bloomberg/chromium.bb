// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image.h"

namespace profiles {

// Avatar access.
extern const char kGAIAPictureFileName[];
extern const char kHighResAvatarFolderName[];

// Avatar formatting.
extern const int kAvatarIconWidth;
extern const int kAvatarIconHeight;
extern const int kAvatarIconPadding;
extern const SkColor kAvatarTutorialBackgroundColor;
extern const SkColor kAvatarTutorialContentTextColor;

// Gets the number of default avatar icons that exist.
size_t GetDefaultAvatarIconCount();

// Gets the number of generic avatar icons that exist.
size_t GetGenericAvatarIconCount();

// Gets the resource ID of the default avatar icon at |index|.
int GetDefaultAvatarIconResourceIDAtIndex(size_t index);

// Gets the resource filename of the default avatar icon at |index|.
const char* GetDefaultAvatarIconFileNameAtIndex(size_t index);

// Returns a URL for the default avatar icon with specified index.
std::string GetDefaultAvatarIconUrl(size_t index);

// Checks if |index| is a valid avatar icon index
bool IsDefaultAvatarIconIndex(size_t index);

// Checks if the given URL points to one of the default avatar icons. If it
// is, returns true and its index through |icon_index|. If not, returns false.
bool IsDefaultAvatarIconUrl(const std::string& icon_url, size_t *icon_index);
// Returns a version of |image| of a specific size and with a grey border.
// Note that no checks are done on the width/height so make sure they're
// reasonable values; in the range of 16-256 is probably best.
gfx::Image GetSizedAvatarIconWithBorder(const gfx::Image& image,
                                        bool is_rectangle,
                                        int width, int height);

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

}  // namespace profiles

#endif  // CHROME_BROWSER_PROFILES_PROFILE_AVATAR_ICON_UTIL_H_
