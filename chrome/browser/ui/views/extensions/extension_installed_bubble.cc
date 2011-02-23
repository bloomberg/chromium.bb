// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_installed_bubble.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/browser_actions_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/layout/layout_constants.h"
#include "views/view.h"

namespace {

const int kIconSize = 43;

const int kRightColumnWidth = 285;

// The InfoBubble uses a BubbleBorder which adds about 6 pixels of whitespace
// around the content view. We compensate by reducing our outer borders by this
// amount + 4px.
const int kOuterMarginInset = 10;
const int kHorizOuterMargin = views::kPanelHorizMargin - kOuterMarginInset;
const int kVertOuterMargin = views::kPanelVertMargin - kOuterMarginInset;

// Interior vertical margin is 8px smaller than standard
const int kVertInnerMargin = views::kPanelVertMargin - 8;

// The image we use for the close button has three pixels of whitespace padding.
const int kCloseButtonPadding = 3;

// We want to shift the right column (which contains the header and text) up
// 4px to align with icon.
const int kRightcolumnVerticalShift = -4;

// How long to wait for browser action animations to complete before retrying.
const int kAnimationWaitTime = 50;

// How often we retry when waiting for browser action animation to end.
const int kAnimationWaitMaxRetry = 10;

}  // namespace

namespace browser {

void ShowExtensionInstalledBubble(
    const Extension* extension,
    Browser* browser,
    SkBitmap icon,
    Profile* profile) {
  ExtensionInstalledBubble::Show(extension, browser, icon);
}

} // namespace browser

// InstalledBubbleContent is the content view which is placed in the
// ExtensionInstalledBubble. It displays the install icon and explanatory
// text about the installed extension.
class InstalledBubbleContent : public views::View,
                               public views::ButtonListener {
 public:
  InstalledBubbleContent(const Extension* extension,
                         ExtensionInstalledBubble::BubbleType type,
                         SkBitmap* icon)
      : info_bubble_(NULL),
        type_(type),
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

    string16 extension_name = UTF8ToUTF16(extension->name());
    base::i18n::AdjustStringForLocaleDirection(&extension_name);
    heading_ = new views::Label(UTF16ToWide(
        l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALLED_HEADING,
                                   extension_name)));
    heading_->SetFont(rb.GetFont(ResourceBundle::MediumFont));
    heading_->SetMultiLine(true);
    heading_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(heading_);

    if (type_ == ExtensionInstalledBubble::PAGE_ACTION) {
      info_ = new views::Label(UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO)));
      info_->SetFont(font);
      info_->SetMultiLine(true);
      info_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      AddChildView(info_);
    }

    if (type_ == ExtensionInstalledBubble::OMNIBOX_KEYWORD) {
      info_ = new views::Label(UTF16ToWide(l10n_util::GetStringFUTF16(
          IDS_EXTENSION_INSTALLED_OMNIBOX_KEYWORD_INFO,
          UTF8ToUTF16(extension->omnibox_keyword()))));
      info_->SetFont(font);
      info_->SetMultiLine(true);
      info_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      AddChildView(info_);
    }

    manage_ = new views::Label(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_INFO)));
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

  void set_info_bubble(InfoBubble* info_bubble) { info_bubble_ = info_bubble; }

  virtual void ButtonPressed(
      views::Button* sender,
      const views::Event& event) {
    if (sender == close_button_) {
      info_bubble_->set_fade_away_on_close(true);
      GetWidget()->Close();
    } else {
      NOTREACHED() << "Unknown view";
    }
  }

 private:
  virtual gfx::Size GetPreferredSize() {
    int width = kHorizOuterMargin;
    width += kIconSize;
    width += views::kPanelHorizMargin;
    width += kRightColumnWidth;
    width += 2 * views::kPanelHorizMargin;
    width += kHorizOuterMargin;

    int height = kVertOuterMargin;
    height += heading_->GetHeightForWidth(kRightColumnWidth);
    height += kVertInnerMargin;
    if (type_ == ExtensionInstalledBubble::PAGE_ACTION ||
        type_ == ExtensionInstalledBubble::OMNIBOX_KEYWORD) {
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
    x += views::kPanelHorizMargin;

    y += kRightcolumnVerticalShift;
    heading_->SizeToFit(kRightColumnWidth);
    heading_->SetX(x);
    heading_->SetY(y);
    y += heading_->height();
    y += kVertInnerMargin;

    if (type_ == ExtensionInstalledBubble::PAGE_ACTION ||
        type_ == ExtensionInstalledBubble::OMNIBOX_KEYWORD) {
      info_->SizeToFit(kRightColumnWidth);
      info_->SetX(x);
      info_->SetY(y);
      y += info_->height();
      y += kVertInnerMargin;
    }

    manage_->SizeToFit(kRightColumnWidth);
    manage_->SetX(x);
    manage_->SetY(y);
    y += manage_->height();
    y += kVertInnerMargin;

    gfx::Size sz;
    x += kRightColumnWidth + 2 * views::kPanelHorizMargin + kHorizOuterMargin -
        close_button_->GetPreferredSize().width();
    y = kVertOuterMargin;
    sz = close_button_->GetPreferredSize();
    // x-1 & y-1 is just slop to get the close button visually aligned with the
    // title text and bubble arrow.
    close_button_->SetBounds(x - 1, y - 1, sz.width(), sz.height());
  }

  // The InfoBubble showing us.
  InfoBubble* info_bubble_;

  ExtensionInstalledBubble::BubbleType type_;
  views::ImageView* icon_;
  views::Label* heading_;
  views::Label* info_;
  views::Label* manage_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(InstalledBubbleContent);
};

void ExtensionInstalledBubble::Show(const Extension* extension,
                                    Browser *browser,
                                    SkBitmap icon) {
  new ExtensionInstalledBubble(extension, browser, icon);
}

ExtensionInstalledBubble::ExtensionInstalledBubble(const Extension* extension,
                                                   Browser *browser,
                                                   SkBitmap icon)
    : extension_(extension),
      browser_(browser),
      icon_(icon),
      animation_wait_retries_(0) {
  AddRef();  // Balanced in InfoBubbleClosing.

  if (!extension_->omnibox_keyword().empty()) {
    type_ = OMNIBOX_KEYWORD;
  } else if (extension_->browser_action()) {
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
      Source<Profile>(browser->profile()));
  registrar_.Add(this, NotificationType::EXTENSION_UNLOADED,
      Source<Profile>(browser->profile()));
}

void ExtensionInstalledBubble::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_LOADED) {
    const Extension* extension = Details<const Extension>(details).ptr();
    if (extension == extension_) {
      animation_wait_retries_ = 0;
      // PostTask to ourself to allow all EXTENSION_LOADED Observers to run.
      MessageLoopForUI::current()->PostTask(FROM_HERE, NewRunnableMethod(this,
          &ExtensionInstalledBubble::ShowInternal));
    }
  } else if (type == NotificationType::EXTENSION_UNLOADED) {
    const Extension* extension =
        Details<UnloadedExtensionInfo>(details)->extension;
    if (extension == extension_)
      extension_ = NULL;
  } else {
    NOTREACHED() << L"Received unexpected notification";
  }
}

void ExtensionInstalledBubble::ShowInternal() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
      browser_->window()->GetNativeHandle());

  const views::View* reference_view = NULL;
  if (type_ == BROWSER_ACTION) {
    BrowserActionsContainer* container =
        browser_view->GetToolbarView()->browser_actions();
    if (container->animating() &&
        animation_wait_retries_++ < kAnimationWaitMaxRetry) {
      // We don't know where the view will be until the container has stopped
      // animating, so check back in a little while.
      MessageLoopForUI::current()->PostDelayedTask(
          FROM_HERE, NewRunnableMethod(this,
          &ExtensionInstalledBubble::ShowInternal), kAnimationWaitTime);
      return;
    }
    reference_view = container->GetBrowserActionView(
        extension_->browser_action());
    // If the view is not visible then it is in the chevron, so point the
    // install bubble to the chevron instead. If this is an incognito window,
    // both could be invisible.
    if (!reference_view || !reference_view->IsVisible()) {
      reference_view = container->chevron();
      if (!reference_view || !reference_view->IsVisible())
        reference_view = NULL;  // fall back to app menu below.
    }
  } else if (type_ == PAGE_ACTION) {
    LocationBarView* location_bar_view = browser_view->GetLocationBarView();
    location_bar_view->SetPreviewEnabledPageAction(extension_->page_action(),
                                                   true);  // preview_enabled
    reference_view = location_bar_view->GetPageActionView(
        extension_->page_action());
    DCHECK(reference_view);
  } else if (type_ == OMNIBOX_KEYWORD) {
    LocationBarView* location_bar_view = browser_view->GetLocationBarView();
    reference_view = location_bar_view;
    DCHECK(reference_view);
  }

  // Default case.
  if (reference_view == NULL)
    reference_view = browser_view->GetToolbarView()->app_menu();

  gfx::Point origin;
  views::View::ConvertPointToScreen(reference_view, &origin);
  gfx::Rect bounds = reference_view->bounds();
  bounds.set_origin(origin);
  BubbleBorder::ArrowLocation arrow_location = BubbleBorder::TOP_RIGHT;

  // For omnibox keyword bubbles, move the arrow to point to the left edge
  // of the omnibox, just to the right of the icon.
  if (type_ == OMNIBOX_KEYWORD) {
    bounds.set_origin(
        browser_view->GetLocationBarView()->GetLocationEntryOrigin());
    bounds.set_width(0);
    arrow_location = BubbleBorder::TOP_LEFT;
  }

  bubble_content_ = new InstalledBubbleContent(extension_, type_, &icon_);
  InfoBubble* info_bubble =
      InfoBubble::Show(browser_view->GetWidget(), bounds, arrow_location,
                       bubble_content_, this);
  bubble_content_->set_info_bubble(info_bubble);
}

// InfoBubbleDelegate
void ExtensionInstalledBubble::InfoBubbleClosing(InfoBubble* info_bubble,
                                                 bool closed_by_escape) {
  if (extension_ && type_ == PAGE_ACTION) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
        browser_->window()->GetNativeHandle());
    browser_view->GetLocationBarView()->SetPreviewEnabledPageAction(
        extension_->page_action(),
        false);  // preview_enabled
  }

  Release();  // Balanced in ctor.
}
