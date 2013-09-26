// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_HISTOGRAMS_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_HISTOGRAMS_H_

namespace media_galleries {

enum MediaGalleriesUsages {
  DIALOG_GALLERY_ADDED,
  DIALOG_PERMISSION_ADDED,
  DIALOG_PERMISSION_REMOVED,
  GET_MEDIA_FILE_SYSTEMS,
  PROFILES_WITH_USAGE,
  SHOW_DIALOG,
  SAVE_DIALOG,
  WEBUI_ADD_GALLERY,
  WEBUI_FORGET_GALLERY,
  MEDIA_GALLERIES_NUM_USAGES
};

void UsageCount(MediaGalleriesUsages usage);

}  // namespace media_galleries

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_MEDIA_GALLERIES_HISTOGRAMS_H_
