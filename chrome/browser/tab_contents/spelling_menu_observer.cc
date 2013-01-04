// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/spelling_menu_observer.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/i18n/case_conversion.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_service.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_platform_mac.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/tab_contents/spelling_bubble_model.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_result.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/context_menu_params.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"

using content::BrowserThread;

SpellingMenuObserver::SpellingMenuObserver(RenderViewContextMenuProxy* proxy)
    : proxy_(proxy),
      loading_frame_(0),
      succeeded_(false) {
  if (proxy && proxy->GetProfile()) {
    integrate_spelling_service_.Init(prefs::kSpellCheckUseSpellingService,
                                     proxy->GetProfile()->GetPrefs());
    autocorrect_spelling_.Init(prefs::kEnableAutoSpellCorrect,
                               proxy->GetProfile()->GetPrefs());
  }
}

SpellingMenuObserver::~SpellingMenuObserver() {
}

void SpellingMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!params.misspelled_word.empty() ||
      params.dictionary_suggestions.empty());

  // Exit if we are not in an editable element because we add a menu item only
  // for editable elements.
  Profile* profile = proxy_->GetProfile();
  if (!params.is_editable || !profile)
    return;

  // Exit if there is no misspelled word.
  if (params.misspelled_word.empty())
    return;

  // Append Dictionary spell check suggestions.
  suggestions_ = params.dictionary_suggestions;
  for (size_t i = 0; i < params.dictionary_suggestions.size() &&
       IDC_SPELLCHECK_SUGGESTION_0 + i <= IDC_SPELLCHECK_SUGGESTION_LAST;
       ++i) {
    proxy_->AddMenuItem(IDC_SPELLCHECK_SUGGESTION_0 + static_cast<int>(i),
                        params.dictionary_suggestions[i]);
  }

  // Append a placeholder item for the suggestion from the Spelling serivce and
  // send a request to the service if we can retrieve suggestions from it.
  // Also, see if we can use the spelling service to get an ideal suggestion.
  // Otherwise, we'll fall back to the set of suggestions.
  bool useSpellingService = SpellingServiceClient::IsAvailable(
      profile, SpellingServiceClient::SPELLCHECK);
  bool useSuggestions = SpellingServiceClient::IsAvailable(
      profile, SpellingServiceClient::SUGGEST);
  if (useSuggestions || useSpellingService) {
    // Initialize variables used in OnTextCheckComplete(). We copy the input
    // text to the result text so we can replace its misspelled regions with
    // suggestions.
    succeeded_ = false;
    result_ = params.misspelled_word;

    // Add a placeholder item. This item will be updated when we receive a
    // response from the Spelling service. (We do not have to disable this
    // item now since Chrome will call IsCommandIdEnabled() and disable it.)
    loading_message_ =
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_CHECKING);
    if (!useSpellingService) {
      proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION,
                          loading_message_);
    }
    // Invoke a JSON-RPC call to the Spelling service in the background so we
    // can update the placeholder item when we receive its response. It also
    // starts the animation timer so we can show animation until we receive
    // it.
    SpellingServiceClient::ServiceType type = SpellingServiceClient::SUGGEST;
    if (useSpellingService)
      type = SpellingServiceClient::SPELLCHECK;
    client_.reset(new SpellingServiceClient);
    bool result = client_->RequestTextCheck(
        profile, type, params.misspelled_word,
        base::Bind(&SpellingMenuObserver::OnTextCheckComplete,
                   base::Unretained(this), type));
    if (result) {
      loading_frame_ = 0;
      animation_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1),
          this, &SpellingMenuObserver::OnAnimationTimerExpired);
    }
  }

  if (!params.dictionary_suggestions.empty()) {
    proxy_->AddSeparator();

    // |spellcheck_service| can be null when the suggested word is
    // provided by Web SpellCheck API.
    SpellcheckService* spellcheck_service =
        SpellcheckServiceFactory::GetForProfile(profile);
    if (spellcheck_service && spellcheck_service->GetMetrics())
      spellcheck_service->GetMetrics()->RecordSuggestionStats(1);
  }

  // If word is misspelled, give option for "Add to dictionary" and a check item
  // "Ask Google for suggestions".
  if (params.dictionary_suggestions.empty()) {
    proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS,
        l10n_util::GetStringUTF16(
            IDS_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS));
    if (useSuggestions || useSpellingService)
      proxy_->AddSeparator();
  }
  misspelled_word_ = params.misspelled_word;
  proxy_->AddMenuItem(IDC_SPELLCHECK_ADD_TO_DICTIONARY,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY));

  proxy_->AddCheckItem(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE,
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE));

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSpellingAutoCorrect)) {
    proxy_->AddCheckItem(IDC_CONTENT_CONTEXT_AUTOCORRECT_SPELLING_TOGGLE,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_AUTOCORRECT));
  }

  proxy_->AddSeparator();
}

bool SpellingMenuObserver::IsCommandIdSupported(int command_id) {
  if (command_id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      command_id <= IDC_SPELLCHECK_SUGGESTION_LAST)
    return true;

  switch (command_id) {
    case IDC_SPELLCHECK_ADD_TO_DICTIONARY:
    case IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS:
    case IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION:
    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
    case IDC_CONTENT_CONTEXT_AUTOCORRECT_SPELLING_TOGGLE:
      return true;

    default:
      return false;
  }
  return false;
}

bool SpellingMenuObserver::IsCommandIdChecked(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_TOGGLE)
    return integrate_spelling_service_.GetValue();
  else if (command_id == IDC_CONTENT_CONTEXT_AUTOCORRECT_SPELLING_TOGGLE)
    return autocorrect_spelling_.GetValue();
  return false;
}

bool SpellingMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      command_id <= IDC_SPELLCHECK_SUGGESTION_LAST)
    return true;

  switch (command_id) {
    case IDC_SPELLCHECK_ADD_TO_DICTIONARY:
      return !misspelled_word_.empty();

    case IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS:
      return false;

    case IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION:
      return succeeded_;

    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
      return integrate_spelling_service_.IsUserModifiable();

    case IDC_CONTENT_CONTEXT_AUTOCORRECT_SPELLING_TOGGLE:
      return integrate_spelling_service_.IsUserModifiable();

    default:
      return false;
  }
  return false;
}

void SpellingMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      command_id <= IDC_SPELLCHECK_SUGGESTION_LAST) {
    proxy_->GetRenderViewHost()->Replace(
        suggestions_[command_id - IDC_SPELLCHECK_SUGGESTION_0]);
    // GetSpellCheckHost() can return null when the suggested word is
    // provided by Web SpellCheck API.
    Profile* profile = proxy_->GetProfile();
    if (profile) {
      SpellcheckService* spellcheck_service =
          SpellcheckServiceFactory::GetForProfile(profile);
      if (spellcheck_service && spellcheck_service->GetMetrics())
        spellcheck_service->GetMetrics()->RecordReplacedWordStats(1);
    }
    return;
  }

  // When we choose the suggestion sent from the Spelling service, we replace
  // the misspelled word with the suggestion and add it to our custom-word
  // dictionary so this word is not marked as misspelled any longer.
  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION) {
    proxy_->GetRenderViewHost()->Replace(result_);
    misspelled_word_ = result_;
  }

  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION ||
      command_id == IDC_SPELLCHECK_ADD_TO_DICTIONARY) {
    // GetHostForProfile() can return null when the suggested word is
    // provided by Web SpellCheck API.
    Profile* profile = proxy_->GetProfile();
    if (profile) {
      SpellcheckService* spellcheck_service =
            SpellcheckServiceFactory::GetForProfile(profile);
      if (spellcheck_service)
        spellcheck_service->GetCustomDictionary()->AddWord(
            UTF16ToUTF8(misspelled_word_));
    }
#if defined(OS_MACOSX)
    spellcheck_mac::AddWord(misspelled_word_);
#endif
  }

  // The spelling service can be toggled by the user only if it is not managed.
  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_TOGGLE &&
      integrate_spelling_service_.IsUserModifiable()) {
    // When a user enables the "Ask Google for spelling suggestions" item, we
    // show a bubble to confirm it. On the other hand, when a user disables this
    // item, we directly update/ the profile and stop integrating the spelling
    // service immediately.
    if (!integrate_spelling_service_.GetValue()) {
      content::RenderViewHost* rvh = proxy_->GetRenderViewHost();
      gfx::Rect rect = rvh->GetView()->GetViewBounds();
      chrome::ShowConfirmBubble(rvh->GetView()->GetNativeView(),
                                gfx::Point(rect.CenterPoint().x(), rect.y()),
                                new SpellingBubbleModel(
                                    proxy_->GetProfile(),
                                    proxy_->GetWebContents(),
                                    false));
    } else {
      Profile* profile = proxy_->GetProfile();
      if (profile)
        profile->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService,
                                        false);
        profile->GetPrefs()->SetBoolean(prefs::kEnableAutoSpellCorrect,
                                        false);
    }
  }
  // Autocorrect requires use of the spelling service and the spelling service
  // can be toggled by the user only if it is not managed.
  if (command_id == IDC_CONTENT_CONTEXT_AUTOCORRECT_SPELLING_TOGGLE &&
      integrate_spelling_service_.IsUserModifiable()) {
    // When the user enables autocorrect, we'll need to make sure that we can
    // ask Google for suggestions since that service is required. So we show
    // the bubble and just make sure to enable autocorrect as well.
    if (!integrate_spelling_service_.GetValue()) {
      content::RenderViewHost* rvh = proxy_->GetRenderViewHost();
      gfx::Rect rect = rvh->GetView()->GetViewBounds();
      chrome::ShowConfirmBubble(rvh->GetView()->GetNativeView(),
                                gfx::Point(rect.CenterPoint().x(), rect.y()),
                                new SpellingBubbleModel(
                                    proxy_->GetProfile(),
                                    proxy_->GetWebContents(),
                                    true));
    } else {
      Profile* profile = proxy_->GetProfile();
      if (profile) {
        bool current_value = autocorrect_spelling_.GetValue();
        profile->GetPrefs()->SetBoolean(prefs::kEnableAutoSpellCorrect,
                                        !current_value);
      }
    }
  }
}

void SpellingMenuObserver::OnTextCheckComplete(
    SpellingServiceClient::ServiceType type,
    bool success,
    const string16& text,
    const std::vector<SpellCheckResult>& results) {
  animation_timer_.Stop();

  // Scan the text-check results and replace the misspelled regions with
  // suggested words. If the replaced text is included in the suggestion list
  // provided by the local spellchecker, we show a "No suggestions from Google"
  // message.
  succeeded_ = success;
  if (results.empty()) {
    succeeded_ = false;
  } else {
    typedef std::vector<SpellCheckResult> SpellCheckResults;
    for (SpellCheckResults::const_iterator it = results.begin();
         it != results.end(); ++it) {
      result_.replace(it->location, it->length, it->replacement);
    }
    string16 result = base::i18n::ToLower(result_);
    for (std::vector<string16>::const_iterator it = suggestions_.begin();
         it != suggestions_.end(); ++it) {
      if (result == base::i18n::ToLower(*it)) {
        succeeded_ = false;
        break;
      }
    }
  }
  if (type != SpellingServiceClient::SPELLCHECK) {
    if (!succeeded_) {
      result_ = l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_SPELLING_NO_SUGGESTIONS_FROM_GOOGLE);
    }

    // Update the menu item with the result text. We disable this item and hide
    // it when the spelling service does not provide valid suggestions.
    proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, succeeded_,
                           false, result_);
  }
}

void SpellingMenuObserver::OnAnimationTimerExpired() {
  // Append '.' characters to the end of "Checking".
  loading_frame_ = (loading_frame_ + 1) & 3;
  string16 loading_message = loading_message_ + string16(loading_frame_,'.');

  // Update the menu item with the text. We disable this item to prevent users
  // from selecting it.
  proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, false, false,
                         loading_message);
}
