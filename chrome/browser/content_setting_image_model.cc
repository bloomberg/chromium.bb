// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_image_model.h"

#include "app/l10n_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

class ContentSettingBlockedImageModel : public ContentSettingImageModel {
 public:
  explicit ContentSettingBlockedImageModel(
      ContentSettingsType content_settings_type)
      : ContentSettingImageModel(content_settings_type) {
  }

  virtual void UpdateFromTabContents(const TabContents* tab_contents) {
    if (tab_contents &&
        tab_contents->IsContentBlocked(get_content_settings_type())) {
      set_icon(kBlockedIconIDs[get_content_settings_type()]);
      set_tooltip(
          l10n_util::GetStringUTF8(kTooltipIDs[get_content_settings_type()]));
      set_visible(true);
    } else {
      set_visible(false);
    }
  }

 private:
  static const int kBlockedIconIDs[];
  static const int kTooltipIDs[];

};

ContentSettingImageModel::ContentSettingImageModel(
    ContentSettingsType content_settings_type)
    : content_settings_type_(content_settings_type),
      is_visible_(false),
      icon_(0) {
}

// static
ContentSettingImageModel* ContentSettingImageModel::
    CreateContentSettingImageModel(ContentSettingsType content_settings_type) {
  return new ContentSettingBlockedImageModel(content_settings_type);
}

const int ContentSettingBlockedImageModel::kBlockedIconIDs[] = {
    IDR_BLOCKED_COOKIES,
    IDR_BLOCKED_IMAGES,
    IDR_BLOCKED_JAVASCRIPT,
    IDR_BLOCKED_PLUGINS,
    IDR_BLOCKED_POPUPS,
};

const int ContentSettingBlockedImageModel::kTooltipIDs[] = {
  IDS_BLOCKED_COOKIES_TITLE,
  IDS_BLOCKED_IMAGES_TITLE,
  IDS_BLOCKED_JAVASCRIPT_TITLE,
  IDS_BLOCKED_PLUGINS_TITLE,
  IDS_BLOCKED_POPUPS_TOOLTIP,
};
