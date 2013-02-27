// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/script_bubble_gtk.h"

#include <string>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/extensions/script_bubble_controller.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

using content::WebContents;
using content::OpenURLParams;
using content::Referrer;
using extensions::Extension;
using extensions::ExtensionSystem;
using extensions::TabHelper;
using extensions::ImageLoader;

namespace {

// The currently open app-modal bubble singleton, or NULL if none is open.
ScriptBubbleGtk* g_bubble = NULL;

}  // namespace


// static
void ScriptBubbleGtk::Show(GtkWidget* anchor, WebContents* web_contents) {
  if (!g_bubble)
    g_bubble = new ScriptBubbleGtk(anchor, web_contents);
}

// static
void ScriptBubbleGtk::OnItemLinkClickedThunk(GtkWidget* sender,
                                             void* user_data) {
  g_bubble->OnItemLinkClicked(sender);
}

ScriptBubbleGtk::ScriptBubbleGtk(GtkWidget* anchor, WebContents* web_contents)
    : anchor_(anchor),
      web_contents_(web_contents),
      profile_(Profile::FromBrowserContext(web_contents_->GetBrowserContext())),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  BuildBubble();
}

ScriptBubbleGtk::~ScriptBubbleGtk() {
}

void ScriptBubbleGtk::Close() {
  if (bubble_)
    bubble_->Close();
}

void ScriptBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                    bool closed_by_escape) {
  g_bubble = NULL;
  delete this;
}

void ScriptBubbleGtk::BuildBubble() {
  GtkThemeService* theme_provider = GtkThemeService::GetFrom(profile_);

  GtkWidget* bubble_content = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(bubble_content),
                                 ui::kContentAreaBorder);

  extensions::TabHelper* tab_helper =
      extensions::TabHelper::FromWebContents(web_contents_);
  const std::set<std::string>& extensions_running_scripts =
      tab_helper->script_bubble_controller()->extensions_running_scripts();
  const ExtensionSet* loaded_extensions =
      ExtensionSystem::Get(profile_)->extension_service()->extensions();

  std::string heading_string =
      l10n_util::GetStringUTF8(IDS_SCRIPT_BUBBLE_HEADLINE);
  GtkWidget* heading = gtk_label_new(heading_string.c_str());
  gtk_misc_set_alignment(GTK_MISC(heading), 0, 0);
  gtk_box_pack_start(GTK_BOX(bubble_content), heading, FALSE, FALSE, 0);

  for (std::set<std::string>::const_iterator iter =
           extensions_running_scripts.begin();
       iter != extensions_running_scripts.end(); ++iter) {
    const Extension* extension = loaded_extensions->GetByID(*iter);
    if (!extension)
      continue;

    GtkWidget* item = gtk_hbox_new(FALSE, ui::kControlSpacing);
    gtk_box_pack_start(GTK_BOX(bubble_content), item, FALSE, FALSE, 0);

    GtkWidget* image = gtk_image_new();
    gtk_widget_set_usize(image, 16, 16);
    gtk_box_pack_start(GTK_BOX(item), image, FALSE, FALSE, 0);

    // Load the image asynchronously.
    icon_controls_[extension->id()] = GTK_IMAGE(image);
    ImageLoader::Get(profile_)->LoadImageAsync(
        extension,
        extensions::IconsInfo::GetIconResource(
            extension,
            extension_misc::EXTENSION_ICON_BITTY,
            ExtensionIconSet::MATCH_EXACTLY),
        gfx::Size(extension_misc::EXTENSION_ICON_BITTY,
                  extension_misc::EXTENSION_ICON_BITTY),
        base::Bind(&ScriptBubbleGtk::OnIconLoaded,
                   weak_ptr_factory_.GetWeakPtr(),
                   extension->id()));

    GtkWidget* link = theme_provider->BuildChromeLinkButton(
        extension->name().c_str());
    gtk_box_pack_start(GTK_BOX(item), link, FALSE, FALSE, 0);
    link_controls_[GTK_WIDGET(link)] = extension->id();
    g_signal_connect(link, "button-press-event",
                     G_CALLBACK(&OnItemLinkClickedThunk), this);
  }

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

void ScriptBubbleGtk::OnIconLoaded(const std::string& extension_id,
                                   const gfx::Image& icon) {
  gtk_image_set_from_pixbuf(
      icon_controls_[extension_id],
      (!icon.IsEmpty() ? icon :
       GtkThemeService::GetFrom(profile_)->GetImageNamed(
           IDR_EXTENSIONS_FAVICON)).ToGdkPixbuf());
}

void ScriptBubbleGtk::OnItemLinkClicked(GtkWidget* button) {
  std::string link(chrome::kChromeUIExtensionsURL);
  link += "?id=" + link_controls_[button];
  web_contents_->OpenURL(OpenURLParams(GURL(link),
                                       Referrer(),
                                       NEW_FOREGROUND_TAB,
                                       content::PAGE_TRANSITION_LINK,
                                       false));
  Close();
}
