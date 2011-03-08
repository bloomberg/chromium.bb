// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/keyword_provider.h"

#include <algorithm>
#include <vector>

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/extensions/extension_omnibox_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "content/common/notification_details.h"
#include "content/common/notification_source.h"
#include "grit/generated_resources.h"
#include "net/base/escape.h"
#include "net/base/net_util.h"
#include "ui/base/l10n/l10n_util.h"

// Helper functor for Start(), for ending keyword mode unless explicitly told
// otherwise.
class KeywordProvider::ScopedEndExtensionKeywordMode {
 public:
  explicit ScopedEndExtensionKeywordMode(KeywordProvider* provider)
      : provider_(provider) { }
  ~ScopedEndExtensionKeywordMode() {
    if (provider_)
      provider_->MaybeEndExtensionKeywordMode();
  }

  void StayInKeywordMode() {
    provider_ = NULL;
  }
 private:
  KeywordProvider* provider_;
};

// static
string16 KeywordProvider::SplitReplacementStringFromInput(
    const string16& input,
    bool trim_leading_whitespace) {
  // The input may contain leading whitespace, strip it.
  string16 trimmed_input;
  TrimWhitespace(input, TRIM_LEADING, &trimmed_input);

  // And extract the replacement string.
  string16 remaining_input;
  SplitKeywordFromInput(trimmed_input, trim_leading_whitespace,
                        &remaining_input);
  return remaining_input;
}

KeywordProvider::KeywordProvider(ACProviderListener* listener, Profile* profile)
    : AutocompleteProvider(listener, profile, "Keyword"),
      model_(NULL),
      current_input_id_(0) {
  // Extension suggestions always come from the original profile, since that's
  // where extensions run. We use the input ID to distinguish whether the
  // suggestions are meant for us.
  registrar_.Add(this, NotificationType::EXTENSION_OMNIBOX_SUGGESTIONS_READY,
                 Source<Profile>(profile->GetOriginalProfile()));
  registrar_.Add(this,
                 NotificationType::EXTENSION_OMNIBOX_DEFAULT_SUGGESTION_CHANGED,
                 Source<Profile>(profile->GetOriginalProfile()));
  registrar_.Add(this, NotificationType::EXTENSION_OMNIBOX_INPUT_ENTERED,
                 Source<Profile>(profile));
}

KeywordProvider::KeywordProvider(ACProviderListener* listener,
                                 TemplateURLModel* model)
    : AutocompleteProvider(listener, NULL, "Keyword"),
      model_(model),
      current_input_id_(0) {
}


namespace {

// Helper functor for Start(), for sorting keyword matches by quality.
class CompareQuality {
 public:
  // A keyword is of higher quality when a greater fraction of it has been
  // typed, that is, when it is shorter.
  //
  // TODO(pkasting): http://b/740691 Most recent and most frequent keywords are
  // probably better rankings than the fraction of the keyword typed.  We should
  // always put any exact matches first no matter what, since the code in
  // Start() assumes this (and it makes sense).
  bool operator()(const string16& keyword1,
                  const string16& keyword2) const {
    return keyword1.length() < keyword2.length();
  }
};

// We need our input IDs to be unique across all profiles, so we keep a global
// UID that each provider uses.
static int global_input_uid_;

}  // namespace

// static
const TemplateURL* KeywordProvider::GetSubstitutingTemplateURLForInput(
    Profile* profile,
    const AutocompleteInput& input,
    string16* remaining_input) {
  if (!input.allow_exact_keyword_match())
    return NULL;

  string16 keyword;
  if (!ExtractKeywordFromInput(input, &keyword, remaining_input))
    return NULL;

  // Make sure the model is loaded. This is cheap and quickly bails out if
  // the model is already loaded.
  TemplateURLModel* model = profile->GetTemplateURLModel();
  DCHECK(model);
  model->Load();

  const TemplateURL* template_url = model->GetTemplateURLForKeyword(keyword);
  return TemplateURL::SupportsReplacement(template_url) ? template_url : NULL;
}

void KeywordProvider::Start(const AutocompleteInput& input,
                            bool minimal_changes) {
  // This object ensures we end keyword mode if we exit the function without
  // toggling keyword mode to on.
  ScopedEndExtensionKeywordMode keyword_mode_toggle(this);

  matches_.clear();

  if (!minimal_changes) {
    done_ = true;

    // Input has changed. Increment the input ID so that we can discard any
    // stale extension suggestions that may be incoming.
    current_input_id_ = ++global_input_uid_;
  }

  // Split user input into a keyword and some query input.
  //
  // We want to suggest keywords even when users have started typing URLs, on
  // the assumption that they might not realize they no longer need to go to a
  // site to be able to search it.  So we call CleanUserInputKeyword() to strip
  // any initial scheme and/or "www.".  NOTE: Any heuristics or UI used to
  // automatically/manually create keywords will need to be in sync with
  // whatever we do here!
  //
  // TODO(pkasting): http://b/1112681 If someday we remember usage frequency for
  // keywords, we might suggest keywords that haven't even been partially typed,
  // if the user uses them enough and isn't obviously typing something else.  In
  // this case we'd consider all input here to be query input.
  string16 keyword, remaining_input;
  if (!ExtractKeywordFromInput(input, &keyword, &remaining_input))
    return;

  // Make sure the model is loaded. This is cheap and quickly bails out if
  // the model is already loaded.
  TemplateURLModel* model = profile_ ? profile_->GetTemplateURLModel() : model_;
  DCHECK(model);
  model->Load();

  // Get the best matches for this keyword.
  //
  // NOTE: We could cache the previous keywords and reuse them here in the
  // |minimal_changes| case, but since we'd still have to recalculate their
  // relevances and we can just recreate the results synchronously anyway, we
  // don't bother.
  //
  // TODO(pkasting): http://b/893701 We should remember the user's use of a
  // search query both from the autocomplete popup and from web pages
  // themselves.
  std::vector<string16> keyword_matches;
  model->FindMatchingKeywords(keyword,
                              !remaining_input.empty(),
                              &keyword_matches);

  // Prune any extension keywords that are disallowed in incognito mode (if
  // we're incognito), or disabled.
  for (std::vector<string16>::iterator i(keyword_matches.begin());
       i != keyword_matches.end(); ) {
    const TemplateURL* template_url(model->GetTemplateURLForKeyword(*i));
    if (profile_ &&
        !input.synchronous_only() && template_url->IsExtensionKeyword()) {
      ExtensionService* service = profile_->GetExtensionService();
      const Extension* extension = service->GetExtensionById(
          template_url->GetExtensionId(), false);
      bool enabled = extension && (!profile_->IsOffTheRecord() ||
                                   service->IsIncognitoEnabled(extension));
      if (!enabled) {
        i = keyword_matches.erase(i);
        continue;
      }
    }
    ++i;
  }
  if (keyword_matches.empty())
    return;
  std::sort(keyword_matches.begin(), keyword_matches.end(), CompareQuality());

  // Limit to one exact or three inexact matches, and mark them up for display
  // in the autocomplete popup.
  // Any exact match is going to be the highest quality match, and thus at the
  // front of our vector.
  if (keyword_matches.front() == keyword) {
    const TemplateURL* template_url(model->GetTemplateURLForKeyword(keyword));
    // TODO(pkasting): We should probably check that if the user explicitly
    // typed a scheme, that scheme matches the one in |template_url|.
    matches_.push_back(CreateAutocompleteMatch(model, keyword, input,
                                               keyword.length(),
                                               remaining_input, -1));

    if (profile_ &&
        !input.synchronous_only() && template_url->IsExtensionKeyword()) {
      if (template_url->GetExtensionId() != current_keyword_extension_id_)
        MaybeEndExtensionKeywordMode();
      if (current_keyword_extension_id_.empty())
        EnterExtensionKeywordMode(template_url->GetExtensionId());
      keyword_mode_toggle.StayInKeywordMode();

      ApplyDefaultSuggestionForExtensionKeyword(profile_, template_url,
                                                remaining_input,
                                                &matches_[0]);

      if (minimal_changes) {
        // If the input hasn't significantly changed, we can just use the
        // suggestions from last time. We need to readjust the relevance to
        // ensure it is less than the main match's relevance.
        for (size_t i = 0; i < extension_suggest_matches_.size(); ++i) {
          matches_.push_back(extension_suggest_matches_[i]);
          matches_.back().relevance = matches_[0].relevance - (i + 1);
        }
      } else {
        extension_suggest_last_input_ = input;
        extension_suggest_matches_.clear();

        bool have_listeners = ExtensionOmniboxEventRouter::OnInputChanged(
            profile_, template_url->GetExtensionId(),
            UTF16ToUTF8(remaining_input), current_input_id_);

        // We only have to wait for suggest results if there are actually
        // extensions listening for input changes.
        if (have_listeners)
          done_ = false;
      }
    }
  } else {
    if (keyword_matches.size() > kMaxMatches) {
      keyword_matches.erase(keyword_matches.begin() + kMaxMatches,
                            keyword_matches.end());
    }
    for (std::vector<string16>::const_iterator i(keyword_matches.begin());
         i != keyword_matches.end(); ++i) {
      matches_.push_back(CreateAutocompleteMatch(model, *i,
                                                 input, keyword.length(),
                                                 remaining_input, -1));
    }
  }
}

void KeywordProvider::Stop() {
  done_ = true;
  MaybeEndExtensionKeywordMode();
}

KeywordProvider::~KeywordProvider() {}

// static
bool KeywordProvider::ExtractKeywordFromInput(const AutocompleteInput& input,
                                              string16* keyword,
                                              string16* remaining_input) {
  if ((input.type() == AutocompleteInput::INVALID) ||
      (input.type() == AutocompleteInput::FORCED_QUERY))
    return false;

  *keyword = TemplateURLModel::CleanUserInputKeyword(
      SplitKeywordFromInput(input.text(), true, remaining_input));
  return !keyword->empty();
}

// static
string16 KeywordProvider::SplitKeywordFromInput(
    const string16& input,
    bool trim_leading_whitespace,
    string16* remaining_input) {
  // Find end of first token.  The AutocompleteController has trimmed leading
  // whitespace, so we need not skip over that.
  const size_t first_white(input.find_first_of(kWhitespaceUTF16));
  DCHECK_NE(0U, first_white);
  if (first_white == string16::npos)
    return input;  // Only one token provided.

  // Set |remaining_input| to everything after the first token.
  DCHECK(remaining_input != NULL);
  const size_t remaining_start = trim_leading_whitespace ?
    input.find_first_not_of(kWhitespaceUTF16, first_white) : first_white + 1;

  if (remaining_start < input.length())
    remaining_input->assign(input.begin() + remaining_start, input.end());

  // Return first token as keyword.
  return input.substr(0, first_white);
}

// static
void KeywordProvider::FillInURLAndContents(
    const string16& remaining_input,
    const TemplateURL* element,
    AutocompleteMatch* match) {
  DCHECK(!element->short_name().empty());
  DCHECK(element->url());
  DCHECK(element->url()->IsValid());
  int message_id = element->IsExtensionKeyword() ?
      IDS_EXTENSION_KEYWORD_COMMAND : IDS_KEYWORD_SEARCH;
  if (remaining_input.empty()) {
    // Allow extension keyword providers to accept empty string input. This is
    // useful to allow extensions to do something in the case where no input is
    // entered.
    if (element->url()->SupportsReplacement() &&
        !element->IsExtensionKeyword()) {
      // No query input; return a generic, no-destination placeholder.
      match->contents.assign(
          l10n_util::GetStringFUTF16(message_id,
              element->AdjustedShortNameForLocaleDirection(),
              l10n_util::GetStringUTF16(IDS_EMPTY_KEYWORD_VALUE)));
      match->contents_class.push_back(
          ACMatchClassification(0, ACMatchClassification::DIM));
    } else {
      // Keyword that has no replacement text (aka a shorthand for a URL).
      match->destination_url = GURL(element->url()->url());
      match->contents.assign(element->short_name());
      AutocompleteMatch::ClassifyLocationInString(0, match->contents.length(),
          match->contents.length(), ACMatchClassification::NONE,
          &match->contents_class);
    }
  } else {
    // Create destination URL by escaping user input and substituting into
    // keyword template URL.  The escaping here handles whitespace in user
    // input, but we rely on later canonicalization functions to do more
    // fixup to make the URL valid if necessary.
    DCHECK(element->url()->SupportsReplacement());
    match->destination_url = GURL(element->url()->ReplaceSearchTerms(
        *element, remaining_input,
        TemplateURLRef::NO_SUGGESTIONS_AVAILABLE, string16()));
    std::vector<size_t> content_param_offsets;
    match->contents.assign(l10n_util::GetStringFUTF16(message_id,
                                                      element->short_name(),
                                                      remaining_input,
                                                      &content_param_offsets));
    if (content_param_offsets.size() == 2) {
      AutocompleteMatch::ClassifyLocationInString(content_param_offsets[1],
          remaining_input.length(), match->contents.length(),
          ACMatchClassification::NONE, &match->contents_class);
    } else {
      // See comments on an identical NOTREACHED() in search_provider.cc.
      NOTREACHED();
    }
  }
}

// static
int KeywordProvider::CalculateRelevance(AutocompleteInput::Type type,
                                        bool complete,
                                        bool supports_replacement,
                                        bool prefer_keyword,
                                        bool allow_exact_keyword_match) {
  if (!complete)
    return (type == AutocompleteInput::URL) ? 700 : 450;
  if (!supports_replacement || (allow_exact_keyword_match && prefer_keyword))
    return 1500;
  return (allow_exact_keyword_match && (type == AutocompleteInput::QUERY)) ?
      1450 : 1100;
}

AutocompleteMatch KeywordProvider::CreateAutocompleteMatch(
    TemplateURLModel* model,
    const string16& keyword,
    const AutocompleteInput& input,
    size_t prefix_length,
    const string16& remaining_input,
    int relevance) {
  DCHECK(model);
  // Get keyword data from data store.
  const TemplateURL* element(
      model->GetTemplateURLForKeyword(keyword));
  DCHECK(element && element->url());
  const bool supports_replacement = element->url()->SupportsReplacement();

  // Create an edit entry of "[keyword] [remaining input]".  This is helpful
  // even when [remaining input] is empty, as the user can select the popup
  // choice and immediately begin typing in query input.
  const bool keyword_complete = (prefix_length == keyword.length());
  if (relevance < 0) {
    relevance =
        CalculateRelevance(input.type(), keyword_complete,
                           // When the user wants keyword matches to take
                           // preference, score them highly regardless of
                           // whether the input provides query text.
                           supports_replacement, input.prefer_keyword(),
                           input.allow_exact_keyword_match());
  }
  AutocompleteMatch result(this, relevance, false,
      supports_replacement ? AutocompleteMatch::SEARCH_OTHER_ENGINE :
                             AutocompleteMatch::HISTORY_KEYWORD);
  result.fill_into_edit.assign(keyword);
  if (!remaining_input.empty() || !keyword_complete || supports_replacement)
    result.fill_into_edit.push_back(L' ');
  result.fill_into_edit.append(remaining_input);
  // If we wanted to set |result.inline_autocomplete_offset| correctly, we'd
  // need CleanUserInputKeyword() to return the amount of adjustment it's made
  // to the user's input.  Because right now inexact keyword matches can't score
  // more highly than a "what you typed" match from one of the other providers,
  // we just don't bother to do this, and leave inline autocompletion off.
  result.inline_autocomplete_offset = string16::npos;

  // Create destination URL and popup entry content by substituting user input
  // into keyword templates.
  FillInURLAndContents(remaining_input, element, &result);

  if (supports_replacement)
    result.template_url = element;
  result.transition = PageTransition::KEYWORD;

  // Create popup entry description based on the keyword name.
  if (!element->IsExtensionKeyword()) {
    result.description.assign(l10n_util::GetStringFUTF16(
        IDS_AUTOCOMPLETE_KEYWORD_DESCRIPTION, keyword));
    string16 keyword_desc(
        l10n_util::GetStringUTF16(IDS_AUTOCOMPLETE_KEYWORD_DESCRIPTION));
    AutocompleteMatch::ClassifyLocationInString(
        keyword_desc.find(ASCIIToUTF16("%s")),
        prefix_length,
        result.description.length(),
        ACMatchClassification::DIM,
        &result.description_class);
  }

  return result;
}

void KeywordProvider::Observe(NotificationType type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  TemplateURLModel* model = profile_ ? profile_->GetTemplateURLModel() : model_;
  const AutocompleteInput& input = extension_suggest_last_input_;

  switch (type.value) {
    case NotificationType::EXTENSION_OMNIBOX_INPUT_ENTERED:
      // Input has been accepted, so we're done with this input session. Ensure
      // we don't send the OnInputCancelled event.
      current_keyword_extension_id_.clear();
      return;

    case NotificationType::EXTENSION_OMNIBOX_DEFAULT_SUGGESTION_CHANGED: {
      // It's possible to change the default suggestion while not in an editing
      // session.
      string16 keyword, remaining_input;
      if (matches_.empty() || current_keyword_extension_id_.empty() ||
          !ExtractKeywordFromInput(input, &keyword, &remaining_input))
        return;

      const TemplateURL* template_url(
          model->GetTemplateURLForKeyword(keyword));
      ApplyDefaultSuggestionForExtensionKeyword(profile_, template_url,
                                                remaining_input,
                                                &matches_[0]);
      listener_->OnProviderUpdate(true);
      return;
    }

    case NotificationType::EXTENSION_OMNIBOX_SUGGESTIONS_READY: {
      const ExtensionOmniboxSuggestions& suggestions =
        *Details<ExtensionOmniboxSuggestions>(details).ptr();
      if (suggestions.request_id != current_input_id_)
        return;  // This is an old result. Just ignore.

      string16 keyword, remaining_input;
      if (!ExtractKeywordFromInput(input, &keyword, &remaining_input)) {
        NOTREACHED();
        return;
      }

      // TODO(mpcomplete): consider clamping the number of suggestions to
      // AutocompleteProvider::kMaxMatches.
      for (size_t i = 0; i < suggestions.suggestions.size(); ++i) {
        const ExtensionOmniboxSuggestion& suggestion =
            suggestions.suggestions[i];
        // We want to order these suggestions in descending order, so start with
        // the relevance of the first result (added synchronously in Start()),
        // and subtract 1 for each subsequent suggestion from the extension.
        // We know that |complete| is true, because we wouldn't get results from
        // the extension unless the full keyword had been typed.
        int first_relevance = CalculateRelevance(input.type(), true, true,
            input.prefer_keyword(), input.allow_exact_keyword_match());
        extension_suggest_matches_.push_back(CreateAutocompleteMatch(
            model, keyword, input, keyword.length(),
            suggestion.content, first_relevance - (i + 1)));

        AutocompleteMatch* match = &extension_suggest_matches_.back();
        match->contents.assign(suggestion.description);
        match->contents_class = suggestion.description_styles;
        match->description.clear();
        match->description_class.clear();
      }

      done_ = true;
      matches_.insert(matches_.end(), extension_suggest_matches_.begin(),
                      extension_suggest_matches_.end());
      listener_->OnProviderUpdate(!extension_suggest_matches_.empty());
      return;
    }

    default:
      NOTREACHED();
      return;
  }
}

void KeywordProvider::EnterExtensionKeywordMode(
    const std::string& extension_id) {
  DCHECK(current_keyword_extension_id_.empty());
  current_keyword_extension_id_ = extension_id;

  ExtensionOmniboxEventRouter::OnInputStarted(
      profile_, current_keyword_extension_id_);
}

void KeywordProvider::MaybeEndExtensionKeywordMode() {
  if (!current_keyword_extension_id_.empty()) {
    ExtensionOmniboxEventRouter::OnInputCancelled(
        profile_, current_keyword_extension_id_);

    current_keyword_extension_id_.clear();
  }
}
