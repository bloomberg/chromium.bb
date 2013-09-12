// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines helper functions shared by the various implementations
// of OmniboxView.

#include "chrome/browser/ui/omnibox/omnibox_view.h"

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "ui/base/clipboard/clipboard.h"

// static
string16 OmniboxView::StripJavascriptSchemas(const string16& text) {
  const string16 kJsPrefix(ASCIIToUTF16(content::kJavaScriptScheme) +
                           ASCIIToUTF16(":"));
  string16 out(text);
  while (StartsWith(out, kJsPrefix, false))
    TrimWhitespace(out.substr(kJsPrefix.length()), TRIM_LEADING, &out);
  return out;
}

// static
string16 OmniboxView::SanitizeTextForPaste(const string16& text) {
  // Check for non-newline whitespace; if found, collapse whitespace runs down
  // to single spaces.
  // TODO(shess): It may also make sense to ignore leading or
  // trailing whitespace when making this determination.
  for (size_t i = 0; i < text.size(); ++i) {
    if (IsWhitespace(text[i]) && text[i] != '\n' && text[i] != '\r') {
      const string16 collapsed = CollapseWhitespace(text, false);
      // If the user is pasting all-whitespace, paste a single space
      // rather than nothing, since pasting nothing feels broken.
      return collapsed.empty() ?
          ASCIIToUTF16(" ") : StripJavascriptSchemas(collapsed);
    }
  }

  // Otherwise, all whitespace is newlines; remove it entirely.
  return StripJavascriptSchemas(CollapseWhitespace(text, true));
}

// static
string16 OmniboxView::GetClipboardText() {
  // Try text format.
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  if (clipboard->IsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                                   ui::Clipboard::BUFFER_STANDARD)) {
    string16 text;
    clipboard->ReadText(ui::Clipboard::BUFFER_STANDARD, &text);
    return SanitizeTextForPaste(text);
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
  if (!IsEditingOrEmpty())
    return controller_->GetToolbarModel()->GetIcon();
  return AutocompleteMatch::TypeToLocationBarIcon(model_.get() ?
      model_->CurrentTextType() : AutocompleteMatchType::URL_WHAT_YOU_TYPED);
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
  controller_->GetToolbarModel()->set_search_term_replacement_enabled(true);
  RevertWithoutResettingSearchTermReplacement();
}

void OmniboxView::RevertWithoutResettingSearchTermReplacement() {
  CloseOmniboxPopup();
  if (model_.get())
    model_->Revert();
  TextChanged();
}

void OmniboxView::CloseOmniboxPopup() {
  if (model_.get())
    model_->StopAutocomplete();
}

bool OmniboxView::IsImeShowingPopup() const {
  // Default to claiming that the IME is not showing a popup, since hiding the
  // omnibox dropdown is a bad user experience when we don't know for sure that
  // we have to.
  return false;
}

bool OmniboxView::IsIndicatingQueryRefinement() const {
  // The default implementation always returns false.  Mobile ports can override
  // this method and implement as needed.
  return false;
}

OmniboxView::OmniboxView(Profile* profile,
                         OmniboxEditController* controller,
                         CommandUpdater* command_updater)
    : controller_(controller),
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

void OmniboxView::ShowURL() {
  controller_->GetToolbarModel()->set_search_term_replacement_enabled(false);
  model_->UpdatePermanentText();
  RevertWithoutResettingSearchTermReplacement();
  SelectAll(true);
}
