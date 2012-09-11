// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/media_stream_infobar.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"

InfoBar* MediaStreamInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  DCHECK(owner);
  return new MediaStreamInfoBar(static_cast<InfoBarTabHelper*>(owner), this);
}

MediaStreamInfoBar::MediaStreamInfoBar(
    InfoBarTabHelper* owner,
    MediaStreamInfoBarDelegate* delegate)
    : InfoBarView(owner, delegate),
      label_(NULL),
      allow_button_(NULL),
      deny_button_(NULL),
      devices_menu_button_(NULL),
      devices_menu_model_(delegate) {
}

MediaStreamInfoBar::~MediaStreamInfoBar() {
}

void MediaStreamInfoBar::Layout() {
  InfoBarView::Layout();

  int available_width = std::max(0, EndX() - StartX() - ContentMinimumWidth());
  gfx::Size label_size = label_->GetPreferredSize();
  label_->SetBounds(StartX(), OffsetY(label_size),
      std::min(label_size.width(), available_width), label_size.height());
  available_width = std::max(0, available_width - label_size.width());

  gfx::Size allow_button_size = allow_button_->GetPreferredSize();
  allow_button_->SetBounds(label_->bounds().right() + kEndOfLabelSpacing,
      OffsetY(allow_button_size), allow_button_size.width(),
      allow_button_size.height());

  gfx::Size deny_button_size = deny_button_->GetPreferredSize();
  deny_button_->SetBounds(
        allow_button_->bounds().right() + kButtonButtonSpacing,
        OffsetY(deny_button_size), deny_button_size.width(),
        deny_button_size.height());

  gfx::Size devices_size = devices_menu_button_->GetPreferredSize();
  devices_menu_button_->SetBounds(EndX() - devices_size.width(),
      OffsetY(devices_size), devices_size.width(), devices_size.height());
}

void MediaStreamInfoBar::ViewHierarchyChanged(bool is_add,
                                              views::View* parent,
                                              views::View* child) {
  if (is_add && child == this && (label_ == NULL)) {
    int message_id = IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO;
    DCHECK(GetDelegate()->HasAudio() || GetDelegate()->HasVideo());
    if (!GetDelegate()->HasAudio())
      message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY;
    else if (!GetDelegate()->HasVideo())
      message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY;

    label_ = CreateLabel(l10n_util::GetStringFUTF16(message_id,
        UTF8ToUTF16(GetDelegate()->GetSecurityOriginSpec())));
    AddChildView(label_);

    allow_button_ = CreateTextButton(this,
        l10n_util::GetStringUTF16(IDS_MEDIA_CAPTURE_ALLOW), false);
    AddChildView(allow_button_);

    deny_button_ = CreateTextButton(this,
        l10n_util::GetStringUTF16(IDS_MEDIA_CAPTURE_DENY), false);
    AddChildView(deny_button_);

    devices_menu_button_ = CreateMenuButton(
        l10n_util::GetStringUTF16(IDS_MEDIA_CAPTURE_DEVICES_MENU_TITLE), this);
    AddChildView(devices_menu_button_);
  }

  // This must happen after adding all other children so InfoBarView can ensure
  // the close button is the last child.
  InfoBarView::ViewHierarchyChanged(is_add, parent, child);
}

void MediaStreamInfoBar::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  if (!owned())
    return;  // We're closing; don't call anything, it might access the owner.
  if (sender == allow_button_) {
    GetDelegate()->Accept();
    RemoveSelf();
  } else if (sender == deny_button_) {
    GetDelegate()->Deny();
    RemoveSelf();
  } else {
    InfoBarView::ButtonPressed(sender, event);
  }
}

int MediaStreamInfoBar::ContentMinimumWidth() const {
  return
      kEndOfLabelSpacing +
      (allow_button_->GetPreferredSize().width() + kButtonButtonSpacing +
          deny_button_->GetPreferredSize().width()) +
      (kButtonButtonSpacing + devices_menu_button_->GetPreferredSize().width());
}

void MediaStreamInfoBar::OnMenuButtonClicked(views::View* source,
                                             const gfx::Point& point) {
  if (!owned())
    return;  // We're closing; don't call anything, it might access the owner.
  DCHECK_EQ(devices_menu_button_, source);
  RunMenuAt(&devices_menu_model_, devices_menu_button_,
            views::MenuItemView::TOPRIGHT);
}

MediaStreamInfoBarDelegate* MediaStreamInfoBar::GetDelegate() {
  return delegate()->AsMediaStreamInfoBarDelegate();
}
