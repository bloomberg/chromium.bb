// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines helper functions shared by the various implementations
// of OmniboxView.

#include "chrome/browser/ui/omnibox/omnibox_view.h"

#include "base/string_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/browser_process.h"
#include "ui/base/clipboard/clipboard.h"

// static
string16 OmniboxView::StripJavascriptSchemas(const string16& text) {
  const string16 kJsPrefix(ASCIIToUTF16(chrome::kJavaScriptScheme) +
                           ASCIIToUTF16(":"));
  string16 out(text);
  while (StartsWith(out, kJsPrefix, false))
    TrimWhitespace(out.substr(kJsPrefix.length()), TRIM_LEADING, &out);
  return out;
}

// static
string16 OmniboxView::GetClipboardText() {
  // Try text format.
  ui::Clipboard* clipboard = g_browser_process->clipboard();
  if (clipboard->IsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                                   ui::Clipboard::BUFFER_STANDARD)) {
    string16 text;
    clipboard->ReadText(ui::Clipboard::BUFFER_STANDARD, &text);

    // If the input contains non-newline whitespace, treat it as
    // search data and convert newlines to spaces.  For instance, a
    // street address.
    // TODO(shess): It may also make sense to ignore leading or
    // trailing whitespace when making this determination.
    for (size_t i = 0; i < text.size(); ++i) {
      if (IsWhitespace(text[i]) && text[i] != '\n' && text[i] != '\r')
        return StripJavascriptSchemas(CollapseWhitespace(text, false));
    }

    // Otherwise, the only whitespace in |text| is newlines.  Remove
    // these entirely, because users are most likely pasting URLs
    // split into multiple lines by terminals, email programs, etc.
    return StripJavascriptSchemas(CollapseWhitespace(text, true));
  }

  // Try bookmark format.
  //
  // It is tempting to try bookmark format first, but the URL we get out of a
  // bookmark has been cannonicalized via GURL.  This means if a user copies
  // and pastes from the URL bar to itself, the text will get fixed up and
  // cannonicalized, which is not what the user expects.  By pasting in this
  // order, we are sure to paste what the user copied.
  if (clipboard->IsFormatAvailable(ui::Clipboard::GetUrlWFormatType(),
                                   ui::Clipboard::BUFFER_STANDARD)) {
    std::string url_str;
    clipboard->ReadBookmark(NULL, &url_str);
    // pass resulting url string through GURL to normalize
    GURL url(url_str);
    if (url.is_valid())
      return StripJavascriptSchemas(UTF8ToUTF16(url.spec()));
  }

  return string16();
}

OmniboxView::~OmniboxView() {
}

void OmniboxView::OpenMatch(const AutocompleteMatch& match,
                            WindowOpenDisposition disposition,
                            const GURL& alternate_nav_url,
                            size_t selected_line) {
  // Invalid URLs such as chrome://history can end up here.
  if (!match.destination_url.is_valid())
    return;
  if (model_.get())
    model_->OpenMatch(match, disposition, alternate_nav_url, selected_line);
}

bool OmniboxView::IsEditingOrEmpty() const {
  return (model_.get() && model_->user_input_in_progress()) ||
      (GetOmniboxTextLength() == 0);
}

int OmniboxView::GetIcon() const {
  if (IsEditingOrEmpty()) {
    return AutocompleteMatch::TypeToIcon(model_.get() ?
          model_->CurrentTextType() : AutocompleteMatch::URL_WHAT_YOU_TYPED);
  } else {
    return toolbar_model_->GetIcon();
  }
}

void OmniboxView::SetUserText(const string16& text) {
  SetUserText(text, text, true);
}

void OmniboxView::SetUserText(const string16& text,
                              const string16& display_text,
                              bool update_popup) {
  if (model_.get())
    model_->SetUserText(text);
  SetWindowTextAndCaretPos(display_text, display_text.length(), update_popup,
                           true);
}

void OmniboxView::RevertAll() {
  CloseOmniboxPopup();
  if (model_.get())
    model_->Revert();
  TextChanged();
}

void OmniboxView::CloseOmniboxPopup() {
  if (model_.get())
    model_->StopAutocomplete();
}

OmniboxView::OmniboxView(Profile* profile,
                         OmniboxEditController* controller,
                         ToolbarModel* toolbar_model,
                         CommandUpdater* command_updater)
    : controller_(controller),
      toolbar_model_(toolbar_model),
      command_updater_(command_updater) {
  // |profile| can be NULL in tests.
  if (profile)
    model_.reset(new OmniboxEditModel(this, controller, profile));
}

void OmniboxView::TextChanged() {
  EmphasizeURLComponents();
  if (model_.get())
    model_->OnChanged();
}
