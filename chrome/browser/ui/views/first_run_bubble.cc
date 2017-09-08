// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/first_run_bubble.h"

#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/event_monitor.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/widget/widget.h"

namespace {

FirstRunBubble* ShowBubbleImpl(Browser* browser,
                               views::View* anchor_view,
                               gfx::NativeWindow parent_window) {
  first_run::LogFirstRunMetric(first_run::FIRST_RUN_BUBBLE_SHOWN);
  FirstRunBubble* delegate =
      new FirstRunBubble(browser, anchor_view, parent_window);
  views::BubbleDialogDelegateView::CreateBubble(delegate)->ShowInactive();
  return delegate;
}

}  // namespace

void FirstRunBubble::Init() {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::FontList& original_font_list =
      rb.GetFontList(ui::ResourceBundle::MediumFont);

  const base::string16 search_engine_name =
      browser_
          ? GetDefaultSearchEngineName(
                TemplateURLServiceFactory::GetForProfile(browser_->profile()))
          : base::string16();

  // TODO(tapted): Update these when there are mocks. http://crbug.com/699338.
  views::Label* title = new views::Label(
      l10n_util::GetStringFUTF16(IDS_FR_BUBBLE_TITLE, search_engine_name),
      views::Label::CustomFont{original_font_list.Derive(
          2, gfx::Font::NORMAL, gfx::Font::Weight::BOLD)});

  views::Link* change =
      new views::Link(l10n_util::GetStringUTF16(IDS_FR_BUBBLE_CHANGE));
  change->SetFontList(original_font_list);
  change->set_listener(this);

  views::Label* subtext = new views::Label(
      l10n_util::GetStringUTF16(IDS_FR_BUBBLE_SUBTEXT), {original_font_list});

  views::GridLayout* layout = views::GridLayout::CreateAndInstall(this);

  ChromeLayoutProvider* provider = ChromeLayoutProvider::Get();

  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(0, provider->GetDistanceMetric(
                                   views::DISTANCE_RELATED_CONTROL_HORIZONTAL));
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(1, 0);

  layout->StartRow(0, 0);
  layout->AddView(title);
  layout->AddView(change);
  layout->StartRowWithPadding(
      0, 0, 0,
      provider->GetDistanceMetric(DISTANCE_RELATED_CONTROL_VERTICAL_SMALL));
  layout->AddView(subtext, columns->num_columns(), 1);
}

FirstRunBubble::FirstRunBubble(Browser* browser,
                               views::View* anchor_view,
                               gfx::NativeWindow parent_window)
    : views::BubbleDialogDelegateView(nullptr, views::BubbleBorder::TOP_LEFT),
      browser_(browser),
      bubble_closer_(this, parent_window) {
  constexpr int kTopInset = 1;
  constexpr int kHorizontalInset = 2;
  constexpr int kBottomInset = 7;
  set_margins(
      gfx::Insets(kTopInset, kHorizontalInset, kBottomInset, kHorizontalInset));
  if (anchor_view) {
    // Compensate for built-in vertical padding in the anchor view's image.
    set_anchor_view_insets(gfx::Insets(
        GetLayoutConstant(LOCATION_BAR_BUBBLE_ANCHOR_VERTICAL_INSET), 0));
    SetAnchorView(anchor_view);
  } else {
    SetAnchorRect(bubble_anchor_util::GetPageInfoAnchorRect(browser));
  }
  chrome::RecordDialogCreation(chrome::DialogIdentifier::FIRST_RUN);
}

FirstRunBubble::~FirstRunBubble() {}

void FirstRunBubble::Show(Browser* browser) {
  ShowBubbleImpl(browser, bubble_anchor_util::GetPageInfoAnchorView(browser),
                 browser->window()->GetNativeWindow());
}

FirstRunBubble* FirstRunBubble::ShowForTest(views::View* anchor_view) {
  return ShowBubbleImpl(nullptr, anchor_view,
                        anchor_view->GetWidget()->GetNativeWindow());
}

int FirstRunBubble::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_NONE;
}

void FirstRunBubble::LinkClicked(views::Link* source, int event_flags) {
  first_run::LogFirstRunMetric(first_run::FIRST_RUN_BUBBLE_CHANGE_INVOKED);

  GetWidget()->Close();
  if (browser_)
    chrome::ShowSearchEngineSettings(browser_);
}

FirstRunBubble::FirstRunBubbleCloser::FirstRunBubbleCloser(
    FirstRunBubble* bubble,
    gfx::NativeWindow parent)
    : bubble_(bubble) {
  event_monitor_ = views::EventMonitor::CreateWindowMonitor(this, parent);
}

FirstRunBubble::FirstRunBubbleCloser::~FirstRunBubbleCloser() {}

void FirstRunBubble::FirstRunBubbleCloser::OnKeyEvent(ui::KeyEvent* event) {
  CloseBubble();
}

void FirstRunBubble::FirstRunBubbleCloser::OnMouseEvent(
    ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    CloseBubble();
}

void FirstRunBubble::FirstRunBubbleCloser::OnGestureEvent(
    ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_TAP_DOWN) {
    CloseBubble();
  }
}

void FirstRunBubble::FirstRunBubbleCloser::CloseBubble() {
  if (!event_monitor_)
    return;

  event_monitor_.reset();
  DCHECK(bubble_);
  bubble_->GetWidget()->Close();
  bubble_ = nullptr;
}
