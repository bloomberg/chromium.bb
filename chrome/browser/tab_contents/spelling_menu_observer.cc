// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/spelling_menu_observer.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/string_escape.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/render_view_context_menu.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "unicode/uloc.h"
#include "webkit/glue/context_menu.h"

#if defined(GOOGLE_CHROME_BUILD)
#include "chrome/browser/spellchecker/internal/spellcheck_internal.h"
#else
// Use a dummy URL and a key on Chromium to avoid build breaks until the
// Spelling API is released. These dummy parameters just cause a timeout and
// show 'no suggestions found'.
#define SPELLING_SERVICE_KEY
#define SPELLING_SERVICE_URL "http://127.0.0.1/rpc"
#endif

SpellingMenuObserver::SpellingMenuObserver(RenderViewContextMenuProxy* proxy)
    : proxy_(proxy),
      loading_frame_(0),
      succeeded_(false) {
}

SpellingMenuObserver::~SpellingMenuObserver() {
}

void SpellingMenuObserver::InitMenu(const ContextMenuParams& params) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Exit if we are not in an editable element because we add a menu item only
  // for editable elements.
  if (!params.is_editable)
    return;

  Profile* profile = proxy_->GetProfile();
  if (!profile || !profile->GetRequestContext())
    return;

  // Retrieve the misspelled word to be sent to the Spelling service.
  string16 text = params.misspelled_word;
  if (text.empty())
    return;

  // Initialize variables used in OnURLFetchComplete(). We copy the input text
  // to the result text so we can replace its misspelled regions with
  // suggestions.
  loading_frame_ = 0;
  succeeded_ = false;
  result_ = text;

  // Add a placeholder item. This item will be updated when we receive a
  // response from the Spelling service. (We do not have to disable this item
  // now since Chrome will call IsCommandIdEnabled() and disable it.)
  loading_message_ =
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_CHECKING);
  proxy_->AddMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION,
                      loading_message_);

  // Invoke a JSON-RPC call to the Spelling service in the background so we can
  // update the placeholder item when we receive its response. It also starts
  // the animation timer so we can show animation until we receive it.
  const PrefService* pref = profile->GetPrefs();
  std::string language =
      pref ? pref->GetString(prefs::kSpellCheckDictionary) : "en-US";
  Invoke(text, language, profile->GetRequestContext());
}

bool SpellingMenuObserver::IsCommandIdSupported(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION;
}

bool SpellingMenuObserver::IsCommandIdEnabled(int command_id) {
  return command_id == IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION && succeeded_;
}

void SpellingMenuObserver::ExecuteCommand(int command_id) {
  if (IsCommandIdEnabled(command_id))
    proxy_->GetRenderViewHost()->Replace(result_);
}

bool SpellingMenuObserver::Invoke(const string16& text,
                                  const std::string& locale,
                                  net::URLRequestContextGetter* context) {
  // Create the parameters needed by Spelling API. Spelling API needs three
  // parameters: ISO language code, ISO3 country code, and text to be checked by
  // the service. On the other hand, Chrome uses an ISO locale ID and it may
  // not include a country ID, e.g. "fr", "de", etc. To create the input
  // parameters, we convert the UI locale to a full locale ID, and  convert the
  // full locale ID to an ISO language code and and ISO3 country code. Also, we
  // convert the given text to a JSON string, i.e. quote all its non-ASCII
  // characters.
  UErrorCode error = U_ZERO_ERROR;
  char id[ULOC_LANG_CAPACITY + ULOC_SCRIPT_CAPACITY + ULOC_COUNTRY_CAPACITY];
  uloc_addLikelySubtags(locale.c_str(), id, arraysize(id), &error);

  error = U_ZERO_ERROR;
  char language[ULOC_LANG_CAPACITY];
  uloc_getLanguage(id, language, arraysize(language), &error);

  const char* country = uloc_getISO3Country(id);

  std::string encoded_text;
  base::JsonDoubleQuote(text, false, &encoded_text);

  // Format the JSON request to be sent to the Spelling service.
  static const char kSpellingRequest[] =
      "{"
      "\"method\":\"spelling.check\","
      "\"apiVersion\":\"v1\","
      "\"params\":{"
      "\"text\":\"%s\","
      "\"language\":\"%s\","
      "\"origin_country\":\"%s\","
      "\"key\":\"" SPELLING_SERVICE_KEY "\""
      "}"
      "}";
  std::string request = base::StringPrintf(kSpellingRequest,
                                           encoded_text.c_str(),
                                           language, country);

  static const char kSpellingServiceURL[] = SPELLING_SERVICE_URL;
  GURL url = GURL(kSpellingServiceURL);
  fetcher_.reset(new URLFetcher(url, URLFetcher::POST, this));
  fetcher_->set_request_context(context);
  fetcher_->set_upload_data("application/json", request);
  fetcher_->Start();

  animation_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1),
      this, &SpellingMenuObserver::OnAnimationTimerExpired);

  return true;
}

void SpellingMenuObserver::OnURLFetchComplete(
    const URLFetcher* source,
    const GURL& url,
    const net::URLRequestStatus& status,
    int response,
    const net::ResponseCookies& cookies,
    const std::string& data) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  fetcher_.reset();
  animation_timer_.Stop();

  // Parse the response JSON and replace misspelled words in the |result_| text
  // with their suggestions.
  succeeded_ = ParseResponse(response, data);
  if (!succeeded_)
    result_ = l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_SPELLING_CORRECT);

  // Update the menu item with the result text. We enable this item only when
  // the request text has misspelled words. (We disable this item not only when
  // we receive a server error but also when the input text consists only of
  // well-spelled words. For either case, we do not need to replace the input
  // text.)
  proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, succeeded_,
                         result_);
}

bool SpellingMenuObserver::ParseResponse(int response,
                                         const std::string& data) {
  // When this JSON-RPC call finishes successfully, the Spelling service returns
  // an JSON object listed below.
  // * result - an envelope object representing the result from the APIARY
  //   server, which is the JSON-API front-end for the Spelling service. This
  //   object consists of the following variable:
  //   - spellingCheckResponse (SpellingCheckResponse).
  // * SpellingCheckResponse - an object representing the result from the
  //   Spelling service. This object consists of the following variable:
  //   - misspellings (optional array of Misspelling)
  // * Misspelling - an object representing a misspelling region and its
  //   suggestions. This object consists of the following variables:
  //   - charStart (number) - the beginning of the misspelled region;
  //   - charLength (number) - the length of the misspelled region;
  //   - suggestions (array of string) - the suggestions for the misspelling
  //     text, and;
  //   - canAutoCorrect (optional boolean) - whether we can use the first
  //     suggestion for auto-correction.
  // For example, the Spelling service returns the following JSON when we send a
  // spelling request for "duck goes quisk" as of 16 August, 2011.
  // {
  //   "result": {
  //   "spellingCheckResponse": {
  //     "misspellings": [{
  //         "charStart": 10,
  //         "charLength": 5,
  //         "suggestions": [{ "suggestion": "quack" }],
  //         "canAutoCorrect": false
  //     }]
  //   }
  // }

  // When a server error happened in the APIARY server (including when we cannot
  // get any responses from the server), it returns an HTTP error.
  if ((response / 100) != 2)
    return false;

  scoped_ptr<DictionaryValue> value(
      static_cast<DictionaryValue*>(base::JSONReader::Read(data, true)));
  if (!value.get() || !value->IsType(base::Value::TYPE_DICTIONARY))
    return false;

  // Retrieve the array of Misspelling objects. When the APIARY service failed
  // calling the Spelling service, it returns a JSON representing the service
  // error. (In this case, its HTTP status is 200.) We just return false for
  // this case.
  ListValue* misspellings = NULL;
  const char kMisspellings[] = "result.spellingCheckResponse.misspellings";
  if (!value->GetList(kMisspellings, &misspellings))
    return false;

  // For each misspelled region, we replace it with its first suggestion.
  for (size_t i = 0; i < misspellings->GetSize(); ++i) {
    // Retrieve the i-th misspelling region, which represents misspelling text
    // in the request text and its alternative.
    DictionaryValue* misspelling = NULL;
    if (!misspellings->GetDictionary(i, &misspelling))
      return false;

    int start = 0;
    int length = 0;
    ListValue* suggestions = NULL;
    if (!misspelling->GetInteger("charStart", &start) ||
        !misspelling->GetInteger("charLength", &length) ||
        !misspelling->GetList("suggestions", &suggestions)) {
      return false;
    }

    // Retrieve the alternative text and replace the misspelling region with the
    // alternative.
    DictionaryValue* suggestion = NULL;
    string16 text;
    if (!suggestions->GetDictionary(0, &suggestion) ||
        !suggestion->GetString("suggestion", &text)) {
      return false;
    }
    result_.replace(start, length, text);
  }

  return true;
}

void SpellingMenuObserver::OnAnimationTimerExpired() {
  if (!fetcher_.get())
    return;

  // Append '.' characters to the end of "Checking".
  loading_frame_ = (loading_frame_ + 1) & 3;
  string16 loading_message = loading_message_;
  for (int i = 0; i < loading_frame_; ++i)
    loading_message.push_back('.');

  // Update the menu item with the text. We disable this item to prevent users
  // from selecting it.
  proxy_->UpdateMenuItem(IDC_CONTENT_CONTEXT_SPELLING_SUGGESTION, false,
                         loading_message);
}
