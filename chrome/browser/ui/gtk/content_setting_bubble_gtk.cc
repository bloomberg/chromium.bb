// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/content_setting_bubble_gtk.h"

#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/stl_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/plugins/plugin_finder.h"
#include "chrome/browser/plugins/plugin_metadata.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/content_settings/content_setting_bubble_model.h"
#include "chrome/browser/ui/content_settings/content_setting_media_menu_model.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/content_settings.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/gtk_util.h"

using content::PluginService;
using content::WebContents;

namespace {

// The maximum width of a title entry in the content box. We elide anything
// longer than this.
const int kMaxLinkPixelSize = 500;

// The minimum and maximum width of the media menu buttons.
const int kMinMediaMenuButtonWidth = 100;
const int kMaxMediaMenuButtonWidth = 600;

std::string BuildElidedText(const std::string& input) {
  return UTF16ToUTF8(ui::ElideText(
      UTF8ToUTF16(input),
      gfx::Font(),
      kMaxLinkPixelSize,
      ui::ELIDE_AT_END));
}

}  // namespace

ContentSettingBubbleGtk::ContentSettingBubbleGtk(
    GtkWidget* anchor,
    BubbleDelegateGtk* delegate,
    ContentSettingBubbleModel* content_setting_bubble_model,
    Profile* profile,
    WebContents* web_contents)
    : anchor_(anchor),
      profile_(profile),
      web_contents_(web_contents),
      delegate_(delegate),
      content_setting_bubble_model_(content_setting_bubble_model),
      bubble_(NULL) {
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<WebContents>(web_contents));
  BuildBubble();
}

ContentSettingBubbleGtk::~ContentSettingBubbleGtk() {
}

void ContentSettingBubbleGtk::Close() {
  STLDeleteValues(&media_menus_);

  if (bubble_)
    bubble_->Close();
}

void ContentSettingBubbleGtk::UpdateMenuLabel(content::MediaStreamType type,
                                              const std::string& label) {
  GtkMediaMenuMap::const_iterator it = media_menus_.begin();
  for (; it != media_menus_.end(); ++it) {
    if (it->second->type == type) {
      gtk_label_set_text(GTK_LABEL(it->second->label.get()), label.c_str());
      return;
    }
  }
  NOTREACHED();
}

void ContentSettingBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                            bool closed_by_escape) {
  delegate_->BubbleClosing(bubble, closed_by_escape);
  delete this;
}

void ContentSettingBubbleGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
  DCHECK(source == content::Source<WebContents>(web_contents_));
  web_contents_ = NULL;
}

void ContentSettingBubbleGtk::BuildBubble() {
  GtkThemeService* theme_provider = GtkThemeService::GetFrom(profile_);

  GtkWidget* bubble_content = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(bubble_content),
                                 ui::kContentAreaBorder);

  const ContentSettingBubbleModel::BubbleContent& content =
      content_setting_bubble_model_->bubble_content();
  if (!content.title.empty()) {
    // Add the content label.
    GtkWidget* label = theme_provider->BuildLabel(content.title.c_str(),
                                                  ui::kGdkBlack);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_box_pack_start(GTK_BOX(bubble_content), label, FALSE, FALSE, 0);
  }

  const std::set<std::string>& plugins = content.resource_identifiers;
  if (!plugins.empty()) {
    GtkWidget* list_content = gtk_vbox_new(FALSE, ui::kControlSpacing);

    PluginFinder* finder = PluginFinder::GetInstance();
    for (std::set<std::string>::const_iterator it = plugins.begin();
        it != plugins.end(); ++it) {
      std::string name = UTF16ToUTF8(finder->FindPluginNameWithIdentifier(*it));
      GtkWidget* label = theme_provider->BuildLabel(
          BuildElidedText(name).c_str(), ui::kGdkBlack);
      GtkWidget* label_box = gtk_hbox_new(FALSE, 0);
      gtk_box_pack_start(GTK_BOX(label_box), label, FALSE, FALSE, 0);

      gtk_box_pack_start(GTK_BOX(list_content),
                         label_box,
                         FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(bubble_content), list_content, FALSE, FALSE,
                       ui::kControlSpacing);
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
      if (!i->image.IsEmpty()) {
        GdkPixbuf* icon_pixbuf = i->image.ToGdkPixbuf();
        gtk_image_set_from_pixbuf(GTK_IMAGE(image), icon_pixbuf);

        // We stuff the image in an event box so we can trap mouse clicks on the
        // image (and launch the popup).
        GtkWidget* event_box = gtk_event_box_new();
        gtk_container_add(GTK_CONTAINER(event_box), image);

        popup_icons_[event_box] = i -popup_items.begin();
        g_signal_connect(event_box, "button_press_event",
                         G_CALLBACK(OnPopupIconButtonPressThunk), this);
        gtk_table_attach(GTK_TABLE(table), event_box, 0, 1, row, row + 1,
                         GTK_FILL, GTK_FILL, ui::kControlSpacing / 2,
                         ui::kControlSpacing / 2);
      }

      GtkWidget* button = gtk_chrome_link_button_new(
          BuildElidedText(i->title).c_str());
      popup_links_[button] = i -popup_items.begin();
      g_signal_connect(button, "clicked", G_CALLBACK(OnPopupLinkClickedThunk),
                       this);
      gtk_table_attach(GTK_TABLE(table), button, 1, 2, row, row + 1,
                       GTK_FILL, GTK_FILL, ui::kControlSpacing / 2,
                       ui::kControlSpacing / 2);
    }

    gtk_box_pack_start(GTK_BOX(bubble_content), table, FALSE, FALSE, 0);
  }

  const ContentSettingBubbleModel::RadioGroup& radio_group =
      content.radio_group;
  for (ContentSettingBubbleModel::RadioItems::const_iterator i =
       radio_group.radio_items.begin();
       i != radio_group.radio_items.end(); ++i) {
    std::string elided = BuildElidedText(*i);
    GtkWidget* radio =
        radio_group_gtk_.empty() ?
            gtk_radio_button_new(NULL) :
            gtk_radio_button_new_from_widget(
                GTK_RADIO_BUTTON(radio_group_gtk_[0]));
    GtkWidget* label =
        theme_provider->BuildLabel(elided.c_str(), ui::kGdkBlack);
    gtk_container_add(GTK_CONTAINER(radio), label);
    gtk_box_pack_start(GTK_BOX(bubble_content), radio, FALSE, FALSE, 0);
    if (i - radio_group.radio_items.begin() == radio_group.default_item) {
      // We must set the default value before we attach the signal handlers
      // or pain occurs.
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), TRUE);
    }
    if (!content.radio_group_enabled)
      gtk_widget_set_sensitive(radio, FALSE);
    radio_group_gtk_.push_back(radio);
  }
  for (std::vector<GtkWidget*>::const_iterator i = radio_group_gtk_.begin();
       i != radio_group_gtk_.end(); ++i) {
    // We can attach signal handlers now that all defaults are set.
    g_signal_connect(*i, "toggled", G_CALLBACK(OnRadioToggledThunk), this);
  }

  // Layout code for the media device menus.
  if (content_setting_bubble_model_->content_type() ==
        CONTENT_SETTINGS_TYPE_MEDIASTREAM) {
    GtkWidget* table = gtk_table_new(content.media_menus.size(), 2, FALSE);
    int menu_width = 0;
    int row = 0;
    for (ContentSettingBubbleModel::MediaMenuMap::const_iterator
             i(content.media_menus.begin());
         i != content.media_menus.end();
         ++i, ++row) {
      GtkWidget* label = theme_provider->BuildLabel(
          i->second.label.c_str(), ui::kGdkBlack);
      gtk_table_attach(GTK_TABLE(table), gtk_util::LeftAlignMisc(label), 0, 1,
                       row, row + 1, GTK_FILL, GTK_FILL,
                       ui::kControlSpacing / 2, ui::kControlSpacing / 2);

      // Build up the gtk menu button.
      MediaMenuGtk* gtk_menu = new MediaMenuGtk(i->first);
      gtk_menu->label.Own(
          gtk_label_new(i->second.selected_device.name.c_str()));
      GtkWidget* button = gtk_button_new();
      GtkWidget* button_content = gtk_hbox_new(FALSE, 0);
      gtk_box_pack_start(GTK_BOX(button_content),
                         gtk_util::LeftAlignMisc(gtk_menu->label.get()),
                         FALSE, FALSE, 0);
      GtkWidget* arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
      gtk_box_pack_end(GTK_BOX(button_content), arrow, FALSE, FALSE, 0);

      gtk_container_add(GTK_CONTAINER(button), button_content);

      // Store the gtk menu to the map.
      gtk_menu->menu_model.reset(new ContentSettingMediaMenuModel(
          gtk_menu->type,
          content_setting_bubble_model_.get(),
          base::Bind(&ContentSettingBubbleGtk::UpdateMenuLabel,
                     base::Unretained(this))));
      gtk_menu->menu.reset(new MenuGtk(NULL, gtk_menu->menu_model.get()));
      media_menus_[button] = gtk_menu;

      // Use the longest width of the menus as the width of the menu buttons.
      GtkRequisition menu_req;
      gtk_widget_size_request(gtk_menu->menu->widget(), &menu_req);
      menu_width = std::max(menu_width, menu_req.width);

      g_signal_connect(button, "clicked", G_CALLBACK(OnMenuButtonClickedThunk),
                       this);
      gtk_table_attach(GTK_TABLE(table), button, 1, 2, row, row + 1,
                       GTK_FILL, GTK_FILL, ui::kControlSpacing * 2,
                       ui::kControlSpacing / 2);
    }

    // Make sure the width is within [kMinMediaMenuButtonWidth,
    // kMaxMediaMenuButtonWidth].
    menu_width = std::max(kMinMediaMenuButtonWidth, menu_width);
    menu_width = std::min(kMaxMediaMenuButtonWidth, menu_width);

    // Set all the menu buttons to the width we calculated above.
    for (GtkMediaMenuMap::const_iterator i = media_menus_.begin();
         i != media_menus_.end(); ++i)
      gtk_widget_set_size_request(i->first, menu_width, -1);

    gtk_box_pack_start(GTK_BOX(bubble_content), table, FALSE, FALSE, 0);
  }

  for (std::vector<ContentSettingBubbleModel::DomainList>::const_iterator i =
       content.domain_lists.begin();
       i != content.domain_lists.end(); ++i) {
    // Put each list into its own vbox to allow spacing between lists.
    GtkWidget* list_content = gtk_vbox_new(FALSE, ui::kControlSpacing);

    GtkWidget* label = theme_provider->BuildLabel(
        BuildElidedText(i->title).c_str(), ui::kGdkBlack);
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
                       ui::kControlSpacing);
  }

  if (!content.custom_link.empty()) {
    GtkWidget* custom_link_box = gtk_hbox_new(FALSE, 0);
    GtkWidget* custom_link = NULL;
    if (content.custom_link_enabled) {
      custom_link =
          theme_provider->BuildChromeLinkButton(content.custom_link.c_str());
      g_signal_connect(custom_link, "clicked",
                       G_CALLBACK(OnCustomLinkClickedThunk), this);
    } else {
      custom_link = theme_provider->BuildLabel(content.custom_link.c_str(),
                                               ui::kGdkBlack);
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
      theme_provider->BuildChromeLinkButton(content.manage_link.c_str());
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

  bubble_ = BubbleGtk::Show(anchor_,
                            NULL,
                            bubble_content,
                            BubbleGtk::ANCHOR_TOP_RIGHT,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
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
  content_setting_bubble_model_->OnDoneClicked();
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

void ContentSettingBubbleGtk::OnMenuButtonClicked(GtkWidget* button) {
  GtkMediaMenuMap::iterator i(media_menus_.find(button));
  DCHECK(i != media_menus_.end());
  i->second->menu->PopupForWidget(button, 1, gtk_get_current_event_time());
}

ContentSettingBubbleGtk::MediaMenuGtk::MediaMenuGtk(
    content::MediaStreamType type)
    : type(type) {}

ContentSettingBubbleGtk::MediaMenuGtk::~MediaMenuGtk() {}
