// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/media_stream_infobar_gtk.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/media/media_stream_devices_menu_model.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/media_stream_infobar_delegate.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/l10n/l10n_util.h"

InfoBar* MediaStreamInfoBarDelegate::CreateInfoBar(InfoBarService* owner) {
  DCHECK(owner);
  return new MediaStreamInfoBarGtk(static_cast<InfoBarTabHelper*>(owner), this);
}

MediaStreamInfoBarGtk::MediaStreamInfoBarGtk(
    InfoBarTabHelper* owner,
    MediaStreamInfoBarDelegate* delegate)
    : InfoBarGtk(owner, delegate) {
  Init();
}

MediaStreamInfoBarGtk::~MediaStreamInfoBarGtk() {
}

void MediaStreamInfoBarGtk::Init() {
  GtkWidget* hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_util::CenterWidgetInHBox(hbox_, hbox, false, 0);
  size_t offset = 0;

  int message_id = IDS_MEDIA_CAPTURE_AUDIO_AND_VIDEO;
  DCHECK(GetDelegate()->HasAudio() || GetDelegate()->HasVideo());
  if (!GetDelegate()->HasAudio())
    message_id = IDS_MEDIA_CAPTURE_VIDEO_ONLY;
  else if (!GetDelegate()->HasVideo())
    message_id = IDS_MEDIA_CAPTURE_AUDIO_ONLY;

  string16 security_origin = UTF8ToUTF16(
      GetDelegate()->GetSecurityOriginSpec());
  string16 text(l10n_util::GetStringFUTF16(message_id,
                                           security_origin, &offset));

  gtk_box_pack_start(GTK_BOX(hbox), CreateLabel(UTF16ToUTF8(text)),
                     FALSE, FALSE, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_MEDIA_CAPTURE_ALLOW).c_str());
  Signals()->Connect(button, "clicked",
                     G_CALLBACK(&OnAllowButtonThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_MEDIA_CAPTURE_DENY).c_str());
  Signals()->Connect(button, "clicked",
                     G_CALLBACK(&OnDenyButtonThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  // The devices button.
  GtkWidget* devices_menu_button = CreateMenuButton(
      l10n_util::GetStringUTF8(IDS_MEDIA_CAPTURE_DEVICES_MENU_TITLE).c_str());
  Signals()->Connect(devices_menu_button, "clicked",
                     G_CALLBACK(&OnDevicesClickedThunk), this);
  gtk_widget_show_all(devices_menu_button);
  gtk_util::CenterWidgetInHBox(hbox_, devices_menu_button, true, 0);
}

void MediaStreamInfoBarGtk::OnDevicesClicked(GtkWidget* sender) {
  // InfoBarGtk takes the ownership of MediaStreamDevicesMenuModel.
  ShowMenuWithModel(sender, NULL,
                    new MediaStreamDevicesMenuModel(GetDelegate()));
}

void MediaStreamInfoBarGtk::OnAllowButton(GtkWidget* widget) {
  GetDelegate()->Accept();
  RemoveSelf();
}

void MediaStreamInfoBarGtk::OnDenyButton(GtkWidget* widget) {
  GetDelegate()->Deny();
  RemoveSelf();
}

MediaStreamInfoBarDelegate* MediaStreamInfoBarGtk::GetDelegate() {
  return delegate()->AsMediaStreamInfoBarDelegate();
}
