// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/accessibility/invert_bubble_views.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/event_utils.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/page_navigator.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/sys_color_change_listener.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace {
const char kLearnMoreUrl[] = "https://groups.google.com/a/googleproductforums.com/d/topic/chrome/Xrco2HsXS-8/discussion";
const int kBubbleWidth = 500;
}  // namespace

class InvertBubbleView : public views::BubbleDelegateView,
                         public views::LinkListener {
 public:
  InvertBubbleView(Profile* profile, views::View* anchor_view);
  virtual ~InvertBubbleView();

  // views::BubbleDelegateView overrides:
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

 protected:
  // views::BubbleDelegateView overrides:
  virtual void Init() OVERRIDE;

  // views::LinkListener overrides:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(InvertBubbleView);
};

void InvertBubbleView::Init() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const gfx::Font& original_font = rb.GetFont(ResourceBundle::MediumFont);

  views::Label* title = new views::Label(
      l10n_util::GetStringUTF16(IDS_INVERT_NOTIFICATION));
  title->SetFont(original_font.DeriveFont(2, gfx::Font::BOLD));
  title->SetMultiLine(true);
  title->SizeToFit(kBubbleWidth);

  views::Link* learn_more =
    new views::Link(l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more->SetFont(original_font);
  learn_more->set_listener(this);

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(title);
  layout->StartRowWithPadding(0, 0, 0,
                              views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(learn_more);

  // Switching to high-contrast mode has a nasty habit of causing Chrome
  // top-level windows to lose focus, so closing the bubble on deactivate
  // makes it disappear before the user has even seen it. This forces the
  // user to close it explicitly, which should be okay because it affects
  // a small minority of users, and only once.
  set_close_on_deactivate(false);
}

gfx::Rect InvertBubbleView::GetAnchorRect() {
  // Set the height to 0 so we display the bubble at the top of the
  // anchor rect.
  gfx::Rect rect(BubbleDelegateView::GetAnchorRect());
  rect.set_height(0);
  return rect;
}

InvertBubbleView::InvertBubbleView(Profile* profile, views::View* anchor_view)
    : views::BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
    profile_(profile) {
}

InvertBubbleView::~InvertBubbleView() {
}

void InvertBubbleView::LinkClicked(views::Link* source, int event_flags) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (browser) {
    WindowOpenDisposition disposition =
        event_utils::DispositionFromEventFlags(event_flags);
    content::OpenURLParams params(
        GURL(kLearnMoreUrl), content::Referrer(),
        disposition == CURRENT_TAB ? NEW_FOREGROUND_TAB : disposition,
        content::PAGE_TRANSITION_LINK, false);
    browser->OpenURL(params);
  }
  GetWidget()->Close();
}

// static
void InvertBubble::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kInvertNotificationShown,
                             false,
                             PrefService::UNSYNCABLE_PREF);
}

void InvertBubble::MaybeShowInvertBubble(Profile* profile,
                                         views::View* anchor_view) {
  PrefService* pref_service = profile->GetPrefs();
  if (gfx::IsInvertedColorScheme() &&
      !pref_service->GetBoolean(prefs::kInvertNotificationShown)) {
    pref_service->SetBoolean(prefs::kInvertNotificationShown, true);
    InvertBubbleView* delegate = new InvertBubbleView(profile, anchor_view);
    views::BubbleDelegateView::CreateBubble(delegate);
    delegate->StartFade(true);
  }
}
