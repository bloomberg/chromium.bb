// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/accessibility/invert_bubble_view.h"

#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/page_navigator.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

namespace {

const char kHighContrastExtensionUrl[] = "https://chrome.google.com/webstore/detail/djcfdncoelnlbldjfhinnjlhdjlikmph";
const char kDarkThemeSearchUrl[] = "https://chrome.google.com/webstore/search-themes/dark";
const char kLearnMoreUrl[] = "https://groups.google.com/a/googleproductforums.com/d/topic/chrome/Xrco2HsXS-8/discussion";
const int kBubbleWidth = 500;

class InvertBubbleView : public views::BubbleDelegateView,
                         public views::LinkListener {
 public:
  InvertBubbleView(Browser* browser, views::View* anchor_view);
  virtual ~InvertBubbleView();

 private:
  // Overridden from views::BubbleDelegateView:
  virtual void Init() OVERRIDE;
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  void OpenLink(const std::string& url, int event_flags);

  Browser* browser_;
  views::Link* high_contrast_;
  views::Link* dark_theme_;
  views::Link* learn_more_;
  views::Link* close_;

  DISALLOW_COPY_AND_ASSIGN(InvertBubbleView);
};

InvertBubbleView::InvertBubbleView(Browser* browser, views::View* anchor_view)
    : views::BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      browser_(browser),
      high_contrast_(NULL),
      dark_theme_(NULL),
      learn_more_(NULL),
      close_(NULL) {
}

InvertBubbleView::~InvertBubbleView() {
}

void InvertBubbleView::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::Font& original_font = rb.GetFont(ui::ResourceBundle::MediumFont);

  views::Label* title = new views::Label(
      l10n_util::GetStringUTF16(IDS_HIGH_CONTRAST_NOTIFICATION));
  title->SetFont(original_font.DeriveFont(2, gfx::Font::BOLD));
  title->SetMultiLine(true);
  title->SizeToFit(kBubbleWidth);

  learn_more_ = new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_->SetFont(original_font);
  learn_more_->set_listener(this);

  high_contrast_ =
      new views::Link(l10n_util::GetStringUTF16(IDS_HIGH_CONTRAST_EXT));
  high_contrast_->SetFont(original_font);
  high_contrast_->set_listener(this);

  dark_theme_ =
      new views::Link(l10n_util::GetStringUTF16(IDS_DARK_THEME));
  dark_theme_->SetFont(original_font);
  dark_theme_->set_listener(this);

  close_ = new views::Link(l10n_util::GetStringUTF16(IDS_CLOSE));
  close_->SetFont(original_font);
  close_->set_listener(this);

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  for (int i = 0; i < 4; i++) {
    columns->AddColumn(views::GridLayout::LEADING,
                       views::GridLayout::LEADING, 0,
                       views::GridLayout::USE_PREF, 0, 0);
  }

  layout->StartRow(0, 0);
  layout->AddView(title, 4, 1);
  layout->StartRowWithPadding(0, 0, 0,
                              views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(high_contrast_);
  layout->AddView(dark_theme_);
  layout->AddView(learn_more_);
  layout->AddView(close_);

  // Switching to high-contrast mode has a nasty habit of causing Chrome
  // top-level windows to lose focus, so closing the bubble on deactivate
  // makes it disappear before the user has even seen it. This forces the
  // user to close it explicitly, which should be okay because it affects
  // a small minority of users, and only once.
  set_close_on_deactivate(false);
  set_move_with_anchor(true);
}

gfx::Rect InvertBubbleView::GetAnchorRect() {
  // Set the height to 0 so we display the bubble at the top of the
  // anchor rect.
  gfx::Rect rect(BubbleDelegateView::GetAnchorRect());
  rect.set_height(0);
  return rect;
}

void InvertBubbleView::LinkClicked(views::Link* source, int event_flags) {
  if (source == high_contrast_)
    OpenLink(kHighContrastExtensionUrl, event_flags);
  else if (source == dark_theme_)
    OpenLink(kDarkThemeSearchUrl, event_flags);
  else if (source == learn_more_)
    OpenLink(kLearnMoreUrl, event_flags);
  else if (source == close_)
    GetWidget()->Close();
  else
    NOTREACHED();
}

void InvertBubbleView::OpenLink(const std::string& url, int event_flags) {
  WindowOpenDisposition disposition =
      ui::DispositionFromEventFlags(event_flags);
  content::OpenURLParams params(
      GURL(url), content::Referrer(),
      disposition == CURRENT_TAB ? NEW_FOREGROUND_TAB : disposition,
      content::PAGE_TRANSITION_LINK, false);
  browser_->OpenURL(params);
}

}  // namespace

namespace chrome {

void MaybeShowInvertBubbleView(Browser* browser, views::View* anchor_view) {
  PrefService* pref_service = browser->profile()->GetPrefs();
  if (gfx::IsInvertedColorScheme() &&
      !pref_service->GetBoolean(prefs::kInvertNotificationShown)) {
    pref_service->SetBoolean(prefs::kInvertNotificationShown, true);
    InvertBubbleView* delegate = new InvertBubbleView(browser, anchor_view);
    views::BubbleDelegateView::CreateBubble(delegate);
    delegate->StartFade(true);
  }
}

}  // namespace chrome
