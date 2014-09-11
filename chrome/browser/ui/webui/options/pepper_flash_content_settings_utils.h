// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_PEPPER_FLASH_CONTENT_SETTINGS_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_PEPPER_FLASH_CONTENT_SETTINGS_UTILS_H_

#include <vector>

#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "ppapi/c/private/ppp_flash_browser_operations.h"
#include "ppapi/shared_impl/ppp_flash_browser_operations_shared.h"

namespace options {

struct MediaException {
  MediaException(const ContentSettingsPattern& in_pattern,
                 ContentSetting in_audio_setting,
                 ContentSetting in_video_setting);
  ~MediaException();

  bool operator==(const MediaException& other) const;

  ContentSettingsPattern pattern;
  ContentSetting audio_setting;
  ContentSetting video_setting;
};

typedef std::vector<MediaException> MediaExceptions;

class PepperFlashContentSettingsUtils {
 public:
  static ContentSetting FlashPermissionToContentSetting(
      PP_Flash_BrowserOperations_Permission permission);

  static void FlashSiteSettingsToMediaExceptions(
      const ppapi::FlashSiteSettings& site_settings,
      MediaExceptions* media_exceptions);

  // Sorts |media_exceptions| in ascending order by comparing the |pattern|
  // field of the elements.
  static void SortMediaExceptions(MediaExceptions* media_exceptions);

  // Checks whether |exceptions_1| and |exceptions_2| describe the same set of
  // exceptions. |exceptions_1| and |exceptions_2| should be sorted by
  // SortMediaExceptions() before passing into this method.
  //
  // When an element of |exceptions_1| has a pattern that doesn't match any
  // element of |exceptions_2|, it would be compared with |default_setting_2|,
  // and visa versa.
  //
  // |ignore_audio_setting| and |ignore_video_setting| specify whether to skip
  // comparison of the |audio_setting| and |video_setting| field of
  // MediaException, respectively.
  static bool AreMediaExceptionsEqual(ContentSetting default_setting_1,
                                      const MediaExceptions& exceptions_1,
                                      ContentSetting default_setting_2,
                                      const MediaExceptions& exceptions_2,
                                      bool ignore_audio_setting,
                                      bool ignore_video_setting);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_PEPPER_FLASH_CONTENT_SETTINGS_UTILS_H_
