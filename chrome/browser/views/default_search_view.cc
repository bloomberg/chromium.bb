// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/default_search_view.h"

#include <string>

#include "app/l10n_util.h"
#include "app/message_box_flags.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "views/controls/message_box_view.h"

// static
void DefaultSearchView::Show(TabContents* tab_contents,
                             TemplateURL* default_url,
                             TemplateURLModel* template_url_model) {
  scoped_ptr<TemplateURL> template_url(default_url);
  if (!template_url_model->CanMakeDefault(default_url) ||
      default_url->url()->GetHost().empty())
    return;

  // When the window closes, it will delete itself.
  new DefaultSearchView(tab_contents, template_url.release(),
                        template_url_model);
}

std::wstring DefaultSearchView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  switch (button) {
    case MessageBoxFlags::DIALOGBUTTON_OK:
      return l10n_util::GetString(IDS_DEFAULT_SEARCH_PROMPT_OK);

    case MessageBoxFlags::DIALOGBUTTON_CANCEL:
      return l10n_util::GetString(IDS_DEFAULT_SEARCH_PROMPT_CANCEL);

    case MessageBoxFlags::DIALOGBUTTON_NONE:
      NOTREACHED();
      break;
  }

  NOTREACHED();
  return std::wstring();
}

std::wstring DefaultSearchView::GetWindowTitle() const {
  return l10n_util::GetString(IDS_DEFAULT_SEARCH_PROMPT_TITLE);
}

void DefaultSearchView::DeleteDelegate() {
  delete this;
}

views::View* DefaultSearchView::GetContentsView() {
  return message_box_view_;
}

int DefaultSearchView::GetDefaultDialogButton() const {
  // To less the likelihood of an accidental default search engine change, make
  // the cancel button the default dialog button.
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

bool DefaultSearchView::Accept() {
  // Check this again in case the default became managed while this dialog was
  // displayed.
  if (!template_url_model_->CanMakeDefault(default_url_.get()))
    return true;

  TemplateURL* set_as_default = default_url_.get();
  template_url_model_->Add(default_url_.release());
  template_url_model_->SetDefaultSearchProvider(set_as_default);
  return true;
}

DefaultSearchView::DefaultSearchView(TabContents* tab_contents,
                                     TemplateURL* default_url,
                                     TemplateURLModel* template_url_model)
    : default_url_(default_url),
      template_url_model_(template_url_model) {
  const int kDialogWidth = 420;

  // Also deleted when the window closes.
  message_box_view_ = new MessageBoxView(
      0,
      l10n_util::GetStringF(IDS_DEFAULT_SEARCH_PROMPT,
                            DefaultHostName()),
      std::wstring(),
      kDialogWidth);
  tab_contents->CreateConstrainedDialog(this);
}

DefaultSearchView::~DefaultSearchView() {
}

string16 DefaultSearchView::DefaultHostName() const {
  return UTF8ToUTF16(default_url_->url()->GetHost());
}
