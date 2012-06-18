// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/spelling_menu_observer.h"

#include "base/bind.h"
#include "base/i18n/case_conversion.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/spellchecker/spellcheck_factory.h"
#include "chrome/browser/spellchecker/spellcheck_host.h"
#include "chrome/browser/spellchecker/spellcheck_host_metrics.h"
#include "chrome/browser/spellchecker/spellcheck_platform_mac.h"
#include "chrome/browser/spellchecker/spelling_service_client.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/browser/tab_contents/spelling_bubble_model.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/spellcheck_messages.h"
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
      succeeded_(false),
      integrate_spelling_service_(false) {
}

SpellingMenuObserver::~SpellingMenuObserver() {
}

void SpellingMenuObserver::InitMenu(const content::ContextMenuParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Exit if we are not in an editable element because we add a menu item only
  // for editable elements.
  Profile* profile = proxy_->GetProfile();
  if (!params.is_editable || !profile)
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
  if (SpellingServiceClient::IsAvailable(profile,
                                         SpellingServiceClient::SUGGEST)) {
    // Retrieve the misspelled word to be sent to the Spelling service.
    string16 text = params.misspelled_word;
    if (!text.empty()) {
      // Initialize variables used in OnURLFetchComplete(). We copy the input
      // text to the result text so we can replace its misspelled regions with
      // suggestions.
      loading_frame_ = 0;
      succeeded_ = false;
      result_ = text;

      // Add a placeholder item. This item will be updated when we receive a
      // response from the Spelling service. (We do not have to disable this
      // item now since Chrome will call IsCommandIdEnabled() and disable it.)
      loading_message_ =
          l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_CHECKING);
      proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION,
                          loading_message_);

      // Invoke a JSON-RPC call to the Spelling service in the background so we
      // can update the placeholder item when we receive its response. It also
      // starts the animation timer so we can show animation until we receive
      // it.
      client_.reset(new SpellingServiceClient);
      bool result = client_->RequestTextCheck(
          profile, 0, SpellingServiceClient::SUGGEST, text,
          base::Bind(&SpellingMenuObserver::OnTextCheckComplete,
                     base::Unretained(this)));
      if (result) {
        animation_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1),
            this, &SpellingMenuObserver::OnAnimationTimerExpired);
      }
    }
  }

  if (!params.dictionary_suggestions.empty()) {
    proxy_->AddSeparator();

    // |spellcheck_host| can be null when the suggested word is
    // provided by Web SpellCheck API.
    SpellCheckHost* spellcheck_host =
        SpellCheckFactory::GetHostForProfile(profile);
    if (spellcheck_host && spellcheck_host->GetMetrics())
      spellcheck_host->GetMetrics()->RecordSuggestionStats(1);
  }

  // If word is misspelled, give option for "Add to dictionary" and a check item
  // "Ask Google for suggestions".
  integrate_spelling_service_ =
      profile->GetPrefs()->GetBoolean(prefs::kSpellCheckUseSpellingService);
  if (!params.misspelled_word.empty()) {
    if (params.dictionary_suggestions.empty()) {
      proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS,
          l10n_util::GetStringUTF16(
              IDS_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS));
    }
    misspelled_word_ = params.misspelled_word;
    proxy_->AddMenuItem(IDC_SPELLCHECK_ADD_TO_DICTIONARY,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_ADD_TO_DICTIONARY));

    proxy_->AddCheckItem(IDC_CONTENT_CONTEXT_SPELLING_TOGGLE,
        l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_ASK_GOOGLE));

    proxy_->AddSeparator();
  }
}

bool SpellingMenuObserver::IsCommandIdSupported(int command_id) {
  if (command_id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      command_id <= IDC_SPELLCHECK_SUGGESTION_4)
    return true;

  switch (command_id) {
    case IDC_SPELLCHECK_ADD_TO_DICTIONARY:
    case IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS:
    case IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION:
    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
      return true;

    default:
      return false;
  }
  return false;
}

bool SpellingMenuObserver::IsCommandIdChecked(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_TOGGLE)
    return integrate_spelling_service_;
  return false;
}

bool SpellingMenuObserver::IsCommandIdEnabled(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      command_id <= IDC_SPELLCHECK_SUGGESTION_4)
    return true;

  switch (command_id) {
    case IDC_SPELLCHECK_ADD_TO_DICTIONARY:
      return !misspelled_word_.empty();

    case IDC_CONTENT_CONTEXT_NO_SPELLING_SUGGESTIONS:
      return false;

    case IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION:
      return succeeded_;

    case IDC_CONTENT_CONTEXT_SPELLING_TOGGLE:
      return true;

    default:
      return false;
  }
  return false;
}

void SpellingMenuObserver::ExecuteCommand(int command_id) {
  DCHECK(IsCommandIdSupported(command_id));

  if (command_id >= IDC_SPELLCHECK_SUGGESTION_0 &&
      command_id <= IDC_SPELLCHECK_SUGGESTION_4) {
    content::RenderViewHost* host = proxy_->GetRenderViewHost();
    host->Send(new SpellCheckMsg_Replace(
        host->GetRoutingID(),
        suggestions_[command_id - IDC_SPELLCHECK_SUGGESTION_0]));
    // GetSpellCheckHost() can return null when the suggested word is
    // provided by Web SpellCheck API.
    Profile* profile = proxy_->GetProfile();
    if (profile) {
      SpellCheckHost* spellcheck_host =
          SpellCheckFactory::GetHostForProfile(profile);
      if (spellcheck_host && spellcheck_host->GetMetrics())
        spellcheck_host->GetMetrics()->RecordReplacedWordStats(1);
    }
    return;
  }

  // When we choose the suggestion sent from the Spelling service, we replace
  // the misspelled word with the suggestion and add it to our custom-word
  // dictionary so this word is not marked as misspelled any longer.
  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION) {
    content::RenderViewHost* host = proxy_->GetRenderViewHost();
    host->Send(new SpellCheckMsg_Replace(host->GetRoutingID(), result_));
    misspelled_word_ = result_;
  }

  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION ||
      command_id == IDC_SPELLCHECK_ADD_TO_DICTIONARY) {
    // GetHostForProfile() can return null when the suggested word is
    // provided by Web SpellCheck API.
    Profile* profile = proxy_->GetProfile();
    if (profile) {
      SpellCheckHost* host = SpellCheckFactory::GetHostForProfile(profile);
      if (host)
        host->AddWord(UTF16ToUTF8(misspelled_word_));
    }
#if defined(OS_MACOSX)
    spellcheck_mac::AddWord(misspelled_word_);
#endif
  }

  if (command_id == IDC_CONTENT_CONTEXT_SPELLING_TOGGLE) {
    // When a user enables the "Ask Google for spelling suggestions" item, we
    // show a bubble to confirm it. On the other hand, when a user disables this
    // item, we directly update/ the profile and stop integrating the spelling
    // service immediately.
    if (!integrate_spelling_service_) {
      content::RenderViewHost* rvh = proxy_->GetRenderViewHost();
      gfx::Rect rect = rvh->GetView()->GetViewBounds();
      browser::ShowConfirmBubble(rvh->GetView()->GetNativeView(),
                                 gfx::Point(rect.CenterPoint().x(), rect.y()),
                                 new SpellingBubbleModel(proxy_->GetProfile()));
    } else {
      Profile* profile = proxy_->GetProfile();
      if (profile)
        profile->GetPrefs()->SetBoolean(prefs::kSpellCheckUseSpellingService,
                                        false);
    }
  }
}

void SpellingMenuObserver::OnTextCheckComplete(
    int tag,
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
  if (!succeeded_) {
    result_ = l10n_util::GetStringUTF16(
        IDS_CONTENT_CONTEXT_SPELLING_NO_SUGGESTIONS_FROM_GOOGLE);
  }

  // Update the menu item with the result text. We disable this item and hide it
  // when the spelling service does not provide valid suggestions.
  proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, succeeded_,
                         false, result_);
}

void SpellingMenuObserver::OnAnimationTimerExpired() {
  // Append '.' characters to the end of "Checking".
  loading_frame_ = (loading_frame_ + 1) & 3;
  string16 loading_message = loading_message_;
  for (int i = 0; i < loading_frame_; ++i)
    loading_message.push_back('.');

  // Update the menu item with the text. We disable this item to prevent users
  // from selecting it.
  proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, false, false,
                         loading_message);
}
