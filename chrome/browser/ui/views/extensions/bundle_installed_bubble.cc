// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

using extensions::BundleInstaller;
using views::GridLayout;

namespace {

// The ID of the column set for the bubble.
const int kColumnSetId = 0;

// The width of the left column.
const int kLeftColumnWidth = 325;

class BundleInstalledBubble : public views::BubbleDelegateView,
                              public views::ButtonListener {
 public:
  BundleInstalledBubble(const BundleInstaller* bundle,
                        View* anchor_view,
                        views::BubbleBorder::Arrow arrow)
      : views::BubbleDelegateView(anchor_view, arrow) {
    GridLayout* layout = GridLayout::CreatePanel(this);
    SetLayoutManager(layout);
    views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);

    column_set->AddColumn(GridLayout::LEADING,
                          GridLayout::FILL,
                          0,  // no resizing
                          GridLayout::USE_PREF,
                          0,  // no fixed with
                          kLeftColumnWidth);
    column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
    column_set->AddColumn(GridLayout::LEADING,
                          GridLayout::LEADING,
                          0,  // no resizing
                          GridLayout::USE_PREF,
                          0,  // no fixed width
                          0); // no min width (only holds close button)

    layout->StartRow(0, kColumnSetId);

    AddContent(layout, bundle);
  }

  virtual ~BundleInstalledBubble() {}

 private:
  void AddContent(GridLayout* layout, const BundleInstaller* bundle) {
    base::string16 installed_heading = bundle->GetHeadingTextFor(
        BundleInstaller::Item::STATE_INSTALLED);
    base::string16 failed_heading = bundle->GetHeadingTextFor(
        BundleInstaller::Item::STATE_FAILED);

    // Insert the list of installed items.
    if (!installed_heading.empty()) {
      layout->StartRow(0, kColumnSetId);
      AddHeading(layout, installed_heading);
      AddCloseButton(layout, this);
      AddItemList(layout, bundle->GetItemsWithState(
          BundleInstaller::Item::STATE_INSTALLED));

      // Insert a line of padding if we're showing both sections.
      if (!failed_heading.empty())
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    }

    // Insert the list of failed items.
    if (!failed_heading.empty()) {
      layout->StartRow(0, kColumnSetId);
      AddHeading(layout, failed_heading);

      // The close button should be in the second column of the first row, so
      // we add it here if there was no installed items section.
      if (installed_heading.empty())
        AddCloseButton(layout, this);

      AddItemList(layout, bundle->GetItemsWithState(
          BundleInstaller::Item::STATE_FAILED));
    }

    views::BubbleDelegateView::CreateBubble(this)->Show();
  }

  void AddItemList(GridLayout* layout, const BundleInstaller::ItemList& items) {
    for (size_t i = 0; i < items.size(); ++i) {
      base::string16 extension_name =
          base::UTF8ToUTF16(items[i].localized_name);
      base::i18n::AdjustStringForLocaleDirection(&extension_name);
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, kColumnSetId);
      views::Label* extension_label = new views::Label(
          l10n_util::GetStringFUTF16(
              IDS_EXTENSION_PERMISSION_LINE, extension_name));
      extension_label->SetMultiLine(true);
      extension_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      extension_label->SizeToFit(kLeftColumnWidth);
      layout->AddView(extension_label);
    }
  }

  void AddCloseButton(GridLayout* layout, views::ButtonListener* listener) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

    views::ImageButton* button = new views::ImageButton(listener);
    button->SetImage(views::CustomButton::STATE_NORMAL,
                     rb.GetImageSkiaNamed(IDR_CLOSE_2));
    button->SetImage(views::CustomButton::STATE_HOVERED,
                     rb.GetImageSkiaNamed(IDR_CLOSE_2_H));
    button->SetImage(views::CustomButton::STATE_PRESSED,
                     rb.GetImageSkiaNamed(IDR_CLOSE_2_P));
    layout->AddView(button);
  }

  void AddHeading(GridLayout* layout, const base::string16& heading) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    views::Label* heading_label = new views::Label(
        heading, rb.GetFontList(ui::ResourceBundle::MediumFont));
    heading_label->SetMultiLine(true);
    heading_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    heading_label->SizeToFit(kLeftColumnWidth);
    layout->AddView(heading_label);
  }

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    GetWidget()->Close();
  }

  DISALLOW_COPY_AND_ASSIGN(BundleInstalledBubble);
};

}  // namespace

void BundleInstaller::ShowInstalledBubble(
    const BundleInstaller* bundle, Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor = browser_view->GetToolbarView()->app_menu();
  new BundleInstalledBubble(bundle, anchor, views::BubbleBorder::TOP_RIGHT);
}
