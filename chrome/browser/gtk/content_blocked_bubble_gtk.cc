// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/content_blocked_bubble_gtk.h"

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "chrome/browser/blocked_popup_container.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/options/content_settings_window_gtk.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"

// Padding between content and edge of info bubble.
static const int kContentBorder = 7;

ContentSettingBubbleGtk::ContentSettingBubbleGtk(
    GtkWindow* toplevel_window,
    const gfx::Rect& bounds,
    InfoBubbleGtkDelegate* delegate,
    ContentSettingBubbleModel* content_setting_bubble_model,
    Profile* profile,
    TabContents* tab_contents)
    : toplevel_window_(toplevel_window),
      bounds_(bounds),
      profile_(profile),
      tab_contents_(tab_contents),
      delegate_(delegate),
      content_setting_bubble_model_(content_setting_bubble_model),
      info_bubble_(NULL) {
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(tab_contents));
  BuildBubble();
}

ContentSettingBubbleGtk::~ContentSettingBubbleGtk() {
}

void ContentSettingBubbleGtk::Close() {
  if (info_bubble_)
    info_bubble_->Close();
}

void ContentSettingBubbleGtk::InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                                bool closed_by_escape) {
  delegate_->InfoBubbleClosing(info_bubble, closed_by_escape);
  delete this;
}

void ContentSettingBubbleGtk::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(source == Source<TabContents>(tab_contents_));
  tab_contents_ = NULL;
}

void ContentSettingBubbleGtk::BuildBubble() {
  GtkThemeProvider* theme_provider = GtkThemeProvider::GetFrom(profile_);

  GtkWidget* bubble_content = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(bubble_content), kContentBorder);

  // Add the content label.
  GtkWidget* label = gtk_label_new(
      content_setting_bubble_model_->bubble_content().title.c_str());
  gtk_box_pack_start(GTK_BOX(bubble_content), label, FALSE, FALSE, 0);

  if (content_setting_bubble_model_->content_type() ==
      CONTENT_SETTINGS_TYPE_POPUPS) {
    const std::vector<ContentSettingBubbleModel::PopupItem>& popup_items =
        content_setting_bubble_model_->bubble_content().popup_items;
    GtkWidget* table = gtk_table_new(popup_items.size(), 2, FALSE);
    int row = 0;
    for (std::vector<ContentSettingBubbleModel::PopupItem>::const_iterator
         i(popup_items.begin()); i != popup_items.end(); ++i, ++row) {
      GtkWidget* image = gtk_image_new();
      if (!i->bitmap.empty()) {
        GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(&i->bitmap);
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), icon_pixbuf);
        g_object_unref(icon_pixbuf);

        // We stuff the image in an event box so we can trap mouse clicks on the
        // image (and launch the popup).
        GtkWidget* event_box = gtk_event_box_new();
        gtk_container_add(GTK_CONTAINER(event_box), image);

        popup_icons_[event_box] = i -popup_items.begin();
        g_signal_connect(event_box, "button_press_event",
                         G_CALLBACK(OnPopupIconButtonPress), this);
        gtk_table_attach(GTK_TABLE(table), event_box, 0, 1, row, row + 1,
                         GTK_FILL, GTK_FILL, gtk_util::kControlSpacing / 2,
                         gtk_util::kControlSpacing / 2);
      }

      GtkWidget* button = gtk_chrome_link_button_new(i->title.c_str());
      popup_links_[button] = i -popup_items.begin();
      g_signal_connect(button, "clicked", G_CALLBACK(OnPopupLinkClicked),
                       this);
      gtk_table_attach(GTK_TABLE(table), button, 1, 2, row, row + 1,
                       GTK_FILL, GTK_FILL, gtk_util::kControlSpacing / 2,
                       gtk_util::kControlSpacing / 2);
    }

    gtk_box_pack_start(GTK_BOX(bubble_content), table, FALSE, FALSE, 0);
  }

  if (content_setting_bubble_model_->content_type() !=
      CONTENT_SETTINGS_TYPE_COOKIES) {
    const ContentSettingBubbleModel::RadioGroups& radio_groups =
        content_setting_bubble_model_->bubble_content().radio_groups;
    for (ContentSettingBubbleModel::RadioGroups::const_iterator i =
         radio_groups.begin(); i != radio_groups.end(); ++i) {
      const ContentSettingBubbleModel::RadioItems& radio_items = i->radio_items;
      RadioGroupGtk radio_group_gtk;
      for (ContentSettingBubbleModel::RadioItems::const_iterator j =
           radio_items.begin(); j != radio_items.end(); ++j) {
        GtkWidget* radio =
            radio_group_gtk.empty() ?
                gtk_radio_button_new_with_label(NULL, j->c_str()) :
                gtk_radio_button_new_with_label_from_widget(
                    GTK_RADIO_BUTTON(radio_group_gtk[0]),
                    j->c_str());
        gtk_box_pack_start(GTK_BOX(bubble_content), radio, FALSE, FALSE, 0);
        if (j - radio_items.begin() == i->default_item) {
          // We must set the default value before we attach the signal handlers
          // or pain occurs.
          gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
        }
        radio_group_gtk.push_back(radio);
      }
      for (std::vector<GtkWidget*>::const_iterator j = radio_group_gtk.begin();
           j != radio_group_gtk.end(); ++j) {
        g_signal_connect(*j, "toggled", G_CALLBACK(OnRadioToggled), this);
      }
      radio_groups_gtk_.push_back(radio_group_gtk);
      gtk_box_pack_start(GTK_BOX(bubble_content), gtk_hseparator_new(), FALSE,
                         FALSE, 0);
    }
  }

  GtkWidget* bottom_box = gtk_hbox_new(FALSE, 0);

  GtkWidget* manage_link = gtk_chrome_link_button_new(
      content_setting_bubble_model_->bubble_content().manage_link.c_str());
  g_signal_connect(manage_link, "clicked", G_CALLBACK(OnManageLinkClicked),
                   this);
  gtk_box_pack_start(GTK_BOX(bottom_box), manage_link, FALSE, FALSE, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_DONE).c_str());
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

// static
void ContentSettingBubbleGtk::OnPopupIconButtonPress(
    GtkWidget* icon_event_box,
    GdkEventButton* event,
    ContentSettingBubbleGtk* bubble) {
  PopupMap::iterator i(bubble->popup_icons_.find(icon_event_box));
  DCHECK(i != bubble->popup_icons_.end());
  bubble->content_setting_bubble_model_->OnPopupClicked(i->second);
  // The views interface implicitly closes because of the launching of a new
  // window; we need to do that explicitly.
  bubble->Close();

}

// static
void ContentSettingBubbleGtk::OnPopupLinkClicked(
    GtkWidget* button,
    ContentSettingBubbleGtk* bubble) {
  PopupMap::iterator i(bubble->popup_links_.find(button));
  DCHECK(i != bubble->popup_links_.end());
  bubble->content_setting_bubble_model_->OnPopupClicked(i->second);
  // The views interface implicitly closes because of the launching of a new
  // window; we need to do that explicitly.
  bubble->Close();
}

// static
void ContentSettingBubbleGtk::OnRadioToggled(
    GtkWidget* widget,
    ContentSettingBubbleGtk* bubble) {
  for (std::vector<RadioGroupGtk>::const_iterator i =
       bubble->radio_groups_gtk_.begin();
       i != bubble->radio_groups_gtk_.end(); ++i) {
    for (RadioGroupGtk::const_iterator j = i->begin(); j != i->end(); j++) {
      if (widget == *j) {
        bubble->content_setting_bubble_model_->OnRadioClicked(
            i - bubble->radio_groups_gtk_.begin(),
            j - i->begin());
        return;
      }
    }
  }
  NOTREACHED() << "unknown radio toggled";
}

// static
void ContentSettingBubbleGtk::OnCloseButtonClicked(
    GtkButton *button,
    ContentSettingBubbleGtk* bubble) {
  bubble->Close();
}

// static
void ContentSettingBubbleGtk::OnManageLinkClicked(
    GtkButton* button,
    ContentSettingBubbleGtk* bubble) {
  bubble->content_setting_bubble_model_->OnManageLinkClicked();
  bubble->Close();
}
