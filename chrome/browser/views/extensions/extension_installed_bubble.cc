// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/extensions/extension_installed_bubble.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/views/browser_actions_container.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/location_bar/location_bar_view.h"
#include "chrome/browser/views/tabs/base_tab.h"
#include "chrome/browser/views/tabs/base_tab_strip.h"
#include "chrome/browser/views/toolbar_view.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
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

// How long to wait for browser action animations to complete before retrying.
const int kAnimationWaitTime = 50;

// How often we retry when waiting for browser action animation to end.
const int kAnimationWaitMaxRetry = 10;

}  // namespace

// InstalledBubbleContent is the content view which is placed in the
// ExtensionInstalledBubble. It displays the install icon and explanatory
// text about the installed extension.
class InstalledBubbleContent : public views::View,
                               public views::ButtonListener {
 public:
  InstalledBubbleContent(Extension* extension,
                         ExtensionInstalledBubble::BubbleType type,
                         SkBitmap* icon)
      : info_bubble_(NULL),
        type_(type),
        info_(NULL),
        create_shortcut_(false) {
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

    std::wstring text;
    if (type_ == ExtensionInstalledBubble::EXTENSION_APP)
      text = l10n_util::GetString(IDS_EXTENSION_APP_INSTALLED_MANAGE_INFO);
    else
      text = l10n_util::GetString(IDS_EXTENSION_INSTALLED_MANAGE_INFO);
    manage_ = new views::Label(text);
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

    if (type_ == ExtensionInstalledBubble::EXTENSION_APP) {
      create_shortcut_view_ = new views::Checkbox(
          l10n_util::GetString(IDS_EXTENSION_PROMPT_CREATE_SHORTCUT));
      create_shortcut_view_->set_listener(this);
      create_shortcut_view_->SetMultiLine(true);
      AddChildView(create_shortcut_view_);
    }
  }

  // Whether to create a shortcut once the bubble closes.
  bool create_shortcut() { return create_shortcut_;}

  void set_info_bubble(InfoBubble* info_bubble) { info_bubble_ = info_bubble; }

  virtual void ButtonPressed(
      views::Button* sender,
      const views::Event& event) {
    if (sender == close_button_) {
      info_bubble_->set_fade_away_on_close(true);
      GetWidget()->Close();
    } else if (sender == create_shortcut_view_) {
      create_shortcut_ = create_shortcut_view_->checked();
    } else {
      NOTREACHED() << "Unknown view";
    }
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
    if (type_ == ExtensionInstalledBubble::EXTENSION_APP) {
      height += create_shortcut_view_->GetHeightForWidth(kRightColumnWidth);
      height += kVertInnerMargin;
    }
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
    y += manage_->height();
    y += kVertInnerMargin;

    gfx::Size sz;
    if (type_ == ExtensionInstalledBubble::EXTENSION_APP) {
      sz.set_height(
          create_shortcut_view_->GetHeightForWidth(kRightColumnWidth));
      sz.set_width(kRightColumnWidth);
      create_shortcut_view_->SetBounds(x, y, sz.width(), sz.height());
      y += create_shortcut_view_->height();
      y += kVertInnerMargin;
    }

    x += kRightColumnWidth + 2*kPanelHorizMargin + kHorizOuterMargin -
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
  views::Checkbox* create_shortcut_view_;
  views::ImageButton* close_button_;

  bool create_shortcut_;

  DISALLOW_COPY_AND_ASSIGN(InstalledBubbleContent);
};

void ExtensionInstalledBubble::Show(Extension *extension, Browser *browser,
                                    SkBitmap icon) {
  new ExtensionInstalledBubble(extension, browser, icon);
}

ExtensionInstalledBubble::ExtensionInstalledBubble(Extension *extension,
                                                   Browser *browser,
                                                   SkBitmap icon)
    : extension_(extension),
      browser_(browser),
      icon_(icon),
      animation_wait_retries_(0) {
  AddRef();  // Balanced in InfoBubbleClosing.

  if (extension->GetFullLaunchURL().is_valid()) {
    type_ = EXTENSION_APP;
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
      NotificationService::AllSources());
}

void ExtensionInstalledBubble::Observe(NotificationType type,
                                       const NotificationSource& source,
                                       const NotificationDetails& details) {
  if (type == NotificationType::EXTENSION_LOADED) {
    Extension* extension = Details<Extension>(details).ptr();
    if (extension == extension_) {
      animation_wait_retries_ = 0;
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
  } else if (type_ == EXTENSION_APP) {
    BaseTabStrip* tabstrip = browser_view->tabstrip();
    BaseTab* tab = tabstrip->GetSelectedBaseTab();
    DCHECK(tab->data().app);
    reference_view = tab;
  }

  // Default case.
  if (reference_view == NULL)
    reference_view = browser_view->GetToolbarView()->app_menu();

  gfx::Point origin;
  views::View::ConvertPointToScreen(reference_view, &origin);
  gfx::Rect bounds = reference_view->bounds();
  bounds.set_x(origin.x());
  bounds.set_y(origin.y());

  bubble_content_ = new InstalledBubbleContent(extension_, type_,
      &icon_);
  InfoBubble* info_bubble =
      InfoBubble::Show(browser_view->GetWidget(), bounds,
                       BubbleBorder::TOP_RIGHT,
                       bubble_content_, this);
  bubble_content_->set_info_bubble(info_bubble);
}

// InfoBubbleDelegate
void ExtensionInstalledBubble::InfoBubbleClosing(InfoBubble* info_bubble,
                                                 bool closed_by_escape) {
  if (type_ == PAGE_ACTION) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForNativeWindow(
        browser_->window()->GetNativeHandle());
    browser_view->GetLocationBarView()->SetPreviewEnabledPageAction(
        extension_->page_action(),
        false);  // preview_enabled
  } else if (type_ == EXTENSION_APP) {
    if (bubble_content_->create_shortcut()) {
      ShellIntegration::ShortcutInfo shortcut_info;
      shortcut_info.url = extension_->GetFullLaunchURL();
      shortcut_info.extension_id = UTF8ToUTF16(extension_->id());
      shortcut_info.title = UTF8ToUTF16(extension_->name());
      shortcut_info.description = UTF8ToUTF16(extension_->description());
      shortcut_info.favicon = icon_;
      shortcut_info.create_on_desktop = true;
      shortcut_info.create_in_applications_menu = false;
      shortcut_info.create_in_quick_launch_bar = false;
      web_app::CreateShortcut(browser_->profile()->GetPath(), shortcut_info,
                              NULL);
    }
  }

  Release();  // Balanced in ctor.
}
