// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar/content_setting_decoration.h"

#include "app/resource_bundle.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#import "chrome/browser/cocoa/content_setting_bubble_cocoa.h"
#import "chrome/browser/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/content_setting_image_model.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/pref_names.h"
#include "net/base/net_util.h"

namespace {

// How far to offset up from the bottom of the view to get the top
// border of the popup 2px below the bottom of the Omnibox.
const CGFloat kPopupPointYOffset = 2.0;

}  // namespace

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

bool ContentSettingDecoration::UpdateFromTabContents(
    const TabContents* tab_contents) {
  bool was_visible = IsVisible();
  int old_icon = content_setting_image_model_->get_icon();
  content_setting_image_model_->UpdateFromTabContents(tab_contents);
  SetVisible(content_setting_image_model_->is_visible());
  bool decoration_changed = was_visible != IsVisible() ||
      old_icon != content_setting_image_model_->get_icon();
  if (IsVisible()) {
    // TODO(thakis): We should use pdfs for these icons on OSX.
    // http://crbug.com/35847
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    SetImage(rb.GetNSImageNamed(content_setting_image_model_->get_icon()));
    SetToolTip(base::SysUTF8ToNSString(
        content_setting_image_model_->get_tooltip()));
  }
  return decoration_changed;
}

NSPoint ContentSettingDecoration::GetBubblePointInFrame(NSRect frame) {
  const NSRect draw_frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(draw_frame),
                     NSMaxY(draw_frame) - kPopupPointYOffset);
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
  // Consider passing in |control_view|.  Or refactoring to be
  // consistent with other decorations (which don't currently bring up
  // their bubble directly).
  AutocompleteTextField* field = owner_->GetAutocompleteTextField();
  NSPoint anchor = GetBubblePointInFrame(frame);
  anchor = [field convertPoint:anchor toView:nil];
  anchor = [[field window] convertBaseToScreen:anchor];

  // Open bubble.
  ContentSettingBubbleModel* model =
      ContentSettingBubbleModel::CreateContentSettingBubbleModel(
          tabContents, profile_,
          content_setting_image_model_->get_content_settings_type());
  [ContentSettingBubbleController showForModel:model
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
