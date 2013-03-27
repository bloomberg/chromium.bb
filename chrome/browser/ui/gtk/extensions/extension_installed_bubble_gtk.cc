// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/extension_installed_bubble_gtk.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
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
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
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

// When showing the bubble for a new browser action, we may have to wait for
// the toolbar to finish animating to know where the item's final position
// will be.
const int kAnimationWaitRetries = 10;
const int kAnimationWaitMS = 50;

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
    : extension_(extension),
      browser_(browser),
      icon_(icon),
      animation_wait_retries_(kAnimationWaitRetries),
      bubble_(NULL) {
  AddRef();  // Balanced in Close().

  extensions::ExtensionActionManager* extension_action_manager =
      ExtensionActionManager::Get(browser_->profile());

  if (!extensions::OmniboxInfo::GetKeyword(extension_).empty())
    type_ = OMNIBOX_KEYWORD;
  else if (extension_action_manager->GetBrowserAction(*extension_))
    type_ = BROWSER_ACTION;
  else if (extension_action_manager->GetPageAction(*extension) &&
           extensions::ActionInfo::IsVerboseInstallMessage(extension))
    type_ = PAGE_ACTION;
  else
    type_ = GENERIC;

  // |extension| has been initialized but not loaded at this point. We need
  // to wait on showing the Bubble until not only the EXTENSION_LOADED gets
  // fired, but all of the EXTENSION_LOADED Observers have run. Only then can we
  // be sure that a browser action or page action has had views created which we
  // can inspect for the purpose of pointing to them.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(browser->profile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(browser->profile()));
}

ExtensionInstalledBubbleGtk::~ExtensionInstalledBubbleGtk() {}

void ExtensionInstalledBubbleGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    const Extension* extension =
        content::Details<const Extension>(details).ptr();
    if (extension == extension_) {
      // PostTask to ourself to allow all EXTENSION_LOADED Observers to run.
      MessageLoopForUI::current()->PostTask(
          FROM_HERE,
          base::Bind(&ExtensionInstalledBubbleGtk::ShowInternal, this));
    }
  } else if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
    const Extension* extension =
        content::Details<extensions::UnloadedExtensionInfo>(details)->extension;
    if (extension == extension_)
      extension_ = NULL;
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

void ExtensionInstalledBubbleGtk::ShowInternal() {
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(
          browser_->window()->GetNativeWindow());

  GtkWidget* reference_widget = NULL;

  if (type_ == BROWSER_ACTION) {
    BrowserActionsToolbarGtk* toolbar =
        browser_window->GetToolbar()->GetBrowserActionsToolbar();

    if (toolbar->animating() && animation_wait_retries_-- > 0) {
      MessageLoopForUI::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ExtensionInstalledBubbleGtk::ShowInternal, this),
          base::TimeDelta::FromMilliseconds(kAnimationWaitMS));
      return;
    }

    reference_widget = toolbar->GetBrowserActionWidget(extension_);
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
  } else if (type_ == PAGE_ACTION) {
    LocationBarViewGtk* location_bar_view =
        browser_window->GetToolbar()->GetLocationBarView();
    ExtensionAction* page_action =
        ExtensionActionManager::Get(browser_->profile())->
        GetPageAction(*extension_);
    location_bar_view->SetPreviewEnabledPageAction(page_action,
                                                   true);  // preview_enabled
    reference_widget = location_bar_view->GetPageActionWidget(page_action);
    // glib delays recalculating layout, but we need reference_widget to know
    // its coordinates, so we force a check_resize here.
    gtk_container_check_resize(GTK_CONTAINER(
        browser_window->GetToolbar()->widget()));
    DCHECK(reference_widget);
  } else if (type_ == OMNIBOX_KEYWORD) {
    LocationBarViewGtk* location_bar_view =
        browser_window->GetToolbar()->GetLocationBarView();
    reference_widget = location_bar_view->location_entry_widget();
    DCHECK(reference_widget);
  }

  // Default case.
  if (reference_widget == NULL)
    reference_widget = browser_window->GetToolbar()->GetAppMenuButton();

  GtkThemeService* theme_provider = GtkThemeService::GetFrom(
      browser_->profile());

  // Setup the BubbleGtk content.
  GtkWidget* bubble_content = gtk_hbox_new(FALSE, kHorizontalColumnSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(bubble_content),
                                 ui::kContentAreaBorder);

  if (!icon_.isNull()) {
    // Scale icon down to 43x43, but allow smaller icons (don't scale up).
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(icon_);
    gfx::Size size(icon_.width(), icon_.height());
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
  string16 extension_name = UTF8ToUTF16(extension_->name());
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
  if (type_ == BROWSER_ACTION) {
    extensions::CommandService* command_service =
        extensions::CommandService::Get(browser_->profile());
    extensions::Command browser_action_command;
    GtkWidget* info_label;
    if (!command_service->GetBrowserActionCommand(
            extension_->id(),
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
  if (type_ == PAGE_ACTION) {
    extensions::CommandService* command_service =
        extensions::CommandService::Get(browser_->profile());
    extensions::Command page_action_command;
    GtkWidget* info_label;
    if (!command_service->GetPageActionCommand(
            extension_->id(),
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
  if (type_ == OMNIBOX_KEYWORD) {
    GtkWidget* info_label = gtk_label_new(l10n_util::GetStringFUTF8(
        IDS_EXTENSION_INSTALLED_OMNIBOX_KEYWORD_INFO,
        UTF8ToUTF16(extensions::OmniboxInfo::GetKeyword(extension_))).c_str());
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
  if (type_ == OMNIBOX_KEYWORD) {
    // Reverse the arrow for omnibox keywords, since the bubble will be on the
    // other side of the window. We also clear the width to avoid centering
    // the popup on the URL bar.
    frame_style = BubbleGtk::ANCHOR_TOP_LEFT;
    if (base::i18n::IsRTL())
      bounds.Offset(bounds.width(), 0);
    bounds.set_width(0);
  }

  bubble_ = BubbleGtk::Show(reference_widget,
                            &bounds,
                            bubble_content,
                            frame_style,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_provider,
                            this);
}

// static
void ExtensionInstalledBubbleGtk::OnButtonClick(GtkWidget* button,
    ExtensionInstalledBubbleGtk* bubble) {
  if (button == bubble->close_button_->widget()) {
    bubble->bubble_->Close();
  } else {
    NOTREACHED();
  }
}

void ExtensionInstalledBubbleGtk::OnLinkClicked(GtkWidget* widget) {
  bubble_->Close();

  std::string configure_url = chrome::kChromeUIExtensionsURL;
  configure_url += chrome::kExtensionConfigureCommandsSubPage;
  chrome::NavigateParams params(
      chrome::GetSingletonTabNavigateParams(
      browser_, GURL(configure_url.c_str())));
  chrome::Navigate(&params);
}

void ExtensionInstalledBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                                bool closed_by_escape) {
  if (extension_ && type_ == PAGE_ACTION) {
    // Turn the page action preview off.
    BrowserWindowGtk* browser_window =
          BrowserWindowGtk::GetBrowserWindowForNativeWindow(
              browser_->window()->GetNativeWindow());
    LocationBarViewGtk* location_bar_view =
        browser_window->GetToolbar()->GetLocationBarView();
    location_bar_view->SetPreviewEnabledPageAction(
        ExtensionActionManager::Get(browser_->profile())->
        GetPageAction(*extension_),
        false);  // preview_enabled
  }

  // We need to allow the bubble to close and remove the widgets from
  // the window before we call Release() because close_button_ depends
  // on all references being cleared before it is destroyed.
  MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&ExtensionInstalledBubbleGtk::Close, this));
}

void ExtensionInstalledBubbleGtk::Close() {
  Release();  // Balanced in ctor.
  bubble_ = NULL;
}
