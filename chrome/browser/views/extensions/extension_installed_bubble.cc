// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_installed_bubble.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/location_bar_view.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/label.h"
#include "views/standard_layout.h"
#include "views/view.h"

namespace {

const int kIconSize = 43;

const int kRightColumnWidth = 285;

// The InfoBubble uses a BubbleBorder which adds about 6 pixels of whitespace
// around the content view. We compensate by reducing our outer borders by this
// amount + 4px.
const int kOuterMarginInset = 10;
const int kHorizOuterMargin = kPanelHorizMargin - kOuterMarginInset;
const int kVertOuterMargin = kPanelVertMargin - kOuterMarginInset;

// Interior vertical margin is 8px smaller than standard
const int kVertInnerMargin = kPanelVertMargin - 8;

// The image we use for the close button has three pixels of whitespace padding.
const int kCloseButtonPadding = 3;

// We want to shift the right column (which contains the header and text) up
// 4px to align with icon.
const int kRightcolumnVerticalShift = -4;

// InstalledBubbleContent is the content view which is placed in the
// ExtensionInstalledBubble. It displays the install icon and explanatory
// text about the installed extension.
class InstalledBubbleContent : public views::View,
                               public views::ButtonListener {
 public:
  InstalledBubbleContent(Extension* extension,
                         ExtensionInstalledBubble::BubbleType type,
                         SkBitmap* icon)
      : type_(type),
        info_(NULL) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    const gfx::Font& font = rb.GetFont(ResourceBundle::BaseFont);

    // Scale down to 43x43, but allow smaller icons (don't scale up).
    gfx::Size size(icon->width(), icon->height());
    if (size.width() > kIconSize || size.height() > kIconSize)
      size = gfx::Size(kIconSize, kIconSize);
    icon_ = new views::ImageView();
    icon_->SetImageSize(size);
    icon_->SetImage(*icon);
    AddChildView(icon_);

    heading_ = new views::Label(
        l10n_util::GetStringF(IDS_EXTENSION_INSTALLED_HEADING,
                              UTF8ToWide(extension->name())));
    heading_->SetFont(font.DeriveFont(3, gfx::Font::NORMAL));
    heading_->SetMultiLine(true);
    heading_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(heading_);

    if (type_ == ExtensionInstalledBubble::PAGE_ACTION) {
      info_ = new views::Label(l10n_util::GetString(
          IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO));
      info_->SetFont(font);
      info_->SetMultiLine(true);
      info_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      AddChildView(info_);
    }

    manage_ = new views::Label(l10n_util::GetString(
      IDS_EXTENSION_INSTALLED_MANAGE_INFO));
    manage_->SetFont(font);
    manage_->SetMultiLine(true);
    manage_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(manage_);

    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(views::CustomButton::BS_NORMAL,
        rb.GetBitmapNamed(IDR_CLOSE_BAR));
    close_button_->SetImage(views::CustomButton::BS_HOT,
        rb.GetBitmapNamed(IDR_CLOSE_BAR_H));
    close_button_->SetImage(views::CustomButton::BS_PUSHED,
        rb.GetBitmapNamed(IDR_CLOSE_BAR_P));
    AddChildView(close_button_);
  }

  virtual void ButtonPressed(
      views::Button* sender,
      const views::Event& event) {
    GetWidget()->Close();
  }

 private:
  virtual gfx::Size GetPreferredSize() {
    int width = kHorizOuterMargin;
    width += kIconSize;
    width += kPanelHorizMargin;
    width += kRightColumnWidth;
    width += 2*kPanelHorizMargin;
    width += kHorizOuterMargin;

    int height = kVertOuterMargin;
    height += heading_->GetHeightForWidth(kRightColumnWidth);
    height += kVertInnerMargin;
    if (type_ == ExtensionInstalledBubble::PAGE_ACTION) {
      height += info_->GetHeightForWidth(kRightColumnWidth);
      height += kVertInnerMargin;
    }
    height += manage_->GetHeightForWidth(kRightColumnWidth);
    height += kVertOuterMargin;
    return gfx::Size(width, std::max(height, kIconSize + 2 * kVertOuterMargin));
  }

  virtual void Layout() {
    int x = kHorizOuterMargin;
    int y = kVertOuterMargin;

    icon_->SetBounds(x, y, kIconSize, kIconSize);
    x += kIconSize;
    x += kPanelHorizMargin;

    y += kRightcolumnVerticalShift;
    heading_->SizeToFit(kRightColumnWidth);
    heading_->SetX(x);
    heading_->SetY(y);
    y += heading_->height();
    y += kVertInnerMargin;

    if (type_ == ExtensionInstalledBubble::PAGE_ACTION) {
      info_->SizeToFit(kRightColumnWidth);
      info_->SetX(x);
      info_->SetY(y);
      y += info_->height();
      y += kVertInnerMargin;
    }

    manage_->SizeToFit(kRightColumnWidth);
    manage_->SetX(x);
    manage_->SetY(y);

    x += kRightColumnWidth + 2*kPanelHorizMargin + kHorizOuterMargin -
        close_button_->GetPreferredSize().width();
    y = kVertOuterMargin;
    gfx::Size sz = close_button_->GetPreferredSize();
    // x-1 & y-1 is just slop to get the close button visually aligned with the
    // title text and bubble arrow.
    close_button_->SetBounds(x - 1, y - 1, sz.width(), sz.height());
}

  ExtensionInstalledBubble::BubbleType type_;
  views::ImageView* icon_;
  views::Label* heading_;
  views::Label* info_;
  views::Label* manage_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(InstalledBubbleContent);
};

}  // namespace

void ExtensionInstalledBubble::Show(Extension *extension, Browser *browser,
                                    SkBitmap icon) {
  new ExtensionInstalledBubble(extension, browser, icon);
}

ExtensionInstalledBubble::ExtensionInstalledBubble(Extension *extension,
                                                   Browser *browser,
                                                   SkBitmap icon)
    : extension_(extension),
      browser_(browser),
      icon_(icon) {
  AddRef();  // Balanced in InfoBubbleClosing.

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
  // be sure that a BrowserAction or PageAction has had views created which we
  // can inspect for the purpose of previewing of pointing to them.
  registrar_.Add(this, NotificationType::EXTENSION_LOADED,
      NotificationService::AllSources());
}

void ExtensionInstalledBubble::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_LOADED) {
    Extension* extension = Details<Extension>(details).ptr();
    if (extension == extension_) {
      // PostTask to ourself to allow all EXTENSION_LOADED Observers to run.
      MessageLoopForUI::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
          &ExtensionInstalledBubble::ShowInternal));
    }
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

void ExtensionInstalledBubble::ShowInternal() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      browser_->window()->GetNativeHandle());

  views::View* reference_view = NULL;
  if (type_ == BROWSER_ACTION) {
    reference_view = browser_view->GetToolbarView()->browser_actions()
        ->GetBrowserActionView(extension_);
    DCHECK(reference_view);
  } else if (type_ == PAGE_ACTION) {
    LocationBarView* location_bar_view = browser_view->GetLocationBarView();
    location_bar_view->SetPreviewEnabledPageAction(extension_->page_action(),
                                                   true);  // preview_enabled
    reference_view = location_bar_view->GetPageActionView(
        extension_->page_action());
    DCHECK(reference_view);
  }

  // Default case.
  if (reference_view == NULL)
    reference_view = browser_view->GetToolbarView()->app_menu();

  gfx::Point origin;
  views::View::ConvertPointToScreen(reference_view, &origin);
  gfx::Rect bounds = reference_view->bounds();
  bounds.set_x(origin.x());
  bounds.set_y(origin.y());

  views::View* bubble_content = new InstalledBubbleContent(extension_, type_,
      &icon_);
  InfoBubble::Show(browser_view->GetWindow(), bounds, bubble_content, this);
}

// InfoBubbleDelegate
void ExtensionInstalledBubble::InfoBubbleClosing(InfoBubble* info_bubble,
                                                 bool closed_by_escape) {
  if (extension_->page_action()) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
        browser_->window()->GetNativeHandle());
    browser_view->GetLocationBarView()->SetPreviewEnabledPageAction(
        extension_->page_action(),
        false);  // preview_enabled
  }
  Release();  // Balanced in ctor.
}
