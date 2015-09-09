// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_H_

#include <string>

#include "base/basictypes.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "ui/gfx/image/image.h"

namespace content {
class WebContents;
}

namespace gfx {
enum class VectorIconId;
}

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
  virtual void UpdateFromWebContents(content::WebContents* web_contents) = 0;

  ContentSettingsType get_content_settings_type() const {
    return content_settings_type_;
  }
  bool is_visible() const { return is_visible_; }
  const gfx::Image& icon() const { return icon_; }
#if defined(OS_MACOSX)
  int icon_id() const { return icon_id_; }
#endif
  // Returns the resource ID of a string to show when the icon appears, or 0 if
  // we don't wish to show anything.
  int explanatory_string_id() const { return explanatory_string_id_; }
  std::string get_tooltip() const { return tooltip_; }

 protected:
  explicit ContentSettingImageModel(ContentSettingsType content_settings_type);
  void SetIconByResourceId(int id);
#if !defined(OS_MACOSX)
  void SetIconByVectorId(gfx::VectorIconId id, bool blocked);
#endif
  void set_visible(bool visible) { is_visible_ = visible; }
  void set_icon(const gfx::Image& image) { icon_ = image; }
  void set_explanatory_string_id(int text_id) {
    explanatory_string_id_ = text_id;
  }
  void set_tooltip(const std::string& tooltip) { tooltip_ = tooltip; }

 private:
  const ContentSettingsType content_settings_type_;
  bool is_visible_;
  int icon_id_;
  gfx::Image icon_;
  int explanatory_string_id_;
  std::string tooltip_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageModel);
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_H_
