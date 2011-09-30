// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/link_infobar_gtk.h"

#include "chrome/browser/tab_contents/link_infobar_delegate.h"
#include "chrome/browser/ui/gtk/gtk_util.h"

// LinkInfoBarDelegate ---------------------------------------------------------

InfoBar* LinkInfoBarDelegate::CreateInfoBar(InfoBarTabHelper* owner) {
  return new LinkInfoBarGtk(owner, this);
}

// LinkInfoBarGtk --------------------------------------------------------------

LinkInfoBarGtk::LinkInfoBarGtk(InfoBarTabHelper* owner,
                               LinkInfoBarDelegate* delegate)
    : InfoBarGtk(owner, delegate) {
  size_t link_offset;
  string16 display_text = delegate->GetMessageTextWithOffset(&link_offset);
  string16 link_text = delegate->GetLinkText();
  AddLabelWithInlineLink(display_text, link_text, link_offset,
                         G_CALLBACK(OnLinkClickedThunk));
}

LinkInfoBarGtk::~LinkInfoBarGtk() {
}

void LinkInfoBarGtk::OnLinkClicked(GtkWidget* button) {
  if (GetDelegate()->LinkClicked(
        gtk_util::DispositionForCurrentButtonPressEvent()))
    RemoveSelf();
}

LinkInfoBarDelegate* LinkInfoBarGtk::GetDelegate() {
  return delegate()->AsLinkInfoBarDelegate();
}
