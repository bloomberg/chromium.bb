// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"

#include "base/i18n/message_formatter.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/save_card_bubble_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "content/public/browser/navigation_details.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(autofill::SaveCardBubbleControllerImpl);

namespace autofill {

namespace {

// Number of seconds the bubble and icon will survive navigations, starting
// from when the bubble is shown.
// TODO(bondd): Share with ManagePasswordsUIController.
const int kSurviveNavigationSeconds = 5;

// Replace "{0}", "{1}", ... in |template_icu| with corresponding strings
// from |display_texts|. Sets |out_message| to the resulting string, with
// start position of each replacement in |out_offsets|.
// Return false on failure. If false is returned then contents of |out_message|
// and |out_offsets| are undefined.
bool ReplaceTemplatePlaceholders(
    const base::string16& template_icu,
    const std::vector<base::string16>& display_texts,
    base::string16* out_message,
    std::vector<size_t>* out_offsets) {
  // Escape "$" -> "$$" for ReplaceStringPlaceholders().
  //
  // Edge cases:
  // 1. Two or more consecutive $ characters will be incorrectly expanded
  //    ("$$" -> "$$$$", which ReplaceStringPlaceholders() then turns into
  //    "$$$").
  //
  // 2. "${" will cause false to be returned. "${0}" will expand to "$${0}".
  //    FormatWithNumberedArgs() turns it into "$$$1", which
  //    ReplaceStringPlaceholders() then turns into "$$1" without doing the
  //    parameter replacement. This causes false to be returned because each
  //    parameter is not used exactly once.
  //
  // Both of these cases are noted in the header file, and are unlikely to
  // occur in any actual legal message.
  base::string16 template_icu_escaped;
  base::ReplaceChars(template_icu, base::ASCIIToUTF16("$"),
                     base::ASCIIToUTF16("$$"), &template_icu_escaped);

  // Replace "{0}" -> "$1", "{1}" -> "$2", ... to prepare |template_dollars|
  // for ReplaceStringPlaceholders().
  base::string16 template_dollars =
      base::i18n::MessageFormatter::FormatWithNumberedArgs(
          template_icu_escaped, "$1", "$2", "$3", "$4", "$5", "$6", "$7");

  // FormatWithNumberedArgs() returns an empty string on failure.
  if (template_dollars.empty() && !template_icu.empty())
    return false;

  // Replace "$1", "$2", ... with the display text of each parameter.
  *out_message = base::ReplaceStringPlaceholders(template_dollars,
                                                 display_texts, out_offsets);

  // Each parameter must be used exactly once. If a parameter is unused or
  // used more than once then it can't be determined which |offsets| entry
  // corresponds to which parameter.
  return out_offsets->size() == display_texts.size();
}

// Parses |line| and sets |out|.
// Returns false on failure. |out| is not modified if false is returned.
bool ParseLegalMessageLine(const base::DictionaryValue& line,
                           SaveCardBubbleController::LegalMessageLine* out) {
  SaveCardBubbleController::LegalMessageLine result;

  // |display_texts| elements are the strings that will be substituted for
  // "{0}", "{1}", etc. in the template string.
  std::vector<base::string16> display_texts;

  // Process all the template parameters.
  const base::ListValue* template_parameters = nullptr;
  if (line.GetList("template_parameter", &template_parameters)) {
    display_texts.resize(template_parameters->GetSize());
    result.links.resize(template_parameters->GetSize());

    for (size_t parameter_index = 0;
         parameter_index < template_parameters->GetSize(); ++parameter_index) {
      const base::DictionaryValue* single_parameter;
      std::string url;
      if (!template_parameters->GetDictionary(parameter_index,
                                              &single_parameter) ||
          !single_parameter->GetString("display_text",
                                       &display_texts[parameter_index]) ||
          !single_parameter->GetString("url", &url)) {
        return false;
      }
      result.links[parameter_index].url = GURL(url);
    }
  }

  // Read the template string. It's a small subset of the ICU message format
  // syntax.
  base::string16 template_icu;
  if (!line.GetString("template", &template_icu))
    return false;

  // Replace the placeholders in |template_icu| with strings from
  // |display_texts|, and store the start position of each replacement in
  // |offsets|.
  std::vector<size_t> offsets;
  if (!ReplaceTemplatePlaceholders(template_icu, display_texts, &result.text,
                                   &offsets)) {
    return false;
  }

  // Fill in range values for all links.
  for (size_t offset_index = 0; offset_index < offsets.size(); ++offset_index) {
    size_t range_start = offsets[offset_index];
    result.links[offset_index].range = gfx::Range(
        range_start, range_start + display_texts[offset_index].size());
  }

  *out = result;
  return true;
}

}  // namespace

SaveCardBubbleControllerImpl::SaveCardBubbleControllerImpl(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      save_card_bubble_view_(nullptr) {
  DCHECK(web_contents);
}

SaveCardBubbleControllerImpl::~SaveCardBubbleControllerImpl() {
  if (save_card_bubble_view_)
    save_card_bubble_view_->Hide();
}

void SaveCardBubbleControllerImpl::ShowBubbleForLocalSave(
    const base::Closure& save_card_callback) {
  is_uploading_ = false;
  is_reshow_ = false;
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_uploading_,
      is_reshow_);

  save_card_callback_ = save_card_callback;
  ShowBubble();
}

void SaveCardBubbleControllerImpl::ShowBubbleForUpload(
    const base::Closure& save_card_callback,
    scoped_ptr<base::DictionaryValue> legal_message) {
  is_uploading_ = true;
  is_reshow_ = false;
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_uploading_,
      is_reshow_);

  const base::ListValue* lines = nullptr;
  if (legal_message->GetList("line", &lines)) {
    // Process all lines of the legal message. See comment in header file for
    // example of valid |legal_message| data.
    legal_message_lines_.resize(lines->GetSize());
    for (size_t i = 0; i < lines->GetSize(); ++i) {
      const base::DictionaryValue* single_line;
      if (!lines->GetDictionary(i, &single_line) ||
          !ParseLegalMessageLine(*single_line, &legal_message_lines_[i])) {
        lines = nullptr;
        break;
      }
    }
  }

  if (!lines) {
    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_INVALID_LEGAL_MESSAGE,
        is_uploading_, is_reshow_);

    legal_message_lines_.clear();
    return;
  }

  save_card_callback_ = save_card_callback;
  ShowBubble();
}

void SaveCardBubbleControllerImpl::ReshowBubble() {
  is_reshow_ = true;
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_SHOW_REQUESTED, is_uploading_,
      is_reshow_);

  ShowBubble();
}

bool SaveCardBubbleControllerImpl::IsIconVisible() const {
  return !save_card_callback_.is_null();
}

SaveCardBubbleView* SaveCardBubbleControllerImpl::save_card_bubble_view()
    const {
  return save_card_bubble_view_;
}

base::string16 SaveCardBubbleControllerImpl::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(
      is_uploading_ ? IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_TO_CLOUD
                    : IDS_AUTOFILL_SAVE_CARD_PROMPT_TITLE_LOCAL);
}

base::string16 SaveCardBubbleControllerImpl::GetExplanatoryMessage() const {
  return is_uploading_ ? l10n_util::GetStringUTF16(
                             IDS_AUTOFILL_SAVE_CARD_PROMPT_UPLOAD_EXPLANATION)
                       : base::string16();
}

void SaveCardBubbleControllerImpl::OnSaveButton() {
  save_card_callback_.Run();
  save_card_callback_.Reset();
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_END_ACCEPTED, is_uploading_,
      is_reshow_);
}

void SaveCardBubbleControllerImpl::OnCancelButton() {
  save_card_callback_.Reset();
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_END_DENIED, is_uploading_, is_reshow_);
}

void SaveCardBubbleControllerImpl::OnLearnMoreClicked() {
  OpenUrl(GURL(kHelpURL));
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEARN_MORE, is_uploading_,
      is_reshow_);
}

void SaveCardBubbleControllerImpl::OnLegalMessageLinkClicked(const GURL& url) {
  OpenUrl(url);
  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_DISMISS_CLICK_LEGAL_MESSAGE,
      is_uploading_, is_reshow_);
}

void SaveCardBubbleControllerImpl::OnBubbleClosed() {
  save_card_bubble_view_ = nullptr;
  UpdateIcon();
}

const SaveCardBubbleController::LegalMessageLines&
SaveCardBubbleControllerImpl::GetLegalMessageLines() const {
  return legal_message_lines_;
}

base::TimeDelta SaveCardBubbleControllerImpl::Elapsed() const {
  return timer_->Elapsed();
}

void SaveCardBubbleControllerImpl::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Nothing to do if there's no bubble available.
  if (save_card_callback_.is_null())
    return;

  // Don't react to in-page (fragment) navigations.
  if (details.is_in_page)
    return;

  // Don't do anything if a navigation occurs before a user could reasonably
  // interact with the bubble.
  if (Elapsed() < base::TimeDelta::FromSeconds(kSurviveNavigationSeconds))
    return;

  // Otherwise, get rid of the bubble and icon.
  save_card_callback_.Reset();
  if (save_card_bubble_view_) {
    save_card_bubble_view_->Hide();
    OnBubbleClosed();

    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_SHOWING, is_uploading_,
        is_reshow_);
  } else {
    UpdateIcon();

    AutofillMetrics::LogSaveCardPromptMetric(
        AutofillMetrics::SAVE_CARD_PROMPT_END_NAVIGATION_HIDDEN, is_uploading_,
        is_reshow_);
  }
}

void SaveCardBubbleControllerImpl::ShowBubble() {
  DCHECK(!save_card_callback_.is_null());
  DCHECK(!save_card_bubble_view_);

  // Need to create location bar icon before bubble, otherwise bubble will be
  // unanchored.
  UpdateIcon();

  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  save_card_bubble_view_ = browser->window()->ShowSaveCreditCardBubble(
      web_contents(), this, is_reshow_);
  DCHECK(save_card_bubble_view_);

  // Update icon after creating |save_card_bubble_view_| so that icon will show
  // its "toggled on" state.
  UpdateIcon();

  timer_.reset(new base::ElapsedTimer());

  AutofillMetrics::LogSaveCardPromptMetric(
      AutofillMetrics::SAVE_CARD_PROMPT_SHOWN, is_uploading_, is_reshow_);
}

void SaveCardBubbleControllerImpl::UpdateIcon() {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents());
  LocationBar* location_bar = browser->window()->GetLocationBar();
  location_bar->UpdateSaveCreditCardIcon();
}

void SaveCardBubbleControllerImpl::OpenUrl(const GURL& url) {
  web_contents()->OpenURL(
      content::OpenURLParams(url, content::Referrer(), NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_LINK, false));
}

}  // namespace autofill
