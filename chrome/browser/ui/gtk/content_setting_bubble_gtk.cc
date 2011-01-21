// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/content_setting_bubble_gtk.h"

#include <set>
#include <string>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/blocked_content_container.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/content_setting_bubble_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/options/content_settings_window_gtk.h"
#include "chrome/common/content_settings.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "gfx/gtk_util.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/text_elider.h"
#include "webkit/plugins/npapi/plugin_list.h"

namespace {

// Padding between content and edge of info bubble.
const int kContentBorder = 7;

// The maximum width of a title entry in the content box. We elide anything
// longer than this.
const int kMaxLinkPixelSize = 500;

std::string BuildElidedText(const std::string& input) {
  return UTF16ToUTF8(ui::ElideText(
      UTF8ToUTF16(input),
      gfx::Font(),
      kMaxLinkPixelSize,
      false));
}

}  // namespace

ContentSettingBubbleGtk::ContentSettingBubbleGtk(
    GtkWidget* anchor,
    InfoBubbleGtkDelegate* delegate,
    ContentSettingBubbleModel* content_setting_bubble_model,
    Profile* profile,
    TabContents* tab_contents)
    : anchor_(anchor),
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

  const ContentSettingBubbleModel::BubbleContent& content =
      content_setting_bubble_model_->bubble_content();
  if (!content.title.empty()) {
    // Add the content label.
    GtkWidget* label = gtk_label_new(content.title.c_str());
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(bubble_content), label, FALSE, FALSE, 0);
  }

  const std::set<std::string>& plugins = content.resource_identifiers;
  if (!plugins.empty()) {
    GtkWidget* list_content = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

    for (std::set<std::string>::const_iterator it = plugins.begin();
        it != plugins.end(); ++it) {
      std::string name = UTF16ToUTF8(
          webkit::npapi::PluginList::Singleton()->GetPluginGroupName(*it));
      if (name.empty())
        name = *it;

      GtkWidget* label = gtk_label_new(BuildElidedText(name).c_str());
      GtkWidget* label_box = gtk_hbox_new(FALSE, 0);
      gtk_box_pack_start(GTK_BOX(label_box), label, FALSE, FALSE, 0);

      gtk_box_pack_start(GTK_BOX(list_content),
                         label_box,
                         FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(bubble_content), list_content, FALSE, FALSE,
                       gtk_util::kControlSpacing);
  }

  if (content_setting_bubble_model_->content_type() ==
      CONTENT_SETTINGS_TYPE_POPUPS) {
    const std::vector<ContentSettingBubbleModel::PopupItem>& popup_items =
        content.popup_items;
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
                         G_CALLBACK(OnPopupIconButtonPressThunk), this);
        gtk_table_attach(GTK_TABLE(table), event_box, 0, 1, row, row + 1,
                         GTK_FILL, GTK_FILL, gtk_util::kControlSpacing / 2,
                         gtk_util::kControlSpacing / 2);
      }

      GtkWidget* button = gtk_chrome_link_button_new(
          BuildElidedText(i->title).c_str());
      popup_links_[button] = i -popup_items.begin();
      g_signal_connect(button, "clicked", G_CALLBACK(OnPopupLinkClickedThunk),
                       this);
      gtk_table_attach(GTK_TABLE(table), button, 1, 2, row, row + 1,
                       GTK_FILL, GTK_FILL, gtk_util::kControlSpacing / 2,
                       gtk_util::kControlSpacing / 2);
    }

    gtk_box_pack_start(GTK_BOX(bubble_content), table, FALSE, FALSE, 0);
  }

  if (content_setting_bubble_model_->content_type() !=
      CONTENT_SETTINGS_TYPE_COOKIES) {
    const ContentSettingBubbleModel::RadioGroup& radio_group =
        content.radio_group;
    for (ContentSettingBubbleModel::RadioItems::const_iterator i =
         radio_group.radio_items.begin();
         i != radio_group.radio_items.end(); ++i) {
      std::string elided = BuildElidedText(*i);
      GtkWidget* radio =
          radio_group_gtk_.empty() ?
              gtk_radio_button_new_with_label(NULL, elided.c_str()) :
              gtk_radio_button_new_with_label_from_widget(
                  GTK_RADIO_BUTTON(radio_group_gtk_[0]),
                  elided.c_str());
      gtk_box_pack_start(GTK_BOX(bubble_content), radio, FALSE, FALSE, 0);
      if (i - radio_group.radio_items.begin() == radio_group.default_item) {
        // We must set the default value before we attach the signal handlers
        // or pain occurs.
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
      }
      radio_group_gtk_.push_back(radio);
    }
    for (std::vector<GtkWidget*>::const_iterator i = radio_group_gtk_.begin();
         i != radio_group_gtk_.end(); ++i) {
      // We can attach signal handlers now that all defaults are set.
      g_signal_connect(*i, "toggled", G_CALLBACK(OnRadioToggledThunk), this);
    }
  }

  for (std::vector<ContentSettingBubbleModel::DomainList>::const_iterator i =
       content.domain_lists.begin();
       i != content.domain_lists.end(); ++i) {
    // Put each list into its own vbox to allow spacing between lists.
    GtkWidget* list_content = gtk_vbox_new(FALSE, gtk_util::kControlSpacing);

    GtkWidget* label = gtk_label_new(BuildElidedText(i->title).c_str());
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    GtkWidget* label_box = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(label_box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(list_content), label_box, FALSE, FALSE, 0);
    for (std::set<std::string>::const_iterator j = i->hosts.begin();
         j != i->hosts.end(); ++j) {
      gtk_box_pack_start(GTK_BOX(list_content),
                         gtk_util::IndentWidget(gtk_util::CreateBoldLabel(*j)),
                         FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(bubble_content), list_content, FALSE, FALSE,
                       gtk_util::kControlSpacing);
  }

  if (!content.custom_link.empty()) {
    GtkWidget* custom_link_box = gtk_hbox_new(FALSE, 0);
    GtkWidget* custom_link = NULL;
    if (content.custom_link_enabled) {
      custom_link = gtk_chrome_link_button_new(content.custom_link.c_str());
      g_signal_connect(custom_link, "clicked",
                       G_CALLBACK(OnCustomLinkClickedThunk), this);
    } else {
      custom_link = gtk_label_new(content.custom_link.c_str());
      gtk_misc_set_alignment(GTK_MISC(custom_link), 0, 0.5);
    }
    DCHECK(custom_link);
    gtk_box_pack_start(GTK_BOX(custom_link_box), custom_link, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(bubble_content), custom_link_box,
                       FALSE, FALSE, 0);
  }

  gtk_box_pack_start(GTK_BOX(bubble_content), gtk_hseparator_new(),
                     FALSE, FALSE, 0);

  GtkWidget* bottom_box = gtk_hbox_new(FALSE, 0);

  GtkWidget* manage_link =
      gtk_chrome_link_button_new(content.manage_link.c_str());
  g_signal_connect(manage_link, "clicked", G_CALLBACK(OnManageLinkClickedThunk),
                   this);
  gtk_box_pack_start(GTK_BOX(bottom_box), manage_link, FALSE, FALSE, 0);

  GtkWidget* button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_DONE).c_str());
  g_signal_connect(button, "clicked", G_CALLBACK(OnCloseButtonClickedThunk),
                   this);
  gtk_box_pack_end(GTK_BOX(bottom_box), button, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(bubble_content), bottom_box, FALSE, FALSE, 0);
  gtk_widget_grab_focus(bottom_box);
  gtk_widget_grab_focus(button);

  InfoBubbleGtk::ArrowLocationGtk arrow_location =
      !base::i18n::IsRTL() ?
      InfoBubbleGtk::ARROW_LOCATION_TOP_RIGHT :
      InfoBubbleGtk::ARROW_LOCATION_TOP_LEFT;
  info_bubble_ = InfoBubbleGtk::Show(
      anchor_,
      NULL,
      bubble_content,
      arrow_location,
      true,  // match_system_theme
      true,  // grab_input
      theme_provider,
      this);
}

void ContentSettingBubbleGtk::OnPopupIconButtonPress(
    GtkWidget* icon_event_box,
    GdkEventButton* event) {
  PopupMap::iterator i(popup_icons_.find(icon_event_box));
  DCHECK(i != popup_icons_.end());
  content_setting_bubble_model_->OnPopupClicked(i->second);
  // The views interface implicitly closes because of the launching of a new
  // window; we need to do that explicitly.
  Close();
}

void ContentSettingBubbleGtk::OnPopupLinkClicked(GtkWidget* button) {
  PopupMap::iterator i(popup_links_.find(button));
  DCHECK(i != popup_links_.end());
  content_setting_bubble_model_->OnPopupClicked(i->second);
  // The views interface implicitly closes because of the launching of a new
  // window; we need to do that explicitly.
  Close();
}

void ContentSettingBubbleGtk::OnRadioToggled(GtkWidget* widget) {
  for (ContentSettingBubbleGtk::RadioGroupGtk::const_iterator i =
       radio_group_gtk_.begin();
       i != radio_group_gtk_.end(); ++i) {
    if (widget == *i) {
      content_setting_bubble_model_->OnRadioClicked(
          i - radio_group_gtk_.begin());
      return;
    }
  }
  NOTREACHED() << "unknown radio toggled";
}

void ContentSettingBubbleGtk::OnCloseButtonClicked(GtkWidget *button) {
  Close();
}

void ContentSettingBubbleGtk::OnCustomLinkClicked(GtkWidget* button) {
  content_setting_bubble_model_->OnCustomLinkClicked();
  Close();
}

void ContentSettingBubbleGtk::OnManageLinkClicked(GtkWidget* button) {
  content_setting_bubble_model_->OnManageLinkClicked();
  Close();
}
