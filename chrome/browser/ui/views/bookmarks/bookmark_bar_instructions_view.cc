// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bar_instructions_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/themes/browser_theme_provider.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/label.h"

using views::View;

// Horizontal padding, in pixels, between the link and label.
static const int kViewPadding = 6;

BookmarkBarInstructionsView::BookmarkBarInstructionsView(Delegate* delegate)
    : delegate_(delegate),
      instructions_(NULL),
      import_link_(NULL),
      baseline_(-1),
      updated_colors_(false) {
  instructions_ = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOKMARKS_NO_ITEMS)));
  AddChildView(instructions_);

  if (browser_defaults::kShowImportOnBookmarkBar) {
    import_link_ = new views::Link(
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_IMPORT_LINK)));
    // We don't want the link to alter tab navigation.
    import_link_->SetFocusable(false);
    import_link_->SetController(this);
    AddChildView(import_link_);
  }
}

gfx::Size BookmarkBarInstructionsView::GetPreferredSize() {
  int ascent = 0, descent = 0, height = 0, width = 0;
  for (int i = 0; i < child_count(); ++i) {
    View* view = GetChildViewAt(i);
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
    View* view = GetChildViewAt(i);
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

void BookmarkBarInstructionsView::ViewHierarchyChanged(bool is_add,
                                                       View* parent,
                                                       View* child) {
  if (!updated_colors_ && is_add && GetWidget())
    UpdateColors();
}

void BookmarkBarInstructionsView::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
}

void BookmarkBarInstructionsView::LinkActivated(views::Link* source,
                                                int event_flags) {
  delegate_->ShowImportDialog();
}

void BookmarkBarInstructionsView::UpdateColors() {
  // We don't always have a theme provider (ui tests, for example).
  const ui::ThemeProvider* theme_provider = GetThemeProvider();
  if (!theme_provider)
    return;
  updated_colors_ = true;
  SkColor text_color =
      theme_provider->GetColor(BrowserThemeProvider::COLOR_BOOKMARK_TEXT);
  instructions_->SetColor(text_color);
  if (import_link_)
    import_link_->SetColor(text_color);
}
