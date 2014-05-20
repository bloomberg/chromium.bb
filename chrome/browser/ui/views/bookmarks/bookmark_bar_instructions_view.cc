// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_instructions_view.h"

#include <algorithm>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_instructions_delegate.h"
#include "grit/generated_resources.h"
#include "ui/accessibility/ax_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/theme_provider.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"

namespace {

// Horizontal padding, in pixels, between the link and label.
const int kViewPadding = 6;

}  // namespace

BookmarkBarInstructionsView::BookmarkBarInstructionsView(
    BookmarkBarInstructionsDelegate* delegate)
    : delegate_(delegate),
      instructions_(NULL),
      import_link_(NULL),
      baseline_(-1),
      updated_colors_(false) {
  instructions_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_BOOKMARKS_NO_ITEMS));
  instructions_->SetAutoColorReadabilityEnabled(false);
  AddChildView(instructions_);

  if (browser_defaults::kShowImportOnBookmarkBar) {
    import_link_ = new views::Link(
        l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_IMPORT_LINK));
    // We don't want the link to alter tab navigation.
    import_link_->SetFocusable(false);
    import_link_->set_listener(this);
    import_link_->set_context_menu_controller(this);
    import_link_->SetAutoColorReadabilityEnabled(false);
    AddChildView(import_link_);
  }
}

gfx::Size BookmarkBarInstructionsView::GetPreferredSize() const {
  int ascent = 0, descent = 0, height = 0, width = 0;
  for (int i = 0; i < child_count(); ++i) {
    const views::View* view = child_at(i);
    gfx::Size pref = view->GetPreferredSize();
    int baseline = view->GetBaseline();
    if (baseline != -1) {
      ascent = std::max(ascent, baseline);
      descent = std::max(descent, pref.height() - baseline);
    } else {
      height = std::max(pref.height(), height);
    }
    width += pref.width();
  }
  width += (child_count() - 1) * kViewPadding;
  if (ascent != 0)
    height = std::max(ascent + descent, height);
  return gfx::Size(width, height);
}

void BookmarkBarInstructionsView::Layout() {
  int remaining_width = width();
  int x = 0;
  for (int i = 0; i < child_count(); ++i) {
    views::View* view = child_at(i);
    gfx::Size pref = view->GetPreferredSize();
    int baseline = view->GetBaseline();
    int y;
    if (baseline != -1 && baseline_ != -1)
      y = baseline_ - baseline;
    else
      y = (height() - pref.height()) / 2;
    int view_width = std::min(remaining_width, pref.width());
    view->SetBounds(x, y, view_width, pref.height());
    x += view_width + kViewPadding;
    remaining_width = std::max(0, width() - x);
  }
}

void BookmarkBarInstructionsView::OnThemeChanged() {
  UpdateColors();
}

void BookmarkBarInstructionsView::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (!updated_colors_ && details.is_add && GetWidget())
    UpdateColors();
}

void BookmarkBarInstructionsView::GetAccessibleState(
    ui::AXViewState* state) {
  state->role = ui::AX_ROLE_GROUP;
}

void BookmarkBarInstructionsView::LinkClicked(views::Link* source,
                                              int event_flags) {
  delegate_->ShowImportDialog();
}

void BookmarkBarInstructionsView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // Do nothing here, we don't want to show the Bookmarks context menu when
  // the user right clicks on the "Import bookmarks now" link.
}

void BookmarkBarInstructionsView::UpdateColors() {
  // We don't always have a theme provider (ui tests, for example).
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;
  updated_colors_ = true;
  SkColor text_color =
      theme_provider->GetColor(ThemeProperties::COLOR_BOOKMARK_TEXT);
  instructions_->SetEnabledColor(text_color);
  if (import_link_)
    import_link_->SetEnabledColor(text_color);
}
