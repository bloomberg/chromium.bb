// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/password_generation_bubble_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/password_generator.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

using views::ColumnSet;
using views::GridLayout;

PasswordGenerationBubbleView::PasswordGenerationBubbleView(
    const gfx::Rect& anchor_rect,
    const webkit::forms::PasswordForm& form,
    views::View* anchor_view,
    content::RenderViewHost* render_view_host,
    autofill::PasswordGenerator* password_generator,
    content::PageNavigator* navigator,
    PasswordManager* password_manager)
    : BubbleDelegateView(anchor_view, views::BubbleBorder::TOP_LEFT),
      accept_button_(NULL),
      text_field_(NULL),
      anchor_rect_(anchor_rect),
      form_(form),
      render_view_host_(render_view_host),
      password_generator_(password_generator),
      navigator_(navigator),
      password_manager_(password_manager) {}

PasswordGenerationBubbleView::~PasswordGenerationBubbleView() {}

void PasswordGenerationBubbleView::Init() {
  // TODO(gcasto): Localize text after we have finalized the UI.
  // crbug.com/118062
  accept_button_ = new views::NativeTextButton(this,
                                               ASCIIToUTF16("Try It"));

  text_field_ = new views::Textfield();
  text_field_->SetText(
      ASCIIToUTF16(password_generator_->Generate()));

  views::Label* title_label = new views::Label(
      ASCIIToUTF16("Password Suggestion"));

  views::Link* learn_more_link = new views::Link(
      l10n_util::GetStringUTF16(IDS_LEARN_MORE));
  learn_more_link->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  learn_more_link->set_listener(this);

  GridLayout* layout = new GridLayout(this);
  SetLayoutManager(layout);

  // Title row.
  ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 100);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  // Input row
  cs = layout->AddColumnSet(1);
  cs->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 100);
  cs->AddPaddingColumn(1, views::kRelatedControlHorizontalSpacing);
  cs->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(title_label);
  layout->AddView(learn_more_link);

  layout->StartRow(0, 1);
  layout->AddView(text_field_);
  layout->AddView(accept_button_);
}

gfx::Rect PasswordGenerationBubbleView::GetAnchorRect() {
  return anchor_rect_;
}

void PasswordGenerationBubbleView::ButtonPressed(views::Button* sender,
                                                 const views::Event& event) {
  if (sender == accept_button_) {
    render_view_host_->Send(new AutofillMsg_GeneratedPasswordAccepted(
        render_view_host_->GetRoutingID(), text_field_->text()));
    password_manager_->SetFormHasGeneratedPassword(form_);
    StartFade(false);
  }
}

void PasswordGenerationBubbleView::LinkClicked(views::Link* source,
                                               int event_flags) {
  content::OpenURLParams params(
      GURL(chrome::kAutoPasswordGenerationLearnMoreURL), content::Referrer(),
      NEW_FOREGROUND_TAB, content::PAGE_TRANSITION_LINK, false);
  navigator_->OpenURL(params);
  StartFade(false);
}

views::View* PasswordGenerationBubbleView::GetInitiallyFocusedView() {
  return text_field_;
}
