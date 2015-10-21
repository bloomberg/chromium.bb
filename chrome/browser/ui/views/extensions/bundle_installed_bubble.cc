// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

using extensions::BundleInstaller;
using views::GridLayout;

namespace {

// The ID of the column set that holds the headings and the close button.
const int kHeadingColumnSetId = 0;

// The width of the columns that hold the heading texts and extension names.
const int kTextColumnWidth = 325;

// The ID of the column set that holds extension icons and names.
const int kItemsColumnSetId = 1;

// The size of extension icons, and width of the corresponding column.
const int kIconSize = 32;

class BundleInstalledBubble : public views::BubbleDelegateView,
                              public views::ButtonListener {
 public:
  BundleInstalledBubble(const BundleInstaller* bundle,
                        View* anchor_view,
                        views::BubbleBorder::Arrow arrow)
      : views::BubbleDelegateView(anchor_view, arrow) {
    GridLayout* layout = GridLayout::CreatePanel(this);
    SetLayoutManager(layout);

    views::ColumnSet* heading_column_set =
        layout->AddColumnSet(kHeadingColumnSetId);
    heading_column_set->AddColumn(GridLayout::LEADING,
                                  GridLayout::FILL,
                                  100.0f,  // take all available space
                                  GridLayout::USE_PREF,
                                  0,  // no fixed width
                                  kTextColumnWidth);
    heading_column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
    heading_column_set->AddColumn(GridLayout::TRAILING,
                                  GridLayout::LEADING,
                                  0,  // no resizing
                                  GridLayout::USE_PREF,
                                  0,  // no fixed width
                                  0); // no min width (only holds close button)

    views::ColumnSet* items_column_set =
        layout->AddColumnSet(kItemsColumnSetId);
    items_column_set->AddColumn(GridLayout::CENTER,
                                GridLayout::CENTER,
                                0,  // no resizing
                                GridLayout::FIXED,
                                kIconSize,
                                kIconSize);
    items_column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
    items_column_set->AddColumn(GridLayout::LEADING,
                                GridLayout::CENTER,
                                100.0f,  // take all available space
                                GridLayout::USE_PREF,
                                0,  // no fixed width
                                kTextColumnWidth);

    AddContent(layout, bundle);
  }

  ~BundleInstalledBubble() override {}

 private:
  void AddContent(GridLayout* layout, const BundleInstaller* bundle) {
    bool has_installed_items = bundle->HasItemWithState(
        BundleInstaller::Item::STATE_INSTALLED);
    bool has_failed_items = bundle->HasItemWithState(
        BundleInstaller::Item::STATE_FAILED);

    // Insert the list of installed items.
    if (has_installed_items) {
      layout->StartRow(0, kHeadingColumnSetId);
      AddHeading(layout, bundle->GetHeadingTextFor(
          BundleInstaller::Item::STATE_INSTALLED));
      AddCloseButton(layout, this);
      AddItemList(layout, bundle->GetItemsWithState(
          BundleInstaller::Item::STATE_INSTALLED));

      // Insert a line of padding if we're showing both sections.
      if (has_failed_items)
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
    }

    // Insert the list of failed items.
    if (has_failed_items) {
      layout->StartRow(0, kHeadingColumnSetId);
      AddHeading(layout, bundle->GetHeadingTextFor(
          BundleInstaller::Item::STATE_FAILED));

      // The close button should be in the second column of the first row, so
      // we add it here if there was no installed items section.
      if (!has_installed_items)
        AddCloseButton(layout, this);

      AddItemList(layout, bundle->GetItemsWithState(
          BundleInstaller::Item::STATE_FAILED));
    }

    views::BubbleDelegateView::CreateBubble(this)->Show();
  }

  void AddItemList(GridLayout* layout, const BundleInstaller::ItemList& items) {
    for (const BundleInstaller::Item& item : items) {
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, kItemsColumnSetId);
      gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(item.icon);
      gfx::Size size(image.width(), image.height());
      if (size.width() > kIconSize || size.height() > kIconSize)
        size = gfx::Size(kIconSize, kIconSize);
      views::ImageView* image_view = new views::ImageView;
      image_view->SetImage(image);
      image_view->SetImageSize(size);
      layout->AddView(image_view);

      views::Label* extension_label =
          new views::Label(item.GetNameForDisplay());
      extension_label->SetMultiLine(true);
      extension_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
      extension_label->SizeToFit(kTextColumnWidth);
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
    heading_label->SizeToFit(kTextColumnWidth);
    layout->AddView(heading_label);
  }

  // views::ButtonListener implementation:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override {
    GetWidget()->Close();
  }

  DISALLOW_COPY_AND_ASSIGN(BundleInstalledBubble);
};

}  // namespace

void BundleInstaller::ShowInstalledBubble(
    const BundleInstaller* bundle, Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  views::View* anchor = browser_view->GetToolbarView()->app_menu_button();
  new BundleInstalledBubble(bundle, anchor, views::BubbleBorder::TOP_RIGHT);
}
