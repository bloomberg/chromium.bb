// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/extensions/bundle_installed_bubble_gtk.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"

using extensions::BundleInstaller;

namespace {

// The horizontal spacing for the main content area.
const int kHorizontalColumnSpacing = 10;

// The vertical spacing for the text area.
const int kTextColumnVerticalSpacing = 7;

// The width of the content area.
const int kContentWidth = 350;

// The padding for list items.
const int kListItemPadding = 2;

// Padding between content and edge of bubble.
const int kContentPadding = 12;

}  // namespace

// static
void BundleInstaller::ShowInstalledBubble(
    const BundleInstaller* bundle, Browser* browser) {
  new BundleInstalledBubbleGtk(bundle, browser);
}

BundleInstalledBubbleGtk::BundleInstalledBubbleGtk(
    const BundleInstaller* bundle, Browser* browser)
    : browser_(browser),
      bubble_(NULL) {
  AddRef();  // Balanced in Close().
  ShowInternal(bundle);
}

BundleInstalledBubbleGtk::~BundleInstalledBubbleGtk() {}

void BundleInstalledBubbleGtk::ShowInternal(const BundleInstaller* bundle) {
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(
          browser_->window()->GetNativeWindow());

  GtkThemeService* theme_provider = GtkThemeService::GetFrom(
      browser_->profile());

  // Anchor the bubble to the wrench menu.
  GtkWidget* reference_widget =
      browser_window->GetToolbar()->GetAppMenuButton();

  GtkWidget* bubble_content = gtk_hbox_new(FALSE, kHorizontalColumnSpacing);
  gtk_container_set_border_width(
      GTK_CONTAINER(bubble_content), kContentPadding);

  GtkWidget* text_column = gtk_vbox_new(FALSE, kTextColumnVerticalSpacing);
  gtk_box_pack_start(GTK_BOX(bubble_content), text_column, FALSE, FALSE, 0);

  InsertExtensionList(
      text_column, bundle, BundleInstaller::Item::STATE_INSTALLED);
  InsertExtensionList(text_column, bundle, BundleInstaller::Item::STATE_FAILED);

  // Close button
  GtkWidget* close_column = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(bubble_content), close_column, FALSE, FALSE, 0);
  close_button_.reset(CustomDrawButton::CloseButtonBubble(theme_provider));
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnButtonClick), this);
  gtk_box_pack_start(GTK_BOX(close_column), close_button_->widget(),
      FALSE, FALSE, 0);

  gfx::Rect bounds = gtk_util::WidgetBounds(reference_widget);

  bubble_ = BubbleGtk::Show(reference_widget,
                            &bounds,
                            bubble_content,
                            BubbleGtk::ANCHOR_TOP_RIGHT,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_provider,
                            this);
}

void BundleInstalledBubbleGtk::InsertExtensionList(
    GtkWidget* parent,
    const BundleInstaller* bundle,
    BundleInstaller::Item::State state) {
  string16 heading = bundle->GetHeadingTextFor(state);
  BundleInstaller::ItemList items = bundle->GetItemsWithState(state);
  if (heading.empty() || items.empty())
    return;

  GtkWidget* heading_label = gtk_util::CreateBoldLabel(UTF16ToUTF8(heading));
  gtk_util::SetLabelWidth(heading_label, kContentWidth);
  gtk_box_pack_start(GTK_BOX(parent), heading_label, FALSE, FALSE, 0);

  for (size_t i = 0; i < items.size(); ++i) {
    GtkWidget* extension_label = gtk_label_new(UTF16ToUTF8(
        items[i].GetNameForDisplay()).c_str());
    gtk_util::SetLabelWidth(extension_label, kContentWidth);
    gtk_box_pack_start(GTK_BOX(parent), extension_label, false, false,
                       kListItemPadding);
  }
}

void BundleInstalledBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                             bool closed_by_escape) {
  // We need to allow the bubble to close and remove the widgets from
  // the window before we call Release() because close_button_ depends
  // on all references being cleared before it is destroyed.
  MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&BundleInstalledBubbleGtk::Close, this));
}

void BundleInstalledBubbleGtk::Close() {
  bubble_ = NULL;

  Release();  // Balanced in BundleInstalledBubbleGtk().
}

void BundleInstalledBubbleGtk::OnButtonClick(GtkWidget* button,
                                             BundleInstalledBubbleGtk* bubble) {
  if (button == bubble->close_button_->widget())
    bubble->bubble_->Close();
  else
    NOTREACHED();
}
