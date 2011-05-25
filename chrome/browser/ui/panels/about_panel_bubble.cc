// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/panels/about_panel_bubble.h"

#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/extension.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/controls/textfield/textfield.h"
#include "views/window/window.h"

namespace {

// Extra padding to put around content over what the InfoBubble provides.
const int kBubblePadding = 4;

// Horizontal spacing between the icon and the contents.
const int kIconHorizontalSpacing = 4;

// Vertical spacing between two controls.
const int kControlVerticalSpacing = 10;

// Horizontal spacing between the text and the left/right of a description.
const int kDescriptionHorizontalSpacing = 6;

// Vertical spacing between the text and the top/bottom of a description.
const int kDescriptionVertialSpacing = 4;

// Horizontal spacing between two links.
const int kLinksHorizontalSpacing = 20;

// Text color of a description.
const SkColor kDescriptionTextColor = SK_ColorBLACK;

// Background color of a description.
const SkColor kDescriptionBackgroundColor = 0xFFE8E8EE;

}

// AboutPanelBubbleView --------------------------------------------------------

AboutPanelBubble::AboutPanelBubbleView::AboutPanelBubbleView(
    SkBitmap icon, Browser* browser, const Extension* extension)
    : icon_(NULL),
      title_(NULL),
      install_date_(NULL),
      description_(NULL),
      uninstall_link_(NULL),
      report_abuse_link_(NULL) {
  const gfx::Font& font =
      ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BaseFont);

  icon_ = new views::ImageView();
  icon_->SetImage(icon);
  AddChildView(icon_);

  title_ = new views::Label(UTF8ToWide(extension->name()));
  title_->SetFont(font.DeriveFont(0, gfx::Font::BOLD));
  title_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(title_);

  base::Time install_time = browser->GetProfile()->GetExtensionService()->
      extension_prefs()->GetInstallTime(extension->id());
  install_date_ = new views::Label(UTF16ToWide(
      l10n_util::GetStringFUTF16(
          IDS_ABOUT_PANEL_BUBBLE_EXTENSION_INSTALL_DATE,
          base::TimeFormatFriendlyDate(install_time))));
  install_date_->SetMultiLine(true);
  install_date_->SetFont(font);
  install_date_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  install_date_->SizeToFit(GetPreferredSize().width() - kBubblePadding * 2);
  AddChildView(install_date_);

  description_ = new views::Textfield(views::Textfield::STYLE_MULTILINE);
  description_->SetText(UTF8ToUTF16(extension->description()));
  description_->SetHeightInLines(2);
  description_->SetHorizontalMargins(kDescriptionHorizontalSpacing,
                                          kDescriptionHorizontalSpacing);
  description_->SetVerticalMargins(kDescriptionVertialSpacing,
                                        kDescriptionVertialSpacing);
  description_->SetFont(font);
  description_->SetTextColor(kDescriptionTextColor);
  description_->SetBackgroundColor(kDescriptionBackgroundColor);
  description_->RemoveBorder();
  description_->SetReadOnly(true);
  AddChildView(description_);

  uninstall_link_ = new views::Link(UTF16ToWide(
      l10n_util::GetStringFUTF16(IDS_ABOUT_PANEL_BUBBLE_UNINSTALL_EXTENSION,
                                 UTF8ToUTF16(extension->name()))));
  AddChildView(uninstall_link_);

  report_abuse_link_ = new views::Link(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_ABOUT_PANEL_BUBBLE_REPORT_ABUSE)));
  AddChildView(report_abuse_link_);
}

void AboutPanelBubble::AboutPanelBubbleView::Layout() {
  gfx::Size icon_size = icon_->GetPreferredSize();
  icon_->SetBounds(kBubblePadding,
                   kBubblePadding,
                   icon_size.width(),
                   icon_size.height());

  gfx::Size canvas = GetPreferredSize();
  int content_left_margin =
      kBubblePadding + icon_size.width() + kIconHorizontalSpacing;
  int content_width = canvas.width() - kBubblePadding - content_left_margin;

  gfx::Size pref_size = title_->GetPreferredSize();
  title_->SetBounds(content_left_margin,
                    kBubblePadding,
                    content_width,
                    pref_size.height());

  int next_y = title_->bounds().bottom() + kControlVerticalSpacing;

  pref_size = install_date_->GetPreferredSize();
  install_date_->SetBounds(content_left_margin,
                           next_y,
                           content_width,
                           pref_size.height());

  next_y = install_date_->bounds().bottom() + kControlVerticalSpacing;

  pref_size = description_->GetPreferredSize();
  description_->SetBounds(
      content_left_margin,
      next_y,
      content_width,
      pref_size.height() + kDescriptionVertialSpacing * 2);

  next_y = description_->bounds().bottom() + kControlVerticalSpacing;

  pref_size = uninstall_link_->GetPreferredSize();
  uninstall_link_->SetBounds(content_left_margin,
                             next_y,
                             pref_size.width(),
                             pref_size.height());

  pref_size = report_abuse_link_->GetPreferredSize();
  report_abuse_link_->SetBounds(
      content_left_margin + uninstall_link_->width() + kLinksHorizontalSpacing,
      next_y,
      pref_size.width(),
      pref_size.height());
}

gfx::Size AboutPanelBubble::AboutPanelBubbleView::GetPreferredSize() {
  return views::Window::GetLocalizedContentsSize(
      IDS_ABOUTPANELBUBBLE_WIDTH_CHARS,
      IDS_ABOUTPANELBUBBLE_HEIGHT_LINES);
}

void AboutPanelBubble::AboutPanelBubbleView::LinkClicked(views::Link* source,
                                                         int event_flags) {
  NOTIMPLEMENTED();
}

// AboutPanelBubble ------------------------------------------------------------

// static
AboutPanelBubble* AboutPanelBubble::Show(
    views::Widget* parent,
    const gfx::Rect& position_relative_to,
    BubbleBorder::ArrowLocation arrow_location,
    SkBitmap icon,
    Browser* browser) {
  // Find the extension. When we create a panel from an extension, the extension
  // ID is passed as the app name to the Browser.
  ExtensionService* extension_service =
      browser->GetProfile()->GetExtensionService();
  const Extension* extension = extension_service->GetExtensionById(
      web_app::GetExtensionIdFromApplicationName(browser->app_name()), false);
  if (!extension)
    return NULL;

  AboutPanelBubble* bubble = new AboutPanelBubble();
  AboutPanelBubbleView* view = new AboutPanelBubbleView(
      icon, browser, extension);
  bubble->InitBubble(
      parent, position_relative_to, arrow_location, view, bubble);
  return bubble;
}

AboutPanelBubble::AboutPanelBubble() {
}

bool AboutPanelBubble::CloseOnEscape() {
  return true;
}

bool AboutPanelBubble::FadeInOnShow() {
  return false;
}

std::wstring AboutPanelBubble::accessible_name() {
  return L"AboutPanelBubble";
}
