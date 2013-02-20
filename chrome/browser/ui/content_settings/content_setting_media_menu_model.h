// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_MEDIA_MENU_MODEL_H_
#define CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_MEDIA_MENU_MODEL_H_

#include <map>

#include "base/callback.h"
#include "content/public/common/media_stream_request.h"
#include "ui/base/models/simple_menu_model.h"

class ContentSettingBubbleModel;

// A menu model that builds the contents of the media capture devices menu in
// the content setting bubble.
class ContentSettingMediaMenuModel : public ui::SimpleMenuModel,
                                     public ui::SimpleMenuModel::Delegate {
 public:
  // Callback to update the label of the menu in the UI.
  typedef base::Callback<void(content::MediaStreamType, const std::string&)>
      MenuLabelChangedCallback;

  ContentSettingMediaMenuModel(
      content::MediaStreamType type,
      ContentSettingBubbleModel* bubble_model,
      const MenuLabelChangedCallback& callback);
  virtual ~ContentSettingMediaMenuModel();

  // ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  typedef std::map<int, content::MediaStreamDevice> CommandMap;

  content::MediaStreamType type_;
  ContentSettingBubbleModel* media_bubble_model_;  // Weak.
  MenuLabelChangedCallback callback_;
  CommandMap commands_;

  DISALLOW_COPY_AND_ASSIGN(ContentSettingMediaMenuModel);
};

#endif  // CHROME_BROWSER_UI_CONTENT_SETTINGS_CONTENT_SETTING_MEDIA_MENU_MODEL_H_
