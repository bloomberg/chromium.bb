// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_installed_bubble_view.h"

#include <algorithm>
#include <string>

#include "base/macros.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
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
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
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

}  // namespace

ExtensionInstalledBubbleView::ExtensionInstalledBubbleView(
    ExtensionInstalledBubble* bubble,
    BubbleReference bubble_reference)
    : bubble_(bubble),
      bubble_reference_(bubble_reference),
      extension_(bubble->extension()),
      browser_(bubble->browser()),
      type_(bubble->type()),
      anchor_position_(bubble->anchor_position()),
      close_(nullptr),
      manage_shortcut_(nullptr) {}

ExtensionInstalledBubbleView::~ExtensionInstalledBubbleView() {}

void ExtensionInstalledBubbleView::UpdateAnchorView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);

  views::View* reference_view = nullptr;
  switch (anchor_position_) {
    case ExtensionInstalledBubble::ANCHOR_BROWSER_ACTION: {
      BrowserActionsContainer* container =
          browser_view->GetToolbarView()->browser_actions();
      // Hitting this DCHECK means |ShouldShow| failed.
      DCHECK(!container->animating());

      reference_view = container->GetViewForId(extension_->id());
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
          extensions::ExtensionActionManager::Get(browser_->profile())
              ->GetPageAction(*extension_);
      location_bar_view->SetPreviewEnabledPageAction(page_action,
                                                     true);  // preview_enabled
      reference_view = location_bar_view->GetPageActionView(page_action);
      DCHECK(reference_view);
      break;
    }
    case ExtensionInstalledBubble::ANCHOR_OMNIBOX: {
      LocationBarView* location_bar_view = browser_view->GetLocationBarView();
      reference_view = location_bar_view;
      DCHECK(reference_view);
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

scoped_ptr<views::View> ExtensionInstalledBubbleView::CreateFootnoteView() {
  if (!(bubble_->options() & ExtensionInstalledBubble::SIGN_IN_PROMO))
    return nullptr;

  return scoped_ptr<views::View>(
      new BubbleSyncPromoView(this, IDS_EXTENSION_INSTALLED_SYNC_PROMO_LINK_NEW,
                              IDS_EXTENSION_INSTALLED_SYNC_PROMO_NEW));
}

void ExtensionInstalledBubbleView::WindowClosing() {
  if (anchor_position_ == ExtensionInstalledBubble::ANCHOR_PAGE_ACTION) {
    BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
    browser_view->GetLocationBarView()->SetPreviewEnabledPageAction(
        extensions::ExtensionActionManager::Get(browser_->profile())
            ->GetPageAction(*extension_),
        false);  // preview_enabled
  }
}

gfx::Rect ExtensionInstalledBubbleView::GetAnchorRect() const {
  // For omnibox keyword bubbles, move the arrow to point to the left edge
  // of the omnibox, just to the right of the icon.
  if (type_ == ExtensionInstalledBubble::OMNIBOX_KEYWORD) {
    const LocationBarView* location_bar_view =
        BrowserView::GetBrowserViewForBrowser(browser_)->GetLocationBarView();
    return gfx::Rect(location_bar_view->GetOmniboxViewOrigin(),
                     gfx::Size(0, location_bar_view->omnibox_view()->height()));
  }
  return views::BubbleDelegateView::GetAnchorRect();
}

void ExtensionInstalledBubbleView::OnWidgetClosing(views::Widget* widget) {
  if (bubble_reference_) {
    DCHECK_EQ(widget, GetWidget());
    // A more specific close reason should already be recorded.
    // This is the catch-all close reason for this bubble.
    bubble_reference_->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST);
  }
}

void ExtensionInstalledBubbleView::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {
  if (!active && bubble_reference_ && widget == GetWidget())
    bubble_reference_->CloseBubble(BUBBLE_CLOSE_FOCUS_LOST);
}

bool ExtensionInstalledBubbleView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  if (!close_on_esc() || accelerator.key_code() != ui::VKEY_ESCAPE)
    return false;
  DCHECK(bubble_reference_);
  bool did_close = bubble_reference_->CloseBubble(BUBBLE_CLOSE_USER_DISMISSED);
  DCHECK(did_close);
  return true;
}

void ExtensionInstalledBubbleView::OnSignInLinkClicked() {
  GetWidget()->Close();
  chrome::ShowBrowserSignin(
      browser_,
      signin_metrics::AccessPoint::ACCESS_POINT_EXTENSION_INSTALL_BUBBLE);
}

void ExtensionInstalledBubbleView::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  DCHECK_EQ(sender, close_);
  GetWidget()->Close();
}

void ExtensionInstalledBubbleView::LinkClicked(views::Link* source,
                                               int event_flags) {
  DCHECK_EQ(manage_shortcut_, source);
  GetWidget()->Close();

  std::string configure_url = chrome::kChromeUIExtensionsURL;
  configure_url += chrome::kExtensionConfigureCommandsSubPage;
  chrome::NavigateParams params(
      chrome::GetSingletonTabNavigateParams(browser_, GURL(configure_url)));
  chrome::Navigate(&params);
}

void ExtensionInstalledBubbleView::InitLayout() {
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

  const ExtensionInstalledBubble& bubble = *bubble_;
  // The number of rows in the content section of the bubble.
  int main_content_row_count = 1;
  if (bubble.options() & ExtensionInstalledBubble::HOW_TO_USE)
    ++main_content_row_count;
  if (bubble.options() & ExtensionInstalledBubble::SHOW_KEYBINDING)
    ++main_content_row_count;
  if (bubble.options() & ExtensionInstalledBubble::HOW_TO_MANAGE)
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

  const SkBitmap& bitmap = bubble.icon();
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
  base::string16 extension_name = base::UTF8ToUTF16(extension_->name());
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

  if (bubble.options() & ExtensionInstalledBubble::HOW_TO_USE) {
    add_content_view(CreateLabel(bubble.GetHowToUseDescription(), font_list));
  }

  if (bubble.options() & ExtensionInstalledBubble::SHOW_KEYBINDING) {
    manage_shortcut_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_SHORTCUTS));
    manage_shortcut_->set_listener(this);
    manage_shortcut_->SetUnderline(false);
    add_content_view(manage_shortcut_);
  }

  if (bubble.options() & ExtensionInstalledBubble::HOW_TO_MANAGE) {
    add_content_view(CreateLabel(
        l10n_util::GetStringUTF16(IDS_EXTENSION_INSTALLED_MANAGE_INFO),
        font_list));
  }
}

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

class ExtensionInstalledBubbleUi : public BubbleUi {
 public:
  explicit ExtensionInstalledBubbleUi(ExtensionInstalledBubble* bubble);
  ~ExtensionInstalledBubbleUi() override;

 private:
  // BubbleUi:
  void Show(BubbleReference bubble_reference) override;
  void Close() override;
  void UpdateAnchorPosition() override;

  ExtensionInstalledBubble* bubble_;
  ExtensionInstalledBubbleView* delegate_view_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledBubbleUi);
};

// Implemented here to create the platform specific instance of the BubbleUi.
scoped_ptr<BubbleUi> ExtensionInstalledBubble::BuildBubbleUi() {
  return make_scoped_ptr(new ExtensionInstalledBubbleUi(this));
}

ExtensionInstalledBubbleUi::ExtensionInstalledBubbleUi(
    ExtensionInstalledBubble* bubble)
    : bubble_(bubble), delegate_view_(nullptr) {
  DCHECK(bubble_);
}

ExtensionInstalledBubbleUi::~ExtensionInstalledBubbleUi() {}

void ExtensionInstalledBubbleUi::Show(BubbleReference bubble_reference) {
  // Owned by widget.
  delegate_view_ = new ExtensionInstalledBubbleView(bubble_, bubble_reference);
  delegate_view_->UpdateAnchorView();

  delegate_view_->set_arrow(bubble_->type() == bubble_->OMNIBOX_KEYWORD
                                ? views::BubbleBorder::TOP_LEFT
                                : views::BubbleBorder::TOP_RIGHT);

  delegate_view_->InitLayout();

  views::BubbleDelegateView::CreateBubble(delegate_view_)->Show();
  content::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromExtensionInstallBubble"));
}

void ExtensionInstalledBubbleUi::Close() {
  if (delegate_view_) {
    delegate_view_->GetWidget()->Close();
    delegate_view_ = nullptr;
  }
}

void ExtensionInstalledBubbleUi::UpdateAnchorPosition() {
  DCHECK(delegate_view_);
  delegate_view_->UpdateAnchorView();
}
