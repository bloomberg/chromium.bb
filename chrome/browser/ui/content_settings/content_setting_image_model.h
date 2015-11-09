// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model_delegate.h"
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

  // Generates a vector of all image models to be used within one window.
  static ScopedVector<ContentSettingImageModel>
      GenerateContentSettingImageModels();

  // Notifies this model that its setting might have changed and it may need to
  // update its visibility, icon and tooltip.
  virtual void UpdateFromWebContents(content::WebContents* web_contents) = 0;

  // Creates the model for the bubble that will be attached to this image.
  // The bubble model is owned by the caller.
  virtual ContentSettingBubbleModel* CreateBubbleModel(
      ContentSettingBubbleModel::Delegate* delegate,
      content::WebContents* web_contents,
      Profile* profile) = 0;

  // Whether the animation should be run for the given |web_contents|.
  virtual bool ShouldRunAnimation(content::WebContents* web_contents) = 0;

  // Remembers that the animation has already run for the given |web_contents|,
  // so that we do not restart it when the parent view is updated.
  virtual void SetAnimationHasRun(content::WebContents* web_contents) = 0;

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
  ContentSettingImageModel();
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
  bool is_visible_;
  int icon_id_;
  gfx::Image icon_;
  int explanatory_string_id_;
  std::string tooltip_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingImageModel);
};

// A subclass for an image model tied to a single content type.
class ContentSettingSimpleImageModel : public ContentSettingImageModel {
 public:
  explicit ContentSettingSimpleImageModel(ContentSettingsType content_type);

  // ContentSettingImageModel implementation.
  ContentSettingBubbleModel* CreateBubbleModel(
      ContentSettingBubbleModel::Delegate* delegate,
      content::WebContents* web_contents,
      Profile* profile) override;
  bool ShouldRunAnimation(content::WebContents* web_contents) override;
  void SetAnimationHasRun(content::WebContents* web_contents) override;

  // Factory method. Used only for testing.
  static scoped_ptr<ContentSettingImageModel> CreateForContentTypeForTesting(
      ContentSettingsType content_type);

 protected:
  ContentSettingsType content_type() { return content_type_; }

 private:
  ContentSettingsType content_type_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingSimpleImageModel);
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_IMAGE_MODEL_H_
