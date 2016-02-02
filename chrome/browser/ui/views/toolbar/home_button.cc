// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/home_button.h"

#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/settings_api_bubble_helper_views.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

// HomePageUndoBubble --------------------------------------------------------

namespace {

class HomePageUndoBubble : public views::BubbleDelegateView,
                           public views::LinkListener {
 public:
  static void ShowBubble(Browser* browser,
                         bool undo_value_is_ntp,
                         const GURL& undo_url,
                         views::View* anchor_view);
  static void HideBubble();

 private:
  HomePageUndoBubble(Browser* browser, bool undo_value_is_ntp,
                     const GURL& undo_url, views::View* anchor_view);
  ~HomePageUndoBubble() override;

  // views::BubbleDelegateView:
  void Init() override;
  void WindowClosing() override;

  // views::LinkListener:
  void LinkClicked(views::Link* source, int event_flags) override;

  static HomePageUndoBubble* home_page_undo_bubble_;

  Browser* browser_;
  bool undo_value_is_ntp_;
  GURL undo_url_;

  DISALLOW_COPY_AND_ASSIGN(HomePageUndoBubble);
};

// static
HomePageUndoBubble* HomePageUndoBubble::home_page_undo_bubble_ = NULL;

void HomePageUndoBubble::ShowBubble(Browser* browser,
                                    bool undo_value_is_ntp,
                                    const GURL& undo_url,
                                    views::View* anchor_view) {
  HideBubble();
  home_page_undo_bubble_ = new HomePageUndoBubble(browser,
                                                  undo_value_is_ntp,
                                                  undo_url,
                                                  anchor_view);
  views::BubbleDelegateView::CreateBubble(home_page_undo_bubble_)->Show();
}

void HomePageUndoBubble::HideBubble() {
  if (home_page_undo_bubble_)
    home_page_undo_bubble_->GetWidget()->Close();
}

HomePageUndoBubble::HomePageUndoBubble(
    Browser* browser,
    bool undo_value_is_ntp,
    const GURL& undo_url,
    views::View* anchor_view)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      browser_(browser),
      undo_value_is_ntp_(undo_value_is_ntp),
      undo_url_(undo_url) {
}

HomePageUndoBubble::~HomePageUndoBubble() {
}

void HomePageUndoBubble::Init() {
  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  // Create two columns for the message and the undo link.
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs = layout->AddColumnSet(1);
  cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::BASELINE, 0,
                views::GridLayout::USE_PREF, 0, 0);
  cs->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(views::GridLayout::CENTER, views::GridLayout::BASELINE, 0,
                views::GridLayout::USE_PREF, 0, 0);

  views::Label* message_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_TOOLBAR_INFORM_SET_HOME_PAGE));
  message_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->StartRow(0, 1);
  layout->AddView(message_label);

  views::Link* undo_link = new views::Link(
      l10n_util::GetStringUTF16(IDS_ONE_CLICK_BUBBLE_UNDO));
  undo_link->set_listener(this);
  layout->AddView(undo_link);
}

void HomePageUndoBubble::LinkClicked(views::Link* source, int event_flags) {
  PrefService* prefs = user_prefs::UserPrefs::Get(browser_->profile());
  prefs->SetBoolean(prefs::kHomePageIsNewTabPage, undo_value_is_ntp_);
  prefs->SetString(prefs::kHomePage, undo_url_.spec());

  HideBubble();
}

void HomePageUndoBubble::WindowClosing() {
  // We have to reset |home_page_undo_bubble_| here, not in our destructor,
  // because we'll be hidden first, then destroyed asynchronously.  If we wait
  // to reset this, and the user triggers a call to ShowBubble() while the
  // window is hidden but not destroyed, GetWidget()->Close() would be
  // called twice.
  DCHECK_EQ(this, home_page_undo_bubble_);
  home_page_undo_bubble_ = NULL;
}

}  // namespace


// HomeButton -----------------------------------------------------------

HomeButton::HomeButton(views::ButtonListener* listener, Browser* browser)
    : ToolbarButton(browser->profile(), listener, nullptr), browser_(browser) {}

HomeButton::~HomeButton() {
}

const char* HomeButton::GetClassName() const {
  return "HomeButton";
}

bool HomeButton::GetDropFormats(
    int* formats,
    std::set<ui::Clipboard::FormatType>* format_types) {
  *formats = ui::OSExchangeData::URL;
  return true;
}

bool HomeButton::CanDrop(const OSExchangeData& data) {
  return data.HasURL(ui::OSExchangeData::CONVERT_FILENAMES);
}

int HomeButton::OnDragUpdated(const ui::DropTargetEvent& event) {
  return (event.source_operations() & ui::DragDropTypes::DRAG_LINK) ?
      ui::DragDropTypes::DRAG_LINK : ui::DragDropTypes::DRAG_NONE;
}

int HomeButton::OnPerformDrop(const ui::DropTargetEvent& event) {
  GURL new_homepage_url;
  base::string16 title;
  if (event.data().GetURLAndTitle(
          ui::OSExchangeData::CONVERT_FILENAMES, &new_homepage_url, &title) &&
      new_homepage_url.is_valid()) {
    PrefService* prefs = browser_->profile()->GetPrefs();
    bool old_is_ntp = prefs->GetBoolean(prefs::kHomePageIsNewTabPage);
    GURL old_homepage(prefs->GetString(prefs::kHomePage));

    prefs->SetBoolean(prefs::kHomePageIsNewTabPage, false);
    prefs->SetString(prefs::kHomePage, new_homepage_url.spec());

    HomePageUndoBubble::ShowBubble(browser_, old_is_ntp, old_homepage, this);
  }
  return ui::DragDropTypes::DRAG_NONE;
}

void HomeButton::NotifyClick(const ui::Event& event) {
  ToolbarButton::NotifyClick(event);
  extensions::MaybeShowExtensionControlledHomeNotification(browser_);
}
