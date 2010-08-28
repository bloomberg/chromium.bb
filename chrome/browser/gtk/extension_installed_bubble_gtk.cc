// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/extension_installed_bubble_gtk.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_actions_toolbar_gtk.h"
#include "chrome/browser/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/location_bar_view_gtk.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "gfx/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

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

// Padding between content and edge of info bubble.
const int kContentBorder = 7;

}  // namespace

void ExtensionInstalledBubbleGtk::Show(Extension* extension, Browser* browser,
                                       SkBitmap icon) {
  new ExtensionInstalledBubbleGtk(extension, browser, icon);
}

ExtensionInstalledBubbleGtk::ExtensionInstalledBubbleGtk(Extension *extension,
                                                         Browser *browser,
                                                         SkBitmap icon)
    : extension_(extension),
      browser_(browser),
      icon_(icon),
      animation_wait_retries_(kAnimationWaitRetries) {
  AddRef();  // Balanced in Close().

  if (extension_->browser_action()) {
    type_ = BROWSER_ACTION;
  } else if (extension->page_action() &&
             !extension->page_action()->default_icon_path().empty()) {
    type_ = PAGE_ACTION;
  } else {
    type_ = GENERIC;
  }

  // |extension| has been initialized but not loaded at this point. We need
  // to wait on showing the Bubble until not only the EXTENSION_LOADED gets
  // fired, but all of the EXTENSION_LOADED Observers have run. Only then can we
  // be sure that a browser action or page action has had views created which we
  // can inspect for the purpose of pointing to them.
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
      NotificationService::AllSources());
}

void ExtensionInstalledBubbleGtk::Observe(NotificationType type,
                                          const NotificationSource& source,
                                          const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_LOADED) {
    Extension* extension = Details<Extension>(details).ptr();
    if (extension == extension_) {
      // PostTask to ourself to allow all EXTENSION_LOADED Observers to run.
      MessageLoopForUI::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
          &ExtensionInstalledBubbleGtk::ShowInternal));
    }
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

void ExtensionInstalledBubbleGtk::ShowInternal() {
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(
          browser_->window()->GetNativeHandle());

  GtkWidget* reference_widget = NULL;

  if (type_ == BROWSER_ACTION) {
    BrowserActionsToolbarGtk* toolbar =
        browser_window->GetToolbar()->GetBrowserActionsToolbar();

    if (toolbar->animating() && animation_wait_retries_-- > 0) {
      MessageLoopForUI::current()->PostDelayedTask(
          FROM_HERE,
          NewRunnableMethod(this, &ExtensionInstalledBubbleGtk::ShowInternal),
          kAnimationWaitMS);
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
    if (reference_widget && !GTK_WIDGET_VISIBLE(reference_widget)) {
      reference_widget = GTK_WIDGET_VISIBLE(toolbar->chevron()) ?
          toolbar->chevron() : NULL;
    }
  } else if (type_ == PAGE_ACTION) {
    LocationBarViewGtk* location_bar_view =
        browser_window->GetToolbar()->GetLocationBarView();
    location_bar_view->SetPreviewEnabledPageAction(extension_->page_action(),
                                                   true);  // preview_enabled
    reference_widget = location_bar_view->GetPageActionWidget(
        extension_->page_action());
    // glib delays recalculating layout, but we need reference_widget to know
    // it's coordinates, so we force a check_resize here.
    gtk_container_check_resize(GTK_CONTAINER(
        browser_window->GetToolbar()->widget()));
    DCHECK(reference_widget);
  }
  // Default case.
  if (reference_widget == NULL)
    reference_widget = browser_window->GetToolbar()->GetAppMenuButton();

  GtkThemeProvider* theme_provider = GtkThemeProvider::GetFrom(
      browser_->profile());

  // Setup the InfoBubble content.
  GtkWidget* bubble_content = gtk_hbox_new(FALSE, kHorizontalColumnSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(bubble_content), kContentBorder);

  // Scale icon down to 43x43, but allow smaller icons (don't scale up).
  GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&icon_);
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
  // Use 3 pixel padding to get visual balance with InfoBubble border on the
  // left.
  gtk_box_pack_start(GTK_BOX(bubble_content), icon_column, FALSE, FALSE,
      kIconPadding);
  GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
  g_object_unref(pixbuf);
  gtk_box_pack_start(GTK_BOX(icon_column), image, FALSE, FALSE, 0);

  // Center text column.
  GtkWidget* text_column = gtk_vbox_new(FALSE, kTextColumnVerticalSpacing);
  gtk_box_pack_start(GTK_BOX(bubble_content), text_column, FALSE, FALSE, 0);

  // Heading label
  GtkWidget* heading_label = gtk_label_new(NULL);
  std::string heading_text = l10n_util::GetStringFUTF8(
      IDS_EXTENSION_INSTALLED_HEADING, UTF8ToUTF16(extension_->name()));
  char* markup = g_markup_printf_escaped("<span size=\"larger\">%s</span>",
      heading_text.c_str());
  gtk_label_set_markup(GTK_LABEL(heading_label), markup);
  g_free(markup);

  gtk_label_set_line_wrap(GTK_LABEL(heading_label), TRUE);
  gtk_widget_set_size_request(heading_label, kTextColumnWidth, -1);
  gtk_box_pack_start(GTK_BOX(text_column), heading_label, FALSE, FALSE, 0);

  // Page action label
  if (type_ == ExtensionInstalledBubbleGtk::PAGE_ACTION) {
    GtkWidget* info_label = gtk_label_new(l10n_util::GetStringUTF8(
            IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO).c_str());
    gtk_label_set_line_wrap(GTK_LABEL(info_label), TRUE);
    gtk_widget_set_size_request(info_label, kTextColumnWidth, -1);
    gtk_box_pack_start(GTK_BOX(text_column), info_label, FALSE, FALSE, 0);
  }

  // Manage label
  GtkWidget* manage_label = gtk_label_new(
      l10n_util::GetStringFUTF8(IDS_EXTENSION_INSTALLED_MANAGE_INFO,
          UTF8ToUTF16(extension_->name())).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(manage_label), TRUE);
  gtk_widget_set_size_request(manage_label, kTextColumnWidth, -1);
  gtk_box_pack_start(GTK_BOX(text_column), manage_label, FALSE, FALSE, 0);

  // Create and pack the close button.
  GtkWidget* close_column = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(bubble_content), close_column, FALSE, FALSE, 0);
  close_button_.reset(CustomDrawButton::CloseButton(theme_provider));
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  close_button_->SetBackground(
      theme_provider->GetColor(BrowserThemeProvider::COLOR_TAB_TEXT),
      rb.GetBitmapNamed(IDR_CLOSE_BAR),
      rb.GetBitmapNamed(IDR_CLOSE_BAR_MASK));
  gtk_box_pack_start(GTK_BOX(close_column), close_button_->widget(),
      FALSE, FALSE, 0);

  InfoBubbleGtk::ArrowLocationGtk arrow_location =
      !base::i18n::IsRTL() ?
      InfoBubbleGtk::ARROW_LOCATION_TOP_RIGHT :
      InfoBubbleGtk::ARROW_LOCATION_TOP_LEFT;
  info_bubble_ = InfoBubbleGtk::Show(reference_widget,
      NULL,
      bubble_content,
      arrow_location,
      true,  // match_system_theme
      true,  // grab_input
      theme_provider,
      this);
}

// static
void ExtensionInstalledBubbleGtk::OnButtonClick(GtkWidget* button,
    ExtensionInstalledBubbleGtk* bubble) {
  if (button == bubble->close_button_->widget()) {
    bubble->info_bubble_->Close();
  } else {
    NOTREACHED();
  }
}
// InfoBubbleDelegate
void ExtensionInstalledBubbleGtk::InfoBubbleClosing(InfoBubbleGtk* info_bubble,
                                                    bool closed_by_escape) {
  if (extension_->page_action()) {
    // Turn the page action preview off.
    BrowserWindowGtk* browser_window =
          BrowserWindowGtk::GetBrowserWindowForNativeWindow(
              browser_->window()->GetNativeHandle());
    LocationBarViewGtk* location_bar_view =
        browser_window->GetToolbar()->GetLocationBarView();
    location_bar_view->SetPreviewEnabledPageAction(extension_->page_action(),
                                                   false);  // preview_enabled
  }

  // We need to allow the info bubble to close and remove the widgets from
  // the window before we call Release() because close_button_ depends
  // on all references being cleared before it is destroyed.
  MessageLoopForUI::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
      &ExtensionInstalledBubbleGtk::Close));
}

void ExtensionInstalledBubbleGtk::Close() {
  Release();  // Balanced in ctor.
  info_bubble_ = NULL;
}
