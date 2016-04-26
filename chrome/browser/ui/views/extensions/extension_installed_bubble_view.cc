// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
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
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/render_text.h"
#include "ui/gfx/text_elider.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

using extensions::Extension;

namespace {

const int kIconSize = 43;

const int kRightColumnWidth = 285;

views::Label* CreateLabel(const base::string16& text,
                          const gfx::FontList& font) {
  views::Label* label = new views::Label(text, font);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

class HeadingAndCloseButtonView : public views::View {
 public:
  HeadingAndCloseButtonView(views::Label* heading, views::LabelButton* close)
      : heading_(heading), close_(close) {
    AddChildView(heading_);
    AddChildView(close_);
  }
  ~HeadingAndCloseButtonView() override {}

  void Layout() override {
    gfx::Size close_size = close_->GetPreferredSize();
    gfx::Size view_size = size();

    // Close button is in the upper right and always gets its full desired size.
    close_->SetBounds(view_size.width() - close_size.width(),
                      0,
                      close_size.width(),
                      close_size.height());
    // The heading takes up the remaining room (modulo padding).
    heading_->SetBounds(
        0,
        0,
        view_size.width() - close_size.width() -
            views::kUnrelatedControlHorizontalSpacing,
        view_size.height());
  }

  gfx::Size GetPreferredSize() const override {
    gfx::Size heading_size = heading_->GetPreferredSize();
    gfx::Size close_size = close_->GetPreferredSize();
    return gfx::Size(kRightColumnWidth,
                     std::max(heading_size.height(), close_size.height()));
  }

  int GetHeightForWidth(int width) const override {
    gfx::Size close_size = close_->GetPreferredSize();
    int heading_width = width - views::kUnrelatedControlHorizontalSpacing -
        close_size.width();
    return std::max(heading_->GetHeightForWidth(heading_width),
                    close_size.height());
  }

 private:
  views::Label* heading_;
  views::LabelButton* close_;

  DISALLOW_COPY_AND_ASSIGN(HeadingAndCloseButtonView);
};

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
                                     public views::ButtonListener,
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
  void Init() override;
  View* CreateFootnoteView() override;
  int GetDialogButtons() const override;

  // BubbleSyncPromoDelegate:
  void OnSignInLinkClicked() override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  ExtensionInstalledBubble* controller_;
  ExtensionInstalledBubble::BubbleType type_;
  ExtensionInstalledBubble::AnchorPosition anchor_position_;

  // The button to close the bubble.
  views::LabelButton* close_;

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
      close_(nullptr),
      manage_shortcut_(nullptr) {}

ExtensionInstalledBubbleView::~ExtensionInstalledBubbleView() {}

void ExtensionInstalledBubbleView::UpdateAnchorView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  views::View* reference_view = nullptr;
  switch (controller_->anchor_position()) {
    case ExtensionInstalledBubble::ANCHOR_BROWSER_ACTION: {
      BrowserActionsContainer* container =
          browser_view->GetToolbarView()->browser_actions();
      // Hitting this DCHECK means |ShouldShow| failed.
      DCHECK(!container->animating());

      reference_view = container->GetViewForId(controller_->extension()->id());
      // If the view is not visible then it is in the chevron, so point the
      // install bubble to the chevron instead. If this is an incognito window,
      // both could be invisible.
      if (!reference_view || !reference_view->visible()) {
        reference_view = container->chevron();
        if (!reference_view || !reference_view->visible())
          reference_view = nullptr;  // fall back to app menu below.
      }
      break;
    }
    case ExtensionInstalledBubble::ANCHOR_PAGE_ACTION: {
      LocationBarView* location_bar_view = browser_view->GetLocationBarView();
      ExtensionAction* page_action =
          extensions::ExtensionActionManager::Get(browser()->profile())
              ->GetPageAction(*controller_->extension());
      location_bar_view->SetPreviewEnabledPageAction(page_action,
                                                     true);  // preview_enabled
      reference_view = location_bar_view->GetPageActionView(page_action);
      DCHECK(reference_view);
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
  if (!reference_view)
    reference_view = browser_view->GetToolbarView()->app_menu_button();
  SetAnchorView(reference_view);
}

void ExtensionInstalledBubbleView::CloseBubble() {
  if (controller_ && controller_->anchor_position() ==
      ExtensionInstalledBubble::ANCHOR_PAGE_ACTION) {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    browser_view->GetLocationBarView()->SetPreviewEnabledPageAction(
        extensions::ExtensionActionManager::Get(browser()->profile())
            ->GetPageAction(*controller_->extension()),
        false);  // preview_enabled
  }
  controller_ = nullptr;
  GetWidget()->Close();
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

void ExtensionInstalledBubbleView::OnSignInLinkClicked() {
  chrome::ShowBrowserSignin(
      browser(),
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE);
  CloseBubble();
}

void ExtensionInstalledBubbleView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  DCHECK_EQ(sender, close_);
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

void ExtensionInstalledBubbleView::Init() {
  UpdateAnchorView();

  // The Extension Installed bubble takes on various forms, depending on the
  // type of extension installed. In general, though, they are all similar:
  //
  // -------------------------
  // |      | Heading    [X] |
  // | Icon | Info           |
  // |      | Extra info     |
  // -------------------------
  //
  // Icon and Heading are always shown (as well as the close button).
  // Info is shown for browser actions, page actions and Omnibox keyword
  // extensions and might list keyboard shorcut for the former two types.
  // Extra info is...
  // ... for other types, either a description of how to manage the extension
  //     or a link to configure the keybinding shortcut (if one exists).
  // Extra info can include a promo for signing into sync.

  // The number of rows in the content section of the bubble.
  int main_content_row_count = 1;
  if (controller_->options() & ExtensionInstalledBubble::HOW_TO_USE)
    ++main_content_row_count;
  if (controller_->options() & ExtensionInstalledBubble::SHOW_KEYBINDING)
    ++main_content_row_count;
  if (controller_->options() & ExtensionInstalledBubble::HOW_TO_MANAGE)
    ++main_content_row_count;

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int cs_id = 0;

  views::ColumnSet* main_cs = layout->AddColumnSet(cs_id);
  // Icon column.
  main_cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  main_cs->AddPaddingColumn(0, views::kUnrelatedControlHorizontalSpacing);
  // Heading column.
  main_cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::FIXED, kRightColumnWidth, 0);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& font_list = rb.GetFontList(ui::ResourceBundle::BaseFont);

  const SkBitmap& bitmap = controller_->icon();
  // Add the icon (for all options).
  // Scale down to 43x43, but allow smaller icons (don't scale up).
  gfx::Size size(bitmap.width(), bitmap.height());
  if (size.width() > kIconSize || size.height() > kIconSize)
    size = gfx::Size(kIconSize, kIconSize);
  views::ImageView* icon = new views::ImageView();
  icon->SetImageSize(size);
  icon->SetImage(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  layout->StartRow(0, cs_id);
  layout->AddView(icon, 1, main_content_row_count);

  // Add the heading (for all options).
  base::string16 extension_name =
      base::UTF8ToUTF16(controller_->extension()->name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  views::Label* heading =
      CreateLabel(l10n_util::GetStringFUTF16(IDS_EXTENSION_INSTALLED_HEADING,
                                             extension_name),
                  rb.GetFontList(ui::ResourceBundle::MediumFont));

  close_ = views::BubbleFrameView::CreateCloseButton(this);

  HeadingAndCloseButtonView* heading_and_close =
      new HeadingAndCloseButtonView(heading, close_);

  layout->AddView(heading_and_close);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  auto add_content_view = [&layout, &cs_id](views::View* view) {
    layout->StartRow(0, cs_id);
    // Skip the icon column.
    layout->SkipColumns(1);
    layout->AddView(view);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  };

  if (controller_->options() & ExtensionInstalledBubble::HOW_TO_USE) {
    add_content_view(
        CreateLabel(controller_->GetHowToUseDescription(), font_list));
  }

  if (controller_->options() & ExtensionInstalledBubble::SHOW_KEYBINDING) {
    manage_shortcut_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS));
    manage_shortcut_->set_listener(this);
    manage_shortcut_->SetUnderline(false);
    add_content_view(manage_shortcut_);
  }

  if (controller_->options() & ExtensionInstalledBubble::HOW_TO_MANAGE) {
    add_content_view(CreateLabel(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_INFO),
        font_list));
  }
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
  if (anchor_position() == ANCHOR_BROWSER_ACTION) {
    BrowserActionsContainer* container =
        BrowserView::GetBrowserViewForBrowser(browser())
            ->GetToolbarView()
            ->browser_actions();
    return !container->animating();
  }
  return true;
}

// Implemented here to create the platform specific instance of the BubbleUi.
std::unique_ptr<BubbleUi> ExtensionInstalledBubble::BuildBubbleUi() {
  return base::WrapUnique(new ExtensionInstalledBubbleUi(this));
}
