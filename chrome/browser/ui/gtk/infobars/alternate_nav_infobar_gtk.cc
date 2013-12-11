// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/alternate_nav_infobar_gtk.h"

#include "chrome/browser/ui/gtk/event_utils.h"
#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"


// AlternateNavInfoBarDelegate -------------------------------------------------

// static
scoped_ptr<InfoBar> AlternateNavInfoBarDelegate::CreateInfoBar(
    scoped_ptr<AlternateNavInfoBarDelegate> delegate) {
  return scoped_ptr<InfoBar>(new AlternateNavInfoBarGtk(delegate.Pass()));
}


// AlternateNavInfoBarGtk ------------------------------------------------------

AlternateNavInfoBarGtk::AlternateNavInfoBarGtk(
    scoped_ptr<AlternateNavInfoBarDelegate> delegate)
    : InfoBarGtk(delegate.PassAs<InfoBarDelegate>()) {
}

AlternateNavInfoBarGtk::~AlternateNavInfoBarGtk() {
}

void AlternateNavInfoBarGtk::PlatformSpecificSetOwner() {
  InfoBarGtk::PlatformSpecificSetOwner();

  size_t link_offset;
  base::string16 display_text =
      GetDelegate()->GetMessageTextWithOffset(&link_offset);
  base::string16 link_text = GetDelegate()->GetLinkText();
  AddLabelWithInlineLink(display_text, link_text, link_offset,
                         G_CALLBACK(OnLinkClickedThunk));
}

AlternateNavInfoBarDelegate* AlternateNavInfoBarGtk::GetDelegate() {
  return static_cast<AlternateNavInfoBarDelegate*>(delegate());
}

void AlternateNavInfoBarGtk::OnLinkClicked(GtkWidget* button) {
  if (GetDelegate()->LinkClicked(
        event_utils::DispositionForCurrentButtonPressEvent()))
    RemoveSelf();
}
