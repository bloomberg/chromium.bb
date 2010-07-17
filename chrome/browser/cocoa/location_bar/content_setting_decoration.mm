// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar/content_setting_decoration.h"

#include "app/resource_bundle.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/content_blocked_bubble_controller.h"
#import "chrome/browser/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/content_setting_image_model.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "net/base/net_util.h"

ContentSettingDecoration::ContentSettingDecoration(
    ContentSettingsType settings_type,
    LocationBarViewMac* owner,
    Profile* profile)
    : content_setting_image_model_(
          ContentSettingImageModel::CreateContentSettingImageModel(
              settings_type)),
      owner_(owner),
      profile_(profile) {
}

ContentSettingDecoration::~ContentSettingDecoration() {
}

void ContentSettingDecoration::UpdateFromTabContents(
    const TabContents* tab_contents) {
  content_setting_image_model_->UpdateFromTabContents(tab_contents);
  SetVisible(content_setting_image_model_->is_visible());
  if (IsVisible()) {
    // TODO(thakis): We should use pdfs for these icons on OSX.
    // http://crbug.com/35847
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SetImage(rb.GetNSImageNamed(content_setting_image_model_->get_icon()));
    SetToolTip(base::SysUTF8ToNSString(
        content_setting_image_model_->get_tooltip()));
  }
}

bool ContentSettingDecoration::OnMousePressed(NSRect frame) {
  // Get host. This should be shared on linux/win/osx medium-term.
  TabContents* tabContents =
      BrowserList::GetLastActive()->GetSelectedTabContents();
  if (!tabContents)
    return true;

  GURL url = tabContents->GetURL();
  std::wstring displayHost;
  net::AppendFormattedHost(
      url,
      UTF8ToWide(profile_->GetPrefs()->GetString(prefs::kAcceptLanguages)),
      &displayHost, NULL, NULL);

  // Find point for bubble's arrow in screen coordinates.
  // TODO(shess): |owner_| is only being used to fetch |field|.
  // Consider passing in |control_view|.
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  frame = GetDrawRectInFrame(frame);
  NSPoint anchor = NSMakePoint(NSMidX(frame) + 1, NSMaxY(frame));
  anchor = [field convertPoint:anchor toView:nil];
  anchor = [[field window] convertBaseToScreen:anchor];

  // Open bubble.
  ContentSettingBubbleModel* model =
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          tabContents, profile_,
          content_setting_image_model_->get_content_settings_type());
  [ContentBlockedBubbleController showForModel:model
                                   parentWindow:[field window]
                                     anchoredAt:anchor];
  return true;
}

NSString* ContentSettingDecoration::GetToolTip() {
  return tooltip_.get();
}

void ContentSettingDecoration::SetToolTip(NSString* tooltip) {
  tooltip_.reset([tooltip retain]);
}
