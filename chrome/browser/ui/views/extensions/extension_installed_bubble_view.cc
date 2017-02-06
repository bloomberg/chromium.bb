// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/extensions/extension_installed_bubble.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/sync/bubble_sync_promo_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/location_bar/page_action_with_badge_view.h"
#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bubble/bubble_controller.h"
#include "components/bubble/bubble_ui.h"
#include "content/public/browser/user_metrics.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_constants.h"

using extensions::Extension;

namespace {

const int kIconSize = 43;

const int kRightColumnWidth = 285;

views::Label* CreateLabel(const base::string16& text) {
  views::Label* label = new views::Label(text);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SizeToFit(kRightColumnWidth);
  return label;
}

// Provides feedback to the user upon successful installation of an
// extension. Depending on the type of extension, the Bubble will
// point to:
//    OMNIBOX_KEYWORD-> The omnibox.
//    BROWSER_ACTION -> The browserAction icon in the toolbar.
//    PAGE_ACTION    -> A preview of the pageAction icon in the location
//                      bar which is shown while the Bubble is shown.
//    GENERIC        -> The app menu. This case includes pageActions that don't
//                      specify a default icon.
class ExtensionInstalledBubbleView : public BubbleSyncPromoDelegate,
                                     public views::BubbleDialogDelegateView,
                                     public views::LinkListener {
 public:
  explicit ExtensionInstalledBubbleView(ExtensionInstalledBubble* bubble);
  ~ExtensionInstalledBubbleView() override;

  // Recalculate the anchor position for this bubble.
  void UpdateAnchorView();

  void CloseBubble();

 private:
  Browser* browser() { return controller_->browser(); }

  // views::BubbleDialogDelegateView:
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  bool ShouldShowWindowIcon() const override;
  bool ShouldShowCloseButton() const override;
  View* CreateFootnoteView() override;
  int GetDialogButtons() const override;
  void Init() override;

  // BubbleSyncPromoDelegate:
  void OnSignInLinkClicked() override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // Gets the size of the icon, capped at kIconSize.
  gfx::Size GetIconSize() const;

  ExtensionInstalledBubble* controller_;

  // The shortcut to open the manage shortcuts page.
  views::Link* manage_shortcut_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleView);
};

ExtensionInstalledBubbleView::ExtensionInstalledBubbleView(
    ExtensionInstalledBubble* controller)
    : BubbleDialogDelegateView(nullptr,
                               controller->anchor_position() ==
                                       ExtensionInstalledBubble::ANCHOR_OMNIBOX
                                   ? views::BubbleBorder::TOP_LEFT
                                   : views::BubbleBorder::TOP_RIGHT),
      controller_(controller),
      manage_shortcut_(nullptr) {}

ExtensionInstalledBubbleView::~ExtensionInstalledBubbleView() {}

void ExtensionInstalledBubbleView::UpdateAnchorView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  views::View* reference_view = nullptr;
  switch (controller_->anchor_position()) {
    case ExtensionInstalledBubble::ANCHOR_ACTION: {
      BrowserActionsContainer* container =
          browser_view->toolbar()->browser_actions();
      // Hitting this DCHECK means |ShouldShow| failed.
      DCHECK(!container->animating());

      reference_view = container->GetViewForId(controller_->extension()->id());
      break;
    }
    case ExtensionInstalledBubble::ANCHOR_OMNIBOX: {
      reference_view = browser_view->GetLocationBarView()->location_icon_view();
      break;
    }
    case ExtensionInstalledBubble::ANCHOR_APP_MENU:
      // Will be caught below.
      break;
  }

  // Default case.
  if (!reference_view || !reference_view->visible())
    reference_view = browser_view->toolbar()->app_menu_button();
  SetAnchorView(reference_view);
}

void ExtensionInstalledBubbleView::CloseBubble() {
  if (GetWidget()->IsClosed())
    return;
  GetWidget()->Close();
}

base::string16 ExtensionInstalledBubbleView::GetWindowTitle() const {
  // Add the heading (for all options).
  base::string16 extension_name =
      base::UTF8ToUTF16(controller_->extension()->name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  return l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALLED_HEADING,
                                    extension_name);
}

gfx::ImageSkia ExtensionInstalledBubbleView::GetWindowIcon() {
  const SkBitmap& bitmap = controller_->icon();
  return gfx::ImageSkiaOperations::CreateResizedImage(
      gfx::ImageSkia::CreateFrom1xBitmap(bitmap),
      skia::ImageOperations::RESIZE_BEST, GetIconSize());
}

bool ExtensionInstalledBubbleView::ShouldShowWindowIcon() const {
  return true;
}

views::View* ExtensionInstalledBubbleView::CreateFootnoteView() {
  if (!(controller_->options() & ExtensionInstalledBubble::SIGN_IN_PROMO))
    return nullptr;

  return new BubbleSyncPromoView(this,
                                 IDS_EXTENSION_INSTALLED_SYNC_PROMO_LINK_NEW,
                                 IDS_EXTENSION_INSTALLED_SYNC_PROMO_NEW);
}

int ExtensionInstalledBubbleView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

bool ExtensionInstalledBubbleView::ShouldShowCloseButton() const {
  return true;
}

void ExtensionInstalledBubbleView::Init() {
  UpdateAnchorView();

  // The Extension Installed bubble takes on various forms, depending on the
  // type of extension installed. In general, though, they are all similar:
  //
  // -------------------------
  // | Icon | Title      (x) |
  // |        Info           |
  // |        Extra info     |
  // -------------------------
  //
  // Icon and Title are always shown (as well as the close button).
  // Info is shown for browser actions, page actions and Omnibox keyword
  // extensions and might list keyboard shorcut for the former two types.
  // Extra info is...
  // ... for other types, either a description of how to manage the extension
  //     or a link to configure the keybinding shortcut (if one exists).
  // Extra info can include a promo for signing into sync.

  std::unique_ptr<views::BoxLayout> layout(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                           views::kRelatedControlVerticalSpacing));
  layout->set_minimum_cross_axis_size(kRightColumnWidth);
  // Indent by the size of the icon.
  layout->set_inside_border_insets(gfx::Insets(
      0, GetIconSize().width() + views::kUnrelatedControlHorizontalSpacing, 0,
      0));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  SetLayoutManager(layout.release());

  if (controller_->options() & ExtensionInstalledBubble::HOW_TO_USE)
    AddChildView(CreateLabel(controller_->GetHowToUseDescription()));

  if (controller_->options() & ExtensionInstalledBubble::SHOW_KEYBINDING) {
    manage_shortcut_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS));
    manage_shortcut_->set_listener(this);
    manage_shortcut_->SetUnderline(false);
    AddChildView(manage_shortcut_);
  }

  if (controller_->options() & ExtensionInstalledBubble::HOW_TO_MANAGE) {
    AddChildView(CreateLabel(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_INFO)));
  }
}

void ExtensionInstalledBubbleView::OnSignInLinkClicked() {
  chrome::ShowBrowserSignin(
      browser(),
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE);
  CloseBubble();
}

void ExtensionInstalledBubbleView::LinkClicked(views::Link* source,
                                               int event_flags) {
  DCHECK_EQ(manage_shortcut_, source);

  std::string configure_url = chrome::kChromeUIExtensionsURL;
  configure_url += chrome::kExtensionConfigureCommandsSubPage;
  chrome::NavigateParams params(
      chrome::GetSingletonTabNavigateParams(browser(), GURL(configure_url)));
  chrome::Navigate(&params);
  CloseBubble();
}

gfx::Size ExtensionInstalledBubbleView::GetIconSize() const {
  const SkBitmap& bitmap = controller_->icon();
  // Scale down to 43x43, but allow smaller icons (don't scale up).
  gfx::Size size(bitmap.width(), bitmap.height());
  return size.width() > kIconSize || size.height() > kIconSize
             ? gfx::Size(kIconSize, kIconSize)
             : size;
}

// NB: This bubble is using the temporarily-deprecated bubble manager interface
// BubbleUi. Do not copy this pattern.
class ExtensionInstalledBubbleUi : public BubbleUi,
                                   public views::WidgetObserver {
 public:
  explicit ExtensionInstalledBubbleUi(ExtensionInstalledBubble* bubble);
  ~ExtensionInstalledBubbleUi() override;

  // BubbleUi:
  void Show(BubbleReference bubble_reference) override;
  void Close() override;
  void UpdateAnchorPosition() override;

  // WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;

 private:
  ExtensionInstalledBubble* bubble_;
  ExtensionInstalledBubbleView* bubble_view_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleUi);
};

ExtensionInstalledBubbleUi::ExtensionInstalledBubbleUi(
    ExtensionInstalledBubble* bubble)
    : bubble_(bubble), bubble_view_(nullptr) {
  DCHECK(bubble_);
}

ExtensionInstalledBubbleUi::~ExtensionInstalledBubbleUi() {
  if (bubble_view_)
    bubble_view_->GetWidget()->RemoveObserver(this);
}

void ExtensionInstalledBubbleUi::Show(BubbleReference /*bubble_reference*/) {
  bubble_view_ = new ExtensionInstalledBubbleView(bubble_);

  views::BubbleDialogDelegateView::CreateBubble(bubble_view_)->Show();
  bubble_view_->GetWidget()->AddObserver(this);
  content::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromExtensionInstallBubble"));
}

void ExtensionInstalledBubbleUi::Close() {
  if (bubble_view_)
    bubble_view_->CloseBubble();
}

void ExtensionInstalledBubbleUi::UpdateAnchorPosition() {
  DCHECK(bubble_view_);
  bubble_view_->UpdateAnchorView();
}

void ExtensionInstalledBubbleUi::OnWidgetClosing(views::Widget* widget) {
  widget->RemoveObserver(this);
  bubble_view_ = nullptr;
}

}  // namespace

// Views specific implementation.
bool ExtensionInstalledBubble::ShouldShow() {
  if (anchor_position() == ANCHOR_ACTION) {
    BrowserActionsContainer* container =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->toolbar()
            ->browser_actions();
    return !container->animating();
  }
  return true;
}

// Implemented here to create the platform specific instance of the BubbleUi.
std::unique_ptr<BubbleUi> ExtensionInstalledBubble::BuildBubbleUi() {
  return base::WrapUnique(new ExtensionInstalledBubbleUi(this));
}
