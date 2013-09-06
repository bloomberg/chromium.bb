// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/extension_installed_bubble_gtk.h"

#include <string>

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/gtk/browser_actions_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"

using extensions::Extension;
using extensions::ExtensionActionManager;

namespace {

const int kHorizontalColumnSpacing = 10;
const int kIconPadding = 3;
const int kIconSize = 43;
const int kTextColumnVerticalSpacing = 7;
const int kTextColumnWidth = 350;

}  // namespace

namespace chrome {

void ShowExtensionInstalledBubble(const Extension* extension,
                                  Browser* browser,
                                  const SkBitmap& icon) {
  ExtensionInstalledBubbleGtk::Show(extension, browser, icon);
}

}  // namespace chrome

void ExtensionInstalledBubbleGtk::Show(const Extension* extension,
                                       Browser* browser,
                                       const SkBitmap& icon) {
  new ExtensionInstalledBubbleGtk(extension, browser, icon);
}

ExtensionInstalledBubbleGtk::ExtensionInstalledBubbleGtk(
    const Extension* extension, Browser *browser, const SkBitmap& icon)
    : bubble_(this, extension, browser, icon) {
}

ExtensionInstalledBubbleGtk::~ExtensionInstalledBubbleGtk() {}

void ExtensionInstalledBubbleGtk::OnDestroy(GtkWidget* widget) {
  gtk_bubble_ = NULL;
  delete this;
}

bool ExtensionInstalledBubbleGtk::MaybeShowNow() {
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(
          bubble_.browser()->window()->GetNativeWindow());

  GtkWidget* reference_widget = NULL;

  if (bubble_.type() == bubble_.BROWSER_ACTION) {
    BrowserActionsToolbarGtk* toolbar =
        browser_window->GetToolbar()->GetBrowserActionsToolbar();
    if (toolbar->animating())
      return false;

    reference_widget = toolbar->GetBrowserActionWidget(bubble_.extension());
    // glib delays recalculating layout, but we need reference_widget to know
    // its coordinates, so we force a check_resize here.
    gtk_container_check_resize(GTK_CONTAINER(
        browser_window->GetToolbar()->widget()));
    // If the widget is not visible then browser_window could be incognito
    // with this extension disabled. Try showing it on the chevron.
    // If that fails, fall back to default position.
    if (reference_widget && !gtk_widget_get_visible(reference_widget)) {
      reference_widget = gtk_widget_get_visible(toolbar->chevron()) ?
          toolbar->chevron() : NULL;
    }
  } else if (bubble_.type() == bubble_.PAGE_ACTION) {
    LocationBarViewGtk* location_bar_view =
        browser_window->GetToolbar()->GetLocationBarView();
    ExtensionAction* page_action =
        ExtensionActionManager::Get(bubble_.browser()->profile())->
        GetPageAction(*bubble_.extension());
    location_bar_view->SetPreviewEnabledPageAction(page_action,
                                                   true);  // preview_enabled
    reference_widget = location_bar_view->GetPageActionWidget(page_action);
    // glib delays recalculating layout, but we need reference_widget to know
    // its coordinates, so we force a check_resize here.
    gtk_container_check_resize(GTK_CONTAINER(
        browser_window->GetToolbar()->widget()));
    DCHECK(reference_widget);
  } else if (bubble_.type() == bubble_.OMNIBOX_KEYWORD) {
    LocationBarViewGtk* location_bar_view =
        browser_window->GetToolbar()->GetLocationBarView();
    reference_widget = location_bar_view->location_entry_widget();
    DCHECK(reference_widget);
  }

  // Default case.
  if (reference_widget == NULL)
    reference_widget = browser_window->GetToolbar()->GetAppMenuButton();

  GtkThemeService* theme_provider = GtkThemeService::GetFrom(
      bubble_.browser()->profile());

  // Setup the BubbleGtk content.
  GtkWidget* bubble_content = gtk_hbox_new(FALSE, kHorizontalColumnSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(bubble_content),
                                 ui::kContentAreaBorder);

  if (!bubble_.icon().isNull()) {
    // Scale icon down to 43x43, but allow smaller icons (don't scale up).
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(bubble_.icon());
    gfx::Size size(bubble_.icon().width(), bubble_.icon().height());
    if (size.width() > kIconSize || size.height() > kIconSize) {
      if (size.width() > size.height()) {
        size.set_height(size.height() * kIconSize / size.width());
        size.set_width(kIconSize);
      } else {
        size.set_width(size.width() * kIconSize / size.height());
        size.set_height(kIconSize);
      }

      GdkPixbuf* old = pixbuf;
      pixbuf = gdk_pixbuf_scale_simple(pixbuf, size.width(), size.height(),
                                       GDK_INTERP_BILINEAR);
      g_object_unref(old);
    }

    // Put Icon in top of the left column.
    GtkWidget* icon_column = gtk_vbox_new(FALSE, 0);
    // Use 3 pixel padding to get visual balance with BubbleGtk border on the
    // left.
    gtk_box_pack_start(GTK_BOX(bubble_content), icon_column, FALSE, FALSE,
                       kIconPadding);
    GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_box_pack_start(GTK_BOX(icon_column), image, FALSE, FALSE, 0);
  }

  // Center text column.
  GtkWidget* text_column = gtk_vbox_new(FALSE, kTextColumnVerticalSpacing);
  gtk_box_pack_start(GTK_BOX(bubble_content), text_column, FALSE, FALSE, 0);

  // Heading label.
  GtkWidget* heading_label = gtk_label_new(NULL);
  string16 extension_name = UTF8ToUTF16(bubble_.extension()->name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  std::string heading_text = l10n_util::GetStringFUTF8(
      IDS_EXTENSION_INSTALLED_HEADING, extension_name);
  char* markup = g_markup_printf_escaped("<span size=\"larger\">%s</span>",
      heading_text.c_str());
  gtk_label_set_markup(GTK_LABEL(heading_label), markup);
  g_free(markup);

  gtk_util::SetLabelWidth(heading_label, kTextColumnWidth);
  gtk_box_pack_start(GTK_BOX(text_column), heading_label, FALSE, FALSE, 0);

  bool has_keybinding = false;

  // Browser action label.
  if (bubble_.type() == bubble_.BROWSER_ACTION) {
    extensions::CommandService* command_service =
        extensions::CommandService::Get(bubble_.browser()->profile());
    extensions::Command browser_action_command;
    GtkWidget* info_label;
    if (!command_service->GetBrowserActionCommand(
            bubble_.extension()->id(),
            extensions::CommandService::ACTIVE_ONLY,
            &browser_action_command,
            NULL)) {
      info_label = gtk_label_new(l10n_util::GetStringUTF8(
          IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO).c_str());
    } else {
      info_label = gtk_label_new(l10n_util::GetStringFUTF8(
          IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO_WITH_SHORTCUT,
          browser_action_command.accelerator().GetShortcutText()).c_str());
      has_keybinding = true;
    }
    gtk_util::SetLabelWidth(info_label, kTextColumnWidth);
    gtk_box_pack_start(GTK_BOX(text_column), info_label, FALSE, FALSE, 0);
  }

  // Page action label.
  if (bubble_.type() == bubble_.PAGE_ACTION) {
    extensions::CommandService* command_service =
        extensions::CommandService::Get(bubble_.browser()->profile());
    extensions::Command page_action_command;
    GtkWidget* info_label;
    if (!command_service->GetPageActionCommand(
            bubble_.extension()->id(),
            extensions::CommandService::ACTIVE_ONLY,
            &page_action_command,
            NULL)) {
      info_label = gtk_label_new(l10n_util::GetStringUTF8(
          IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO).c_str());
    } else {
      info_label = gtk_label_new(l10n_util::GetStringFUTF8(
          IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO_WITH_SHORTCUT,
          page_action_command.accelerator().GetShortcutText()).c_str());
      has_keybinding = true;
    }
    gtk_util::SetLabelWidth(info_label, kTextColumnWidth);
    gtk_box_pack_start(GTK_BOX(text_column), info_label, FALSE, FALSE, 0);
  }

  // Omnibox keyword label.
  if (bubble_.type() == bubble_.OMNIBOX_KEYWORD) {
    GtkWidget* info_label = gtk_label_new(l10n_util::GetStringFUTF8(
        IDS_EXTENSION_INSTALLED_OMNIBOX_KEYWORD_INFO,
        UTF8ToUTF16(extensions::OmniboxInfo::GetKeyword(
            bubble_.extension()))).c_str());
    gtk_util::SetLabelWidth(info_label, kTextColumnWidth);
    gtk_box_pack_start(GTK_BOX(text_column), info_label, FALSE, FALSE, 0);
  }

  if (has_keybinding) {
    GtkWidget* manage_link = theme_provider->BuildChromeLinkButton(
        l10n_util::GetStringUTF8(IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS));
    GtkWidget* link_hbox = gtk_hbox_new(FALSE, 0);
    // Stick it in an hbox so it doesn't expand to the whole width.
    gtk_box_pack_end(GTK_BOX(link_hbox), manage_link, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(text_column), link_hbox, FALSE, FALSE, 0);
    g_signal_connect(manage_link, "clicked",
                     G_CALLBACK(OnLinkClickedThunk), this);
  } else {
    // Manage label.
    GtkWidget* manage_label = gtk_label_new(
        l10n_util::GetStringUTF8(IDS_EXTENSION_INSTALLED_MANAGE_INFO).c_str());
    gtk_util::SetLabelWidth(manage_label, kTextColumnWidth);
    gtk_box_pack_start(GTK_BOX(text_column), manage_label, FALSE, FALSE, 0);
  }

  // Create and pack the close button.
  GtkWidget* close_column = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(bubble_content), close_column, FALSE, FALSE, 0);
  close_button_.reset(CustomDrawButton::CloseButtonBubble(theme_provider));
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);
  gtk_box_pack_start(GTK_BOX(close_column), close_button_->widget(),
      FALSE, FALSE, 0);

  BubbleGtk::FrameStyle frame_style = BubbleGtk::ANCHOR_TOP_RIGHT;

  gfx::Rect bounds = gtk_util::WidgetBounds(reference_widget);
  if (bubble_.type() == bubble_.OMNIBOX_KEYWORD) {
    // Reverse the arrow for omnibox keywords, since the bubble will be on the
    // other side of the window. We also clear the width to avoid centering
    // the popup on the URL bar.
    frame_style = BubbleGtk::ANCHOR_TOP_LEFT;
    if (base::i18n::IsRTL())
      bounds.Offset(bounds.width(), 0);
    bounds.set_width(0);
  }

  gtk_bubble_ = BubbleGtk::Show(reference_widget,
                                &bounds,
                                bubble_content,
                                frame_style,
                                BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                                theme_provider,
                                this);
  g_signal_connect(bubble_content, "destroy",
                   G_CALLBACK(&OnDestroyThunk), this);

  // gtk_bubble_ is now the owner of |this| and deletes it when the bubble
  // goes away.
  bubble_.IgnoreBrowserClosing();
  return true;
}

// static
void ExtensionInstalledBubbleGtk::OnButtonClick(GtkWidget* button,
    ExtensionInstalledBubbleGtk* bubble) {
  if (button == bubble->close_button_->widget()) {
    bubble->gtk_bubble_->Close();
  } else {
    NOTREACHED();
  }
}

void ExtensionInstalledBubbleGtk::OnLinkClicked(GtkWidget* widget) {
  gtk_bubble_->Close();

  std::string configure_url = chrome::kChromeUIExtensionsURL;
  configure_url += chrome::kExtensionConfigureCommandsSubPage;
  chrome::NavigateParams params(
      chrome::GetSingletonTabNavigateParams(
          bubble_.browser(), GURL(configure_url.c_str())));
  chrome::Navigate(&params);
}

void ExtensionInstalledBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                                bool closed_by_escape) {
  if (bubble_.extension() && bubble_.type() == bubble_.PAGE_ACTION) {
    // Turn the page action preview off.
    BrowserWindowGtk* browser_window =
          BrowserWindowGtk::GetBrowserWindowForNativeWindow(
              bubble_.browser()->window()->GetNativeWindow());
    LocationBarViewGtk* location_bar_view =
        browser_window->GetToolbar()->GetLocationBarView();
    location_bar_view->SetPreviewEnabledPageAction(
        ExtensionActionManager::Get(bubble_.browser()->profile())->
        GetPageAction(*bubble_.extension()),
        false);  // preview_enabled
  }
}
