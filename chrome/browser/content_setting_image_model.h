// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTENT_SETTING_IMAGE_MODEL_H_
#define CHROME_BROWSER_CONTENT_SETTING_IMAGE_MODEL_H_
#pragma once

#include <string>

#include "chrome/common/content_settings_types.h"

class TabContents;

// This model provides data (icon ids and tooltip) for the content setting icons
// that are displayed in the location bar.
class ContentSettingImageModel {
 public:
  virtual ~ContentSettingImageModel() {}

  // Factory function.
  static ContentSettingImageModel* CreateContentSettingImageModel(
     ContentSettingsType content_settings_type);

  // Notifies this model that its setting might have changed and it may need to
  // update its visibility, icon and tooltip.
  virtual void UpdateFromTabContents(const TabContents* tab_contents) = 0;

  ContentSettingsType get_content_settings_type() const {
    return content_settings_type_;
  }
  bool is_visible() const { return is_visible_; }
  int get_icon() const { return icon_; }
  std::string get_tooltip() const { return tooltip_; }

 protected:
  explicit ContentSettingImageModel(ContentSettingsType content_settings_type);
  void set_visible(bool visible) { is_visible_ = visible; }
  void set_icon(int icon) { icon_ = icon; }
  void set_tooltip(const std::string& tooltip) { tooltip_ = tooltip; }

 private:
  const ContentSettingsType content_settings_type_;
  bool is_visible_;
  int icon_;
  std::string tooltip_;
};

#endif  // CHROME_BROWSER_CONTENT_SETTING_IMAGE_MODEL_H_
