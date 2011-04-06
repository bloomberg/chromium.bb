// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/instant_confirm_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/instant/instant_confirm_dialog.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/label.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/window/window.h"

InstantConfirmView::InstantConfirmView(Profile* profile) : profile_(profile) {
  views::Label* description_label = new views::Label(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_INSTANT_OPT_IN_MESSAGE)));
  description_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  description_label->SetMultiLine(true);

  views::Link* learn_more_link = new views::Link(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_LEARN_MORE)));
  learn_more_link->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  learn_more_link->SetController(this);

  views::GridLayout* layout = views::GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  const int first_column_set = 1;
  views::ColumnSet* column_set = layout->AddColumnSet(first_column_set);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, first_column_set);
  layout->AddView(description_label);
  layout->StartRow(0, first_column_set);
  layout->AddView(learn_more_link);
}

bool InstantConfirmView::Accept(bool window_closing) {
  return Accept();
}

bool InstantConfirmView::Accept() {
  InstantController::Enable(profile_);
  return true;
}

bool InstantConfirmView::Cancel() {
  return true;
}

views::View* InstantConfirmView::GetContentsView() {
  return this;
}

std::wstring InstantConfirmView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_INSTANT_OPT_IN_TITLE));
}

gfx::Size InstantConfirmView::GetPreferredSize() {
  DCHECK(GetLayoutManager());
  int pref_width = views::Window::GetLocalizedContentsWidth(
      IDS_INSTANT_CONFIRM_DIALOG_WIDTH_CHARS);
  int pref_height =
      GetLayoutManager()->GetPreferredHeightForWidth(this, pref_width);
  return gfx::Size(pref_width, pref_height);
}

bool InstantConfirmView::IsModal() const {
  return true;
}

void InstantConfirmView::LinkActivated(views::Link* source, int event_flags) {
  Browser* browser = BrowserList::GetLastActive();
  browser->OpenURL(browser::InstantLearnMoreURL(), GURL(),
                   NEW_FOREGROUND_TAB, PageTransition::TYPED);
}

namespace browser {

void ShowInstantConfirmDialog(gfx::NativeWindow parent, Profile* profile) {
  views::Window::CreateChromeWindow(parent, gfx::Rect(),
                                    new InstantConfirmView(profile))->Show();
}

}  // namespace browser
