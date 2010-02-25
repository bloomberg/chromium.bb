// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/content_blocked_bubble_gtk.h"

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/options/content_settings_window_gtk.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"

ContentBlockedBubbleGtk::ContentBlockedBubbleGtk(
    GtkWindow* toplevel_window,
    const gfx::Rect& bounds,
    InfoBubbleGtkDelegate* delegate,
    ContentSettingsType content_type,
    const std::string& host,
    const std::wstring& display_host,
    Profile* profile,
    TabContents* tab_contents)
    : toplevel_window_(toplevel_window),
      bounds_(bounds),
      content_type_(content_type),
      host_(host),
      display_host_(display_host),
      profile_(profile),
      tab_contents_(tab_contents),
      delegate_(delegate),
      info_bubble_(NULL),
      allow_radio_(NULL),
      block_radio_(NULL) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents));
  BuildBubble();
}

ContentBlockedBubbleGtk::~ContentBlockedBubbleGtk() {
}

void ContentBlockedBubbleGtk::Close() {
  if (info_bubble_)
    info_bubble_->Close();
}

void ContentBlockedBubbleGtk::InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                                bool closed_by_escape) {
  delegate_->InfoBubbleClosing(info_bubble, closed_by_escape);
  delete this;
}

void ContentBlockedBubbleGtk::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(source == Source<TabContents>(tab_contents_));
  tab_contents_ = NULL;
}

void ContentBlockedBubbleGtk::BuildBubble() {
  GtkThemeProvider* theme_provider = GtkThemeProvider::GetFrom(profile_);

  GtkWidget* bubble_content = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

  // Add the content label.
  static const int kTitleIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    IDS_BLOCKED_COOKIES_TITLE,
    IDS_BLOCKED_IMAGES_TITLE,
    IDS_BLOCKED_JAVASCRIPT_TITLE,
    IDS_BLOCKED_PLUGINS_TITLE,
    IDS_BLOCKED_POPUPS_TITLE,
  };
  DCHECK_EQ(arraysize(kTitleIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  GtkWidget* label = gtk_label_new(l10n_util::GetStringUTF8(
      kTitleIDs[content_type_]).c_str());
  gtk_box_pack_start(GTK_BOX(bubble_content), label, FALSE, FALSE, 0);

  if (content_type_ == CONTENT_SETTINGS_TYPE_POPUPS) {
    BlockedPopupContainer::BlockedContents blocked_contents;
    DCHECK(tab_contents_);
    DCHECK(tab_contents_->blocked_popup_container());
    tab_contents_->blocked_popup_container()->GetBlockedContents(
        &blocked_contents);

    GtkWidget* table = gtk_table_new(blocked_contents.size(), 2, FALSE);
    int row = 0;
    for (BlockedPopupContainer::BlockedContents::const_iterator
             i(blocked_contents.begin()); i != blocked_contents.end();
         ++i, ++row) {
      SkBitmap icon = (*i)->GetFavIcon();
      GtkWidget* image = gtk_image_new();
      if (!icon.empty()) {
        GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), icon_pixbuf);
        g_object_unref(icon_pixbuf);

        // We stuff the image in an event box so we can trap mouse clicks on the
        // image (and launch the popup).
        GtkWidget* event_box = gtk_event_box_new();
        gtk_container_add(GTK_CONTAINER(event_box), image);

        popup_icons_[event_box] = *i;
        g_signal_connect(event_box, "button_press_event",
                         G_CALLBACK(OnPopupIconButtonPress), this);
        gtk_table_attach(GTK_TABLE(table), event_box, 0, 1, row, row + 1,
                         GTK_FILL, GTK_FILL, gtk_util::kControlSpacing / 2,
                         gtk_util::kControlSpacing / 2);
      }

      string16 title((*i)->GetTitle());
      // The popup may not have committed a load yet, in which case it won't
      // have a URL or title.
      if (title.empty())
        title = l10n_util::GetStringUTF16(IDS_TAB_LOADING_TITLE);

      GtkWidget* button =
          gtk_chrome_link_button_new(UTF16ToUTF8(title).c_str());
      popup_links_[button] = *i;
      g_signal_connect(button, "clicked", G_CALLBACK(OnPopupLinkClicked),
                       this);
      gtk_table_attach(GTK_TABLE(table), button, 1, 2, row, row + 1,
                       GTK_FILL, GTK_FILL, gtk_util::kControlSpacing / 2,
                       gtk_util::kControlSpacing / 2);
    }

    gtk_box_pack_start(GTK_BOX(bubble_content), table, FALSE, FALSE, 0);
  }

  if (content_type_ != CONTENT_SETTINGS_TYPE_COOKIES) {
    static const int kAllowIDs[CONTENT_SETTINGS_NUM_TYPES] = {
      0,  // Not displayed for cookies
      IDS_BLOCKED_IMAGES_UNBLOCK,
      IDS_BLOCKED_JAVASCRIPT_UNBLOCK,
      IDS_BLOCKED_PLUGINS_UNBLOCK,
      IDS_BLOCKED_POPUPS_UNBLOCK,
    };
    DCHECK_EQ(arraysize(kAllowIDs),
              static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
    allow_radio_ = gtk_radio_button_new_with_label(
        NULL,
        l10n_util::GetStringFUTF8(kAllowIDs[content_type_],
                                  WideToUTF16Hack(display_host_)).c_str());
    gtk_box_pack_start(GTK_BOX(bubble_content), allow_radio_, FALSE,
                       FALSE, 0);

    static const int kBlockIDs[CONTENT_SETTINGS_NUM_TYPES] = {
      0,  // Not displayed for cookies
      IDS_BLOCKED_IMAGES_NO_ACTION,
      IDS_BLOCKED_JAVASCRIPT_NO_ACTION,
      IDS_BLOCKED_PLUGINS_NO_ACTION,
      IDS_BLOCKED_POPUPS_NO_ACTION,
    };
    DCHECK_EQ(arraysize(kBlockIDs),
              static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
    block_radio_ = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(allow_radio_),
        l10n_util::GetStringUTF8(kBlockIDs[content_type_]).c_str());
    gtk_box_pack_start(GTK_BOX(bubble_content), block_radio_, FALSE,
                       FALSE, 0);

    // We must set the default value before we attach the signal handlers or
    // pain occurs.
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
        (profile_->GetHostContentSettingsMap()->GetContentSetting(host_,
            content_type_) == CONTENT_SETTING_ALLOW) ?
        allow_radio_ : block_radio_), TRUE);

    g_signal_connect(allow_radio_, "toggled",
                     G_CALLBACK(OnAllowBlockToggled), this);
    g_signal_connect(block_radio_, "toggled",
                     G_CALLBACK(OnAllowBlockToggled), this);
  }

  gtk_box_pack_start(GTK_BOX(bubble_content), gtk_hseparator_new(), FALSE,
                     FALSE, 0);

  GtkWidget* bottom_box = gtk_hbox_new(FALSE, 0);

  static const int kLinkIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    IDS_BLOCKED_COOKIES_LINK,
    IDS_BLOCKED_IMAGES_LINK,
    IDS_BLOCKED_JAVASCRIPT_LINK,
    IDS_BLOCKED_PLUGINS_LINK,
    IDS_BLOCKED_POPUPS_LINK,
  };
  DCHECK_EQ(arraysize(kLinkIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  GtkWidget* manage_link = gtk_chrome_link_button_new(l10n_util::GetStringUTF8(
      kLinkIDs[content_type_]).c_str());
  g_signal_connect(manage_link, "clicked", G_CALLBACK(OnManageLinkClicked),
                   this);
  gtk_box_pack_start(GTK_BOX(bottom_box), manage_link, FALSE, FALSE, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_CLOSE).c_str());
  g_signal_connect(button, "clicked", G_CALLBACK(OnCloseButtonClicked), this);
  gtk_box_pack_end(GTK_BOX(bottom_box), button, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(bubble_content), bottom_box, FALSE, FALSE, 0);

  InfoBubbleGtk::ArrowLocationGtk arrow_location =
      (l10n_util::GetTextDirection() == l10n_util::LEFT_TO_RIGHT) ?
      InfoBubbleGtk::ARROW_LOCATION_TOP_RIGHT :
      InfoBubbleGtk::ARROW_LOCATION_TOP_LEFT;
  info_bubble_ = InfoBubbleGtk::Show(
      toplevel_window_,
      bounds_,
      bubble_content,
      arrow_location,
      true,
      theme_provider,
      this);
}

void ContentBlockedBubbleGtk::LaunchPopup(TabContents* popup) {
  if (tab_contents_ && tab_contents_->blocked_popup_container()) {
    tab_contents_->blocked_popup_container()->LaunchPopupForContents(popup);

    // The views interface implicitly closes because of the launching of a new
    // window; we need to do that explicitly.
    Close();
  }
}

// static
void ContentBlockedBubbleGtk::OnPopupIconButtonPress(
    GtkWidget* icon_event_box,
    GdkEventButton* event,
    ContentBlockedBubbleGtk* bubble) {
  PopupMap::iterator i(bubble->popup_icons_.find(icon_event_box));
  DCHECK(i != bubble->popup_icons_.end());
  bubble->LaunchPopup(i->second);
}

// static
void ContentBlockedBubbleGtk::OnPopupLinkClicked(
    GtkWidget* button,
    ContentBlockedBubbleGtk* bubble) {
  PopupMap::iterator i(bubble->popup_links_.find(button));
  DCHECK(i != bubble->popup_links_.end());
  bubble->LaunchPopup(i->second);
}

// static
void ContentBlockedBubbleGtk::OnAllowBlockToggled(
    GtkWidget* widget,
    ContentBlockedBubbleGtk* bubble) {
  DCHECK((widget == bubble->allow_radio_) || (widget == bubble->block_radio_));
  bubble->profile_->GetHostContentSettingsMap()->SetContentSetting(
      bubble->host_,
      bubble->content_type_,
      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(bubble->allow_radio_)) ?
      CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}

// static
void ContentBlockedBubbleGtk::OnCloseButtonClicked(
    GtkButton *button,
    ContentBlockedBubbleGtk* bubble) {
  bubble->Close();
}

// static
void ContentBlockedBubbleGtk::OnManageLinkClicked(
    GtkButton* button,
    ContentBlockedBubbleGtk* bubble) {
  if (bubble->tab_contents_) {
    bubble->tab_contents_->delegate()->ShowContentSettingsWindow(
        bubble->content_type_);
  } else {
    ContentSettingsWindowGtk::Show(NULL, bubble->content_type_,
                                   bubble->profile_);
  }

  bubble->Close();
}
