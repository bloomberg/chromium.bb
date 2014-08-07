// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines helper functions shared by the various implementations
// of OmniboxView.

#include "chrome/browser/ui/omnibox/omnibox_view.h"

#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_controller.h"
#include "chrome/browser/ui/toolbar/toolbar_model.h"
#include "components/omnibox/autocomplete_match.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "grit/component_scaled_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/l10n/l10n_util.h"

// static
base::string16 OmniboxView::StripJavascriptSchemas(const base::string16& text) {
  const base::string16 kJsPrefix(
      base::ASCIIToUTF16(url::kJavaScriptScheme) + base::ASCIIToUTF16(":"));
  base::string16 out(text);
  while (StartsWith(out, kJsPrefix, false)) {
    base::TrimWhitespace(out.substr(kJsPrefix.length()), base::TRIM_LEADING,
                         &out);
  }
  return out;
}

// static
base::string16 OmniboxView::SanitizeTextForPaste(const base::string16& text) {
  // Check for non-newline whitespace; if found, collapse whitespace runs down
  // to single spaces.
  // TODO(shess): It may also make sense to ignore leading or
  // trailing whitespace when making this determination.
  for (size_t i = 0; i < text.size(); ++i) {
    if (IsWhitespace(text[i]) && text[i] != '\n' && text[i] != '\r') {
      const base::string16 collapsed = base::CollapseWhitespace(text, false);
      // If the user is pasting all-whitespace, paste a single space
      // rather than nothing, since pasting nothing feels broken.
      return collapsed.empty() ?
          base::ASCIIToUTF16(" ") : StripJavascriptSchemas(collapsed);
    }
  }

  // Otherwise, all whitespace is newlines; remove it entirely.
  return StripJavascriptSchemas(base::CollapseWhitespace(text, true));
}

// static
base::string16 OmniboxView::GetClipboardText() {
  // Try text format.
  ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
  if (clipboard->IsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                                   ui::CLIPBOARD_TYPE_COPY_PASTE)) {
    base::string16 text;
    clipboard->ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &text);
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
                                   ui::CLIPBOARD_TYPE_COPY_PASTE)) {
    std::string url_str;
    clipboard->ReadBookmark(NULL, &url_str);
    // pass resulting url string through GURL to normalize
    GURL url(url_str);
    if (url.is_valid())
      return StripJavascriptSchemas(base::UTF8ToUTF16(url.spec()));
  }

  return base::string16();
}

OmniboxView::~OmniboxView() {
}

void OmniboxView::HandleOriginChipMouseRelease() {
  // Only hide if there isn't any current text in the Omnibox (e.g. search
  // terms).
  if (controller()->GetToolbarModel()->GetText().empty())
    controller()->HideOriginChip();
}

void OmniboxView::OnDidKillFocus() {
  if (chrome::ShouldDisplayOriginChip() && !model()->user_input_in_progress())
    controller()->ShowOriginChip();
}

void OmniboxView::OpenMatch(const AutocompleteMatch& match,
                            WindowOpenDisposition disposition,
                            const GURL& alternate_nav_url,
                            const base::string16& pasted_text,
                            size_t selected_line) {
  // Invalid URLs such as chrome://history can end up here.
  if (!match.destination_url.is_valid() || !model_)
    return;
  model_->OpenMatch(
      match, disposition, alternate_nav_url, pasted_text, selected_line);
  OnMatchOpened(match, controller_->GetWebContents());
}

bool OmniboxView::IsEditingOrEmpty() const {
  return (model_.get() && model_->user_input_in_progress()) ||
      (GetOmniboxTextLength() == 0);
}

int OmniboxView::GetIcon() const {
  if (!IsEditingOrEmpty())
    return controller_->GetToolbarModel()->GetIcon();
  int id = AutocompleteMatch::TypeToIcon(model_.get() ?
      model_->CurrentTextType() : AutocompleteMatchType::URL_WHAT_YOU_TYPED);
  return (id == IDR_OMNIBOX_HTTP) ? IDR_LOCATION_BAR_HTTP : id;
}

base::string16 OmniboxView::GetHintText() const {
  // If the user is in keyword mode (the "Search <some site>:" chip is showing)
  // then it doesn't make sense to show the "Search <default search engine>"
  // hint text.
  if (model_->is_keyword_selected())
    return base::string16();

  // Attempt to determine the default search provider and use that in the hint
  // text.
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(model_->profile());
  if (template_url_service) {
    TemplateURL* template_url =
        template_url_service->GetDefaultSearchProvider();
    if (template_url)
      return l10n_util::GetStringFUTF16(
          IDS_OMNIBOX_EMPTY_HINT_WITH_DEFAULT_SEARCH_PROVIDER,
          template_url->AdjustedShortNameForLocaleDirection());
  }

  // Otherwise return a hint based on there being no default search provider.
  return l10n_util::GetStringUTF16(
      IDS_OMNIBOX_EMPTY_HINT_NO_DEFAULT_SEARCH_PROVIDER);
}

void OmniboxView::SetUserText(const base::string16& text) {
  SetUserText(text, text, true);
}

void OmniboxView::SetUserText(const base::string16& text,
                              const base::string16& display_text,
                              bool update_popup) {
  if (model_.get())
    model_->SetUserText(text);
  SetWindowTextAndCaretPos(display_text, display_text.length(), update_popup,
                           true);
}

void OmniboxView::ShowURL() {
  SetFocus();
  controller_->GetToolbarModel()->set_origin_chip_enabled(false);
  controller_->GetToolbarModel()->set_url_replacement_enabled(false);
  model_->UpdatePermanentText();
  RevertWithoutResettingSearchTermReplacement();
  SelectAll(true);
}

void OmniboxView::HideURL() {
  controller_->GetToolbarModel()->set_origin_chip_enabled(true);
  controller_->GetToolbarModel()->set_url_replacement_enabled(true);
  model_->UpdatePermanentText();
  RevertWithoutResettingSearchTermReplacement();
}

void OmniboxView::RevertAll() {
  controller_->GetToolbarModel()->set_origin_chip_enabled(true);
  controller_->GetToolbarModel()->set_url_replacement_enabled(true);
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

void OmniboxView::ShowImeIfNeeded() {
}

bool OmniboxView::IsIndicatingQueryRefinement() const {
  // The default implementation always returns false.  Mobile ports can override
  // this method and implement as needed.
  return false;
}

void OmniboxView::OnMatchOpened(const AutocompleteMatch& match,
                                content::WebContents* web_contents) {
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
