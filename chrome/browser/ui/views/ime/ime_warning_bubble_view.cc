// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime/ime_warning_bubble_view.h"

#include <string>

#include "base/callback_helpers.h"
#include "chrome/browser/extensions/api/input_ime/input_ime_api_nonchromeos.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_button.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/feature_switch.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/layout/layout_constants.h"

using extensions::Extension;

namespace {

// The column width of the warning bubble.
const int kColumnWidth = 285;

views::Label* CreateLabel(const base::string16& text,
                          const gfx::FontList& font) {
  views::Label* label = new views::Label(text, font);
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

}  // namespace

// static
void ImeWarningBubbleView::ShowBubble(
    const extensions::Extension* extension,
    BrowserView* browser_view,
    const ImeWarningBubbleResponseCallback& callback) {
  // The ImeWarningBubbleView is self-owned, it deletes itself when the widget
  // is closed or the parent browser is destroyed.
  ImeWarningBubbleView::ime_warning_bubble_for_test_ =
      new ImeWarningBubbleView(extension, browser_view, callback);
}

bool ImeWarningBubbleView::Accept() {
  if (never_show_checkbox_->checked()) {
    base::ResetAndReturn(&response_callback_)
        .Run(ImeWarningBubblePermissionStatus::GRANTED_AND_NEVER_SHOW);
  } else {
    base::ResetAndReturn(&response_callback_)
        .Run(ImeWarningBubblePermissionStatus::GRANTED);
  }
  return true;
}

bool ImeWarningBubbleView::Cancel() {
  if (!response_callback_.is_null())
    base::ResetAndReturn(&response_callback_)
        .Run(ImeWarningBubblePermissionStatus::DENIED);
  return true;
}

bool ImeWarningBubbleView::ShouldDefaultButtonBeBlue() const {
  return true;
}

void ImeWarningBubbleView::OnToolbarActionsBarAnimationEnded() {
  if (!bubble_has_shown_) {
    views::BubbleDialogDelegateView::CreateBubble(this)->Show();
    bubble_has_shown_ = true;
  }
}

void ImeWarningBubbleView::OnBrowserRemoved(Browser* browser) {
  // |browser_| is being destroyed before the bubble is successfully shown.
  if ((browser_ == browser) && !bubble_has_shown_)
    delete this;
}

// static
ImeWarningBubbleView* ImeWarningBubbleView::ime_warning_bubble_for_test_ =
    nullptr;

ImeWarningBubbleView::ImeWarningBubbleView(
    const extensions::Extension* extension,
    BrowserView* browser_view,
    const ImeWarningBubbleResponseCallback& callback)
    : extension_(extension),
      browser_view_(browser_view),
      browser_(browser_view->browser()),
      anchor_to_browser_action_(false),
      never_show_checkbox_(nullptr),
      response_callback_(callback),
      bubble_has_shown_(false),
      toolbar_actions_bar_observer_(this),
      weak_ptr_factory_(this) {
  container_ = browser_view_->toolbar()->browser_actions();
  toolbar_actions_bar_ = container_->toolbar_actions_bar();
  BrowserList::AddObserver(this);

  // The lifetime of this bubble is tied to the lifetime of the browser.
  set_parent_window(
      platform_util::GetViewForWindow(browser_view_->GetNativeWindow()));
  InitAnchorView();
  InitLayout();

  // If the toolbar is not animating, shows the warning bubble directly.
  // Otherwise, shows the bubble in method OnToolbarActionsBarAnimationEnded().
  if (IsToolbarAnimating()) {
    toolbar_actions_bar_observer_.Add(toolbar_actions_bar_);
    return;
  }
  views::BubbleDialogDelegateView::CreateBubble(this)->Show();
  bubble_has_shown_ = true;
}

ImeWarningBubbleView::~ImeWarningBubbleView() {
  if (!response_callback_.is_null()) {
    base::ResetAndReturn(&response_callback_)
        .Run(ImeWarningBubblePermissionStatus::ABORTED);
  }

  BrowserList::RemoveObserver(this);
}

void ImeWarningBubbleView::InitAnchorView() {
  views::View* reference_view = nullptr;

  anchor_to_browser_action_ =
      extensions::ActionInfo::GetBrowserActionInfo(extension_) ||
      extensions::FeatureSwitch::extension_action_redesign()->IsEnabled();
  if (anchor_to_browser_action_) {
    // Anchors the bubble to the browser action of the extension.
    reference_view = container_->GetViewForId(extension_->id());
  }
  if (!reference_view || !reference_view->visible()) {
    // Anchors the bubble to the app menu.
    reference_view = browser_view_->toolbar()->app_menu_button();
  }
  SetAnchorView(reference_view);
  set_arrow(views::BubbleBorder::TOP_RIGHT);
}

void ImeWarningBubbleView::InitLayout() {
  // The Ime warning bubble is like this:
  //
  // -----------------------------------------
  // | Warning info                          |
  // |                                       |
  // | [check box] Never show this again.    |
  // |                                       |
  // |                    --------  -------- |
  // |                    |CANCEL|  |  OK  | |
  // |                    --------  -------- |
  // -----------------------------------------
  //

  views::GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  int cs_id = 0;

  views::ColumnSet* main_cs = layout->AddColumnSet(cs_id);
  // The first row which shows the warning info.
  main_cs->AddColumn(views::GridLayout::LEADING, views::GridLayout::LEADING, 0,
                     views::GridLayout::FIXED, kColumnWidth, 0);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  layout->StartRow(0, cs_id);
  base::string16 extension_name = base::UTF8ToUTF16(extension_->name());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  views::Label* warning = CreateLabel(
      l10n_util::GetStringFUTF16(IDS_IME_API_ACTIVATED_WARNING, extension_name),
      rb.GetFontList(ResourceBundle::BaseFont));
  layout->AddView(warning);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

  // The seconde row which shows the check box.
  layout->StartRow(0, cs_id);
  never_show_checkbox_ =
      new views::Checkbox(l10n_util::GetStringUTF16(IDS_IME_API_NEVER_SHOW));
  layout->AddView(never_show_checkbox_);
  layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
}

bool ImeWarningBubbleView::IsToolbarAnimating() {
  return anchor_to_browser_action_ && container_->animating();
}
