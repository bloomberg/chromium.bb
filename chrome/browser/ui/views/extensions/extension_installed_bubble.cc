// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_installed_bubble.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/commands/command_service_factory.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/browser_action_view.h"
#include "chrome/browser/ui/views/browser_actions_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar_view.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_constants.h"

using extensions::Extension;

namespace {

const int kIconSize = 43;

const int kRightColumnWidth = 285;

// The Bubble uses a BubbleBorder which adds about 6 pixels of whitespace
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

namespace chrome {

void ShowExtensionInstalledBubble(const Extension* extension,
                                  Browser* browser,
                                  const SkBitmap& icon) {
  ExtensionInstalledBubble::Show(extension, browser, icon);
}

}  // namespace chrome

// InstalledBubbleContent is the content view which is placed in the
// ExtensionInstalledBubble. It displays the install icon and explanatory
// text about the installed extension.
class InstalledBubbleContent : public views::View,
                               public views::ButtonListener,
                               public views::LinkListener {
 public:
  InstalledBubbleContent(Browser* browser,
                         const Extension* extension,
                         ExtensionInstalledBubble::BubbleType type,
                         SkBitmap* icon,
                         ExtensionInstalledBubble* bubble)
      : browser_(browser),
        extension_id_(extension->id()),
        bubble_(bubble),
        type_(type),
        info_(NULL),
        manage_(NULL),
       manage_shortcut_(NULL) {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    const gfx::Font& font = rb.GetFont(ui::ResourceBundle::BaseFont);

    // Scale down to 43x43, but allow smaller icons (don't scale up).
    gfx::Size size(icon->width(), icon->height());
    if (size.width() > kIconSize || size.height() > kIconSize)
      size = gfx::Size(kIconSize, kIconSize);
    icon_ = new views::ImageView();
    icon_->SetImageSize(size);
    icon_->SetImage(*icon);
    AddChildView(icon_);

    // The Extension Installed bubble takes on various forms, depending on the
    // type of extension installed. In general, though, they are all similar:
    //
    // -------------------------
    // |      | Heading    [X] |
    // | Icon | Info           |
    // |      | Extra info     |
    // -------------------------
    //
    // Icon and Heading are always shown.
    // Info is shown for browser actions, page actions and Omnibox keyword
    // extensions and might list keyboard shorcut for the former two types.
    // Extra info is...
    // ... for Apps: a link that opens the App Install UI.
    // ... for other types, either a description of how to manage the extension
    //     or a link to configure the keybinding shortcut (if one exists).

    string16 extension_name = UTF8ToUTF16(extension->name());
    base::i18n::AdjustStringForLocaleDirection(&extension_name);
    heading_ = new views::Label(l10n_util::GetStringFUTF16(
        IDS_EXTENSION_INSTALLED_HEADING, extension_name));
    heading_->SetFont(rb.GetFont(ui::ResourceBundle::MediumFont));
    heading_->SetMultiLine(true);
    heading_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    AddChildView(heading_);

    bool has_keybinding = false;

    switch (type_) {
      case ExtensionInstalledBubble::BROWSER_ACTION: {
        extensions::CommandService* command_service =
            extensions::CommandServiceFactory::GetForProfile(
                browser_->profile());
        extensions::Command browser_action_command;
        if (!command_service->GetBrowserActionCommand(
                extension->id(),
                extensions::CommandService::ACTIVE_ONLY,
                &browser_action_command,
                NULL)) {
          info_ = new views::Label(l10n_util::GetStringUTF16(
              IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO));
        } else {
          has_keybinding = true;
          info_ = new views::Label(l10n_util::GetStringFUTF16(
              IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO_WITH_SHORTCUT,
              browser_action_command.accelerator().GetShortcutText()));
        }

        info_->SetFont(font);
        info_->SetMultiLine(true);
        info_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
        AddChildView(info_);

        manage_shortcut_ = new views::Link(
            l10n_util::GetStringUTF16(
            IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS));
        break;
      }
      case ExtensionInstalledBubble::PAGE_ACTION: {
        extensions::CommandService* command_service =
            extensions::CommandServiceFactory::GetForProfile(
                browser_->profile());
        extensions::Command page_action_command;
        if (!command_service->GetPageActionCommand(
                extension->id(),
                extensions::CommandService::ACTIVE_ONLY,
                &page_action_command,
                NULL)) {
          info_ = new views::Label(l10n_util::GetStringUTF16(
              IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO));
        } else {
          has_keybinding = true;
          info_ = new views::Label(l10n_util::GetStringFUTF16(
              IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO_WITH_SHORTCUT,
              page_action_command.accelerator().GetShortcutText()));
          manage_shortcut_ = new views::Link(
              l10n_util::GetStringUTF16(
                  IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS));
        }

        info_->SetFont(font);
        info_->SetMultiLine(true);
        info_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
        AddChildView(info_);
        break;
      }
      case ExtensionInstalledBubble::OMNIBOX_KEYWORD: {
        info_ = new views::Label(l10n_util::GetStringFUTF16(
            IDS_EXTENSION_INSTALLED_OMNIBOX_KEYWORD_INFO,
            UTF8ToUTF16(extension->omnibox_keyword())));
        info_->SetFont(font);
        info_->SetMultiLine(true);
        info_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
        AddChildView(info_);
        break;
      }
      case ExtensionInstalledBubble::APP: {
        views::Link* link = new views::Link(
            l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_APP_INFO));
        link->set_listener(this);
        manage_ = link;
        manage_->SetFont(font);
        manage_->SetMultiLine(true);
        manage_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
        AddChildView(manage_);
        break;
      }
      default:
        break;
    }

    if (has_keybinding) {
      manage_shortcut_->set_listener(this);
      AddChildView(manage_shortcut_);
    } else if (type_ != ExtensionInstalledBubble::APP) {
      manage_ = new views::Label(
          l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_INFO));
      manage_->SetFont(font);
      manage_->SetMultiLine(true);
      manage_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      AddChildView(manage_);
    }

    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(views::CustomButton::BS_NORMAL,
        rb.GetImageSkiaNamed(IDR_CLOSE_BAR));
    close_button_->SetImage(views::CustomButton::BS_HOT,
        rb.GetImageSkiaNamed(IDR_CLOSE_BAR_H));
    close_button_->SetImage(views::CustomButton::BS_PUSHED,
        rb.GetImageSkiaNamed(IDR_CLOSE_BAR_P));
    AddChildView(close_button_);
  }

  virtual void ButtonPressed(views::Button* sender, const ui::Event& event) {
    if (sender == close_button_)
      bubble_->StartFade(false);
    else
      NOTREACHED() << "Unknown view";
  }

  // Implements the views::LinkListener interface.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE {
    GetWidget()->Close();
    if (source == manage_shortcut_) {
      std::string configure_url = chrome::kChromeUIExtensionsURL;
      configure_url += chrome::kExtensionConfigureCommandsSubPage;
      chrome::NavigateParams params(
          chrome::GetSingletonTabNavigateParams(
              browser_, GURL(configure_url.c_str())));
      chrome::Navigate(&params);
    } else {
      ExtensionInstallUI::OpenAppInstalledUI(browser_, extension_id_);
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
    if (info_) {
      height += info_->GetHeightForWidth(kRightColumnWidth);
      height += kVertInnerMargin;
    }

    if (manage_) {
      height += manage_->GetHeightForWidth(kRightColumnWidth);
      height += kVertOuterMargin;
    }

    if (manage_shortcut_) {
      height += manage_shortcut_->GetHeightForWidth(kRightColumnWidth);
      height += kVertOuterMargin;
    }

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

    if (info_) {
      info_->SizeToFit(kRightColumnWidth);
      info_->SetX(x);
      info_->SetY(y);
      y += info_->height();
      y += kVertInnerMargin;
    }

    if (manage_) {
      manage_->SizeToFit(kRightColumnWidth);
      manage_->SetX(x);
      manage_->SetY(y);
      y += manage_->height();
      y += kVertInnerMargin;
    }

    if (manage_shortcut_) {
      gfx::Size sz = manage_shortcut_->GetPreferredSize();
      manage_shortcut_->SetBounds(width() - 2 * kHorizOuterMargin - sz.width(),
                                  y,
                                  sz.width(),
                                  sz.height());
      y += manage_shortcut_->height();
      y += kVertInnerMargin;
    }

    gfx::Size sz;
    x += kRightColumnWidth + 2 * views::kPanelHorizMargin + kHorizOuterMargin -
        close_button_->GetPreferredSize().width();
    y = kVertOuterMargin;
    sz = close_button_->GetPreferredSize();
    // x-1 & y-1 is just slop to get the close button visually aligned with the
    // title text and bubble arrow.
    close_button_->SetBounds(x - 1, y - 1, sz.width(), sz.height());
  }

  // The browser we're associated with.
  Browser* browser_;

  // The id of the extension just installed.
  const std::string extension_id_;

  // The ExtensionInstalledBubble showing us.
  ExtensionInstalledBubble* bubble_;

  ExtensionInstalledBubble::BubbleType type_;
  views::ImageView* icon_;
  views::Label* heading_;
  views::Label* info_;
  views::Label* manage_;
  views::Link* manage_shortcut_;
  views::ImageButton* close_button_;

  DISALLOW_COPY_AND_ASSIGN(InstalledBubbleContent);
};

void ExtensionInstalledBubble::Show(const Extension* extension,
                                    Browser *browser,
                                    const SkBitmap& icon) {
  new ExtensionInstalledBubble(extension, browser, icon);
}

ExtensionInstalledBubble::ExtensionInstalledBubble(const Extension* extension,
                                                   Browser *browser,
                                                   const SkBitmap& icon)
    : extension_(extension),
      browser_(browser),
      icon_(icon),
      animation_wait_retries_(0) {
  if (extension->is_app())
    type_ = APP;
  else if (!extension_->omnibox_keyword().empty())
    type_ = OMNIBOX_KEYWORD;
  else if (extension_->browser_action())
    type_ = BROWSER_ACTION;
  else if (extension->page_action() && extension->is_verbose_install_message())
    type_ = PAGE_ACTION;
  else
    type_ = GENERIC;

  // |extension| has been initialized but not loaded at this point. We need
  // to wait on showing the Bubble until not only the EXTENSION_LOADED gets
  // fired, but all of the EXTENSION_LOADED Observers have run. Only then can we
  // be sure that a BrowserAction or PageAction has had views created which we
  // can inspect for the purpose of previewing of pointing to them.
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(browser->profile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(browser->profile()));
}

ExtensionInstalledBubble::~ExtensionInstalledBubble() {}

void ExtensionInstalledBubble::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
    const Extension* extension =
        content::Details<const Extension>(details).ptr();
    if (extension == extension_) {
      animation_wait_retries_ = 0;
      // PostTask to ourself to allow all EXTENSION_LOADED Observers to run.
      MessageLoopForUI::current()->PostTask(
          FROM_HERE,
          base::Bind(&ExtensionInstalledBubble::ShowInternal,
                     base::Unretained(this)));
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

void ExtensionInstalledBubble::ShowInternal() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  views::View* reference_view = NULL;
  if (type_ == APP) {
    if (browser_view->IsTabStripVisible()) {
      TabStrip* tabstrip = browser_view->tabstrip();
      views::View* ntp_button = tabstrip->newtab_button();
      if (ntp_button && ntp_button->IsDrawn()) {
        reference_view = ntp_button;
      } else {
        // Just have the bubble point at the tab strip.
        reference_view = tabstrip;
      }
    }
  } else if (type_ == BROWSER_ACTION) {
    BrowserActionsContainer* container =
        browser_view->GetToolbarView()->browser_actions();
    if (container->animating() &&
        animation_wait_retries_++ < kAnimationWaitMaxRetry) {
      // We don't know where the view will be until the container has stopped
      // animating, so check back in a little while.
      MessageLoopForUI::current()->PostDelayedTask(
          FROM_HERE,
          base::Bind(&ExtensionInstalledBubble::ShowInternal,
                     base::Unretained(this)),
          base::TimeDelta::FromMilliseconds(kAnimationWaitTime));
      return;
    }
    reference_view = container->GetBrowserActionView(
        extension_->browser_action());
    // If the view is not visible then it is in the chevron, so point the
    // install bubble to the chevron instead. If this is an incognito window,
    // both could be invisible.
    if (!reference_view || !reference_view->visible()) {
      reference_view = container->chevron();
      if (!reference_view || !reference_view->visible())
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
  set_anchor_view(reference_view);

  set_arrow_location(type_ == OMNIBOX_KEYWORD ? views::BubbleBorder::TOP_LEFT :
                                                views::BubbleBorder::TOP_RIGHT);
  SetLayoutManager(new views::FillLayout());
  AddChildView(
      new InstalledBubbleContent(browser_, extension_, type_, &icon_, this));
  views::BubbleDelegateView::CreateBubble(this);
  StartFade(true);
}

gfx::Rect ExtensionInstalledBubble::GetAnchorRect() {
  // For omnibox keyword bubbles, move the arrow to point to the left edge
  // of the omnibox, just to the right of the icon.
  if (type_ == OMNIBOX_KEYWORD) {
    LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(browser_)->GetLocationBarView();
    return gfx::Rect(location_bar_view->GetLocationEntryOrigin(),
        gfx::Size(0, location_bar_view->location_entry_view()->height()));
  }
  return views::BubbleDelegateView::GetAnchorRect();
}

void ExtensionInstalledBubble::WindowClosing() {
  if (extension_ && type_ == PAGE_ACTION) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    browser_view->GetLocationBarView()->SetPreviewEnabledPageAction(
        extension_->page_action(),
        false);  // preview_enabled
  }
}
