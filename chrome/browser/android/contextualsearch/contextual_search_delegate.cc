// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"

#include <algorithm>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/contextualsearch/resolved_search_term.h"
#include "chrome/browser/android/proto/client_discourse_context.pb.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/translate/translate_service.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/browser/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/template_url_service.h"
#include "components/variations/net/variations_http_headers.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "net/base/escape.h"
#include "net/url_request/url_fetcher.h"
#include "url/gurl.h"

using content::ContentViewCore;

namespace {

const char kContextualSearchFieldTrialName[] = "ContextualSearch";
const char kContextualSearchSurroundingSizeParamName[] = "surrounding_size";
const char kContextualSearchIcingSurroundingSizeParamName[] =
    "icing_surrounding_size";
const char kContextualSearchResolverURLParamName[] = "resolver_url";
const char kContextualSearchDoNotSendURLParamName[] = "do_not_send_url";
const char kContextualSearchResponseDisplayTextParam[] = "display_text";
const char kContextualSearchResponseSelectedTextParam[] = "selected_text";
const char kContextualSearchResponseSearchTermParam[] = "search_term";
const char kContextualSearchResponseLanguageParam[] = "lang";
const char kContextualSearchResponseResolvedTermParam[] = "resolved_term";
const char kContextualSearchPreventPreload[] = "prevent_preload";
const char kContextualSearchMentions[] = "mentions";
const char kContextualSearchServerEndpoint[] = "_/contextualsearch?";
const int kContextualSearchRequestVersion = 2;
const char kContextualSearchResolverUrl[] =
    "contextual-search-resolver-url";
// The default size of the content surrounding the selection to gather, allowing
// room for other parameters.
const int kContextualSearchDefaultContentSize = 1536;
const int kContextualSearchDefaultIcingSurroundingSize = 400;
const int kContextualSearchMaxSelection = 100;
// The maximum length of a URL to build.
const int kMaxURLSize = 2048;
const char kXssiEscape[] = ")]}'\n";
const char kDiscourseContextHeaderPrefix[] = "X-Additional-Discourse-Context: ";
const char kDoPreventPreloadValue[] = "1";

// The number of characters that should be shown after the selected expression.
const int kSurroundingSizeForUI = 60;

} // namespace

// URLFetcher ID, only used for tests: we only have one kind of fetcher.
const int ContextualSearchDelegate::kContextualSearchURLFetcherID = 1;

// Handles tasks for the ContextualSearchManager in a separable, testable way.
ContextualSearchDelegate::ContextualSearchDelegate(
    net::URLRequestContextGetter* url_request_context,
    TemplateURLService* template_url_service,
    const ContextualSearchDelegate::SearchTermResolutionCallback&
        search_term_callback,
    const ContextualSearchDelegate::SurroundingTextCallback&
        surrounding_callback,
    const ContextualSearchDelegate::IcingCallback& icing_callback)
    : url_request_context_(url_request_context),
      template_url_service_(template_url_service),
      search_term_callback_(search_term_callback),
      surrounding_callback_(surrounding_callback),
      icing_callback_(icing_callback) {
}

ContextualSearchDelegate::~ContextualSearchDelegate() {
}

void ContextualSearchDelegate::StartSearchTermResolutionRequest(
    const std::string& selection,
    bool use_resolved_search_term,
    content::ContentViewCore* content_view_core,
    bool may_send_base_page_url) {
  GatherSurroundingTextWithCallback(
      selection, use_resolved_search_term, content_view_core,
      may_send_base_page_url,
      base::Bind(&ContextualSearchDelegate::StartSearchTermRequestFromSelection,
                 AsWeakPtr()));
}

void ContextualSearchDelegate::GatherAndSaveSurroundingText(
    const std::string& selection,
    bool use_resolved_search_term,
    content::ContentViewCore* content_view_core,
    bool may_send_base_page_url) {
  GatherSurroundingTextWithCallback(
      selection, use_resolved_search_term, content_view_core,
      may_send_base_page_url,
      base::Bind(&ContextualSearchDelegate::SaveSurroundingText, AsWeakPtr()));
  // TODO(donnd): clear the context here, since we're done with it (but risky).
}

void ContextualSearchDelegate::ContinueSearchTermResolutionRequest() {
  DCHECK(context_.get());
  if (!context_.get())
    return;
  GURL request_url(BuildRequestUrl());
  DCHECK(request_url.is_valid());

  // Reset will delete any previous fetcher, and we won't get any callback.
  search_term_fetcher_.reset(
      net::URLFetcher::Create(kContextualSearchURLFetcherID, request_url,
                              net::URLFetcher::GET, this).release());
  search_term_fetcher_->SetRequestContext(url_request_context_);

  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  variations::AppendVariationHeaders(
      search_term_fetcher_->GetOriginalURL(),
      false,  // Impossible to be incognito at this point.
      false, &headers);
  search_term_fetcher_->SetExtraRequestHeaders(headers.ToString());

  SetDiscourseContextAndAddToHeader(*context_);

  search_term_fetcher_->Start();
}

void ContextualSearchDelegate::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(source == search_term_fetcher_.get());
  int response_code = source->GetResponseCode();
  std::string search_term;
  std::string display_text;
  std::string alternate_term;
  std::string prevent_preload;
  int mention_start = 0;
  int mention_end = 0;
  int start_adjust = 0;
  int end_adjust = 0;
  std::string context_language;
  std::string target_language;

  if (source->GetStatus().is_success() && response_code == 200) {
    std::string response;
    bool has_string_response = source->GetResponseAsString(&response);
    DCHECK(has_string_response);
    if (has_string_response) {
      DecodeSearchTermFromJsonResponse(
          response, &search_term, &display_text, &alternate_term,
          &prevent_preload, &mention_start, &mention_end, &context_language);
      if (mention_start != 0 || mention_end != 0) {
        // Sanity check that our selection is non-zero and it is less than
        // 100 characters as that would make contextual search bar hide.
        // We also check that there is at least one character overlap between
        // the new and old selection.
        if (mention_start >= mention_end
            || (mention_end - mention_start) > kContextualSearchMaxSelection
            || mention_end <= context_->start_offset
            || mention_start >= context_->end_offset) {
          start_adjust = 0;
          end_adjust = 0;
        } else {
          start_adjust = mention_start - context_->start_offset;
          end_adjust = mention_end - context_->end_offset;
        }
      }
    }
  }
  bool is_invalid = response_code == net::URLFetcher::RESPONSE_CODE_INVALID;
  ResolvedSearchTerm resolved_search_term(
      is_invalid, response_code, search_term, display_text, alternate_term,
      prevent_preload == kDoPreventPreloadValue, start_adjust, end_adjust,
      context_language);
  search_term_callback_.Run(resolved_search_term);

  // The ContextualSearchContext is consumed once the request has completed.
  context_.reset();
}

// TODO(jeremycho): Remove selected_text and base_page_url CGI parameters.
GURL ContextualSearchDelegate::BuildRequestUrl() {
  // TODO(jeremycho): Confirm this is the right way to handle TemplateURL fails.
  if (!template_url_service_ ||
      !template_url_service_->GetDefaultSearchProvider()) {
    return GURL();
  }

  std::string selected_text_escaped(
      net::EscapeQueryParamValue(context_->selected_text, true));
  std::string base_page_url_escaped(
      net::EscapeQueryParamValue(context_->page_url.spec(), true));
  bool use_resolved_search_term = context_->use_resolved_search_term;

  // If the request is too long, don't include the base-page URL.
  std::string request = GetSearchTermResolutionUrlString(
      selected_text_escaped, base_page_url_escaped, use_resolved_search_term);
  if (request.length() >= kMaxURLSize) {
    request = GetSearchTermResolutionUrlString(
          selected_text_escaped, "", use_resolved_search_term);
  }
  return GURL(request);
}

std::string ContextualSearchDelegate::GetSearchTermResolutionUrlString(
    const std::string& selected_text,
    const std::string& base_page_url,
    const bool use_resolved_search_term) {
  TemplateURL* template_url = template_url_service_->GetDefaultSearchProvider();

  TemplateURLRef::SearchTermsArgs search_terms_args =
      TemplateURLRef::SearchTermsArgs(base::string16());

  TemplateURLRef::SearchTermsArgs::ContextualSearchParams params(
      kContextualSearchRequestVersion,
      selected_text,
      base_page_url,
      use_resolved_search_term);

  search_terms_args.contextual_search_params = params;

  std::string request(
      template_url->contextual_search_url_ref().ReplaceSearchTerms(
          search_terms_args,
          template_url_service_->search_terms_data(),
          NULL));

  // The switch/param should be the URL up to and including the endpoint.
  std::string replacement_url;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
      kContextualSearchResolverUrl)) {
    replacement_url =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kContextualSearchResolverUrl);
  } else {
    std::string param_value = variations::GetVariationParamValue(
        kContextualSearchFieldTrialName, kContextualSearchResolverURLParamName);
    if (!param_value.empty()) replacement_url = param_value;
  }

  // If a replacement URL was specified above, do the substitution.
  if (!replacement_url.empty()) {
    size_t pos = request.find(kContextualSearchServerEndpoint);
    if (pos != std::string::npos) {
      request.replace(0, pos + strlen(kContextualSearchServerEndpoint),
                      replacement_url);
    }
  }
  return request;
}

void ContextualSearchDelegate::GatherSurroundingTextWithCallback(
    const std::string& selection,
    bool use_resolved_search_term,
    content::ContentViewCore* content_view_core,
    bool may_send_base_page_url,
    HandleSurroundingsCallback callback) {
  // Immediately cancel any request that's in flight, since we're building a new
  // context (and the response disposes of any existing context).
  search_term_fetcher_.reset();
  // Decide if the URL should be sent with the context.
  GURL page_url(content_view_core->GetWebContents()->GetURL());
  GURL url_to_send;
  if (may_send_base_page_url &&
      CanSendPageURL(page_url, ProfileManager::GetActiveUserProfile(),
                     template_url_service_)) {
    url_to_send = page_url;
  }
  std::string encoding(content_view_core->GetWebContents()->GetEncoding());
  context_.reset(new ContextualSearchContext(
      selection, use_resolved_search_term, url_to_send, encoding));
  content_view_core->RequestTextSurroundingSelection(
      GetSearchTermSurroundingSize(), callback);
}

void ContextualSearchDelegate::StartSearchTermRequestFromSelection(
    const base::string16& surrounding_text,
    int start_offset,
    int end_offset) {
  // TODO(donnd): figure out how to gather text surrounding the selection
  // for other purposes too: e.g. to determine if we should select the
  // word where the user tapped.
  if (context_.get()) {
    SaveSurroundingText(surrounding_text, start_offset, end_offset);
    SendSurroundingText(kSurroundingSizeForUI);
    ContinueSearchTermResolutionRequest();
  } else {
    DVLOG(1) << "ctxs: Null context, ignored!";
  }
}

void ContextualSearchDelegate::SaveSurroundingText(
    const base::string16& surrounding_text,
    int start_offset,
    int end_offset) {
  DCHECK(context_.get());
  // Sometimes the surroundings are 0, 0, '', so fall back on the selection.
  // See crbug.com/393100.
  if (start_offset == 0 && end_offset == 0 && surrounding_text.length() == 0) {
    context_->surrounding_text = base::UTF8ToUTF16(context_->selected_text);
    context_->start_offset = 0;
    context_->end_offset = context_->selected_text.length();
  } else {
    context_->surrounding_text = surrounding_text;
    context_->start_offset = start_offset;
    context_->end_offset = end_offset;
  }

  // Pin the start and end offsets to ensure they point within the string.
  int surrounding_length = context_->surrounding_text.length();
  context_->start_offset =
      std::min(surrounding_length, std::max(0, context_->start_offset));
  context_->end_offset =
      std::min(surrounding_length, std::max(0, context_->end_offset));

  // Call the Icing callback with a shortened copy of the surroundings.
  int icing_surrounding_size = GetIcingSurroundingSize();
  size_t selection_start = context_->start_offset;
  size_t selection_end = context_->end_offset;
  if (icing_surrounding_size >= 0 && selection_start < selection_end) {
    int icing_padding_each_side = icing_surrounding_size / 2;
    base::string16 icing_surrounding_text = SurroundingTextForIcing(
        context_->surrounding_text, icing_padding_each_side, &selection_start,
        &selection_end);
    if (selection_start < selection_end)
      icing_callback_.Run(context_->encoding, icing_surrounding_text,
                          selection_start, selection_end);
  }
}

void ContextualSearchDelegate::SendSurroundingText(int max_surrounding_chars) {
  const base::string16& surrounding = context_->surrounding_text;

  // Determine the text after the selection.
  int surrounding_length = surrounding.length();  // Cast to int.
  int num_after_characters = std::min(
      surrounding_length - context_->end_offset, max_surrounding_chars);
  base::string16 after_text = surrounding.substr(
      context_->end_offset, num_after_characters);

  base::TrimWhitespace(after_text, base::TRIM_ALL, &after_text);
  surrounding_callback_.Run(UTF16ToUTF8(after_text));
}

void ContextualSearchDelegate::SetDiscourseContextAndAddToHeader(
    const ContextualSearchContext& context) {
  discourse_context::ClientDiscourseContext proto;
  discourse_context::Display* display = proto.add_display();
  display->set_uri(context.page_url.spec());

  discourse_context::Media* media = display->mutable_media();
  media->set_mime_type(context.encoding);

  discourse_context::Selection* selection = display->mutable_selection();
  selection->set_content(UTF16ToUTF8(context.surrounding_text));
  selection->set_start(context.start_offset);
  selection->set_end(context.end_offset);
  selection->set_is_uri_encoded(false);

  std::string serialized;
  proto.SerializeToString(&serialized);

  std::string encoded_context;
  base::Base64Encode(serialized, &encoded_context);
  // The server memoizer expects a web-safe encoding.
  std::replace(encoded_context.begin(), encoded_context.end(), '+', '-');
  std::replace(encoded_context.begin(), encoded_context.end(), '/', '_');
  search_term_fetcher_->AddExtraRequestHeader(
      kDiscourseContextHeaderPrefix + encoded_context);
}

bool ContextualSearchDelegate::CanSendPageURL(
    const GURL& current_page_url,
    Profile* profile,
    TemplateURLService* template_url_service) {
  // Check whether there is a Finch parameter preventing us from sending the
  // page URL.
  std::string param_value = variations::GetVariationParamValue(
      kContextualSearchFieldTrialName, kContextualSearchDoNotSendURLParamName);
  if (!param_value.empty())
    return false;

  // Ensure that the default search provider is Google.
  TemplateURL* default_search_provider =
      template_url_service->GetDefaultSearchProvider();
  bool is_default_search_provider_google =
      default_search_provider &&
      default_search_provider->url_ref().HasGoogleBaseURLs(
          template_url_service->search_terms_data());
  if (!is_default_search_provider_google)
    return false;

  // Only allow HTTP URLs or HTTPS URLs.
  if (current_page_url.scheme() != url::kHttpScheme &&
      (current_page_url.scheme() != url::kHttpsScheme))
    return false;

  // Check that the user has sync enabled, is logged in, and syncs their Chrome
  // History.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  sync_driver::SyncPrefs sync_prefs(profile->GetPrefs());
  if (service == NULL || !service->CanSyncStart() ||
      !sync_prefs.GetPreferredDataTypes(syncer::UserTypes())
           .Has(syncer::PROXY_TABS) ||
      !service->GetActiveDataTypes().Has(syncer::HISTORY_DELETE_DIRECTIVES)) {
    return false;
  }

  return true;
}

// Gets the target language from the translate service using the user's profile.
std::string ContextualSearchDelegate::GetTargetLanguage() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* pref_service = profile->GetPrefs();
  std::string result = TranslateService::GetTargetLanguage(pref_service);
  DCHECK(!result.empty());
  return result;
}

// Returns the accept languages preference string.
std::string ContextualSearchDelegate::GetAcceptLanguages() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  PrefService* pref_service = profile->GetPrefs();
  return pref_service->GetString(prefs::kAcceptLanguages);
}

// Decodes the given response from the search term resolution request and sets
// the value of the given parameters.
void ContextualSearchDelegate::DecodeSearchTermFromJsonResponse(
    const std::string& response,
    std::string* search_term,
    std::string* display_text,
    std::string* alternate_term,
    std::string* prevent_preload,
    int* mention_start,
    int* mention_end,
    std::string* lang) {
  bool contains_xssi_escape = response.find(kXssiEscape) == 0;
  const std::string& proper_json =
      contains_xssi_escape ? response.substr(strlen(kXssiEscape)) : response;
  JSONStringValueDeserializer deserializer(proper_json);
  scoped_ptr<base::Value> root = deserializer.Deserialize(NULL, NULL);

  if (root.get() != NULL && root->IsType(base::Value::TYPE_DICTIONARY)) {
    base::DictionaryValue* dict =
        static_cast<base::DictionaryValue*>(root.get());
    dict->GetString(kContextualSearchPreventPreload, prevent_preload);
    dict->GetString(kContextualSearchResponseSearchTermParam, search_term);
    dict->GetString(kContextualSearchResponseLanguageParam, lang);
    // For the display_text, if not present fall back to the "search_term".
    if (!dict->GetString(kContextualSearchResponseDisplayTextParam,
                         display_text)) {
      *display_text = *search_term;
    }
    // Extract mentions for selection expansion.
    base::ListValue* mentions_list = NULL;
    dict->GetList(kContextualSearchMentions, &mentions_list);
    if (mentions_list != NULL && mentions_list->GetSize() >= 2)
      ExtractMentionsStartEnd(*mentions_list, mention_start, mention_end);
    // If either the selected text or the resolved term is not the search term,
    // use it as the alternate term.
    std::string selected_text;
    dict->GetString(kContextualSearchResponseSelectedTextParam, &selected_text);
    if (selected_text != *search_term) {
      *alternate_term = selected_text;
    } else {
      std::string resolved_term;
      dict->GetString(kContextualSearchResponseResolvedTermParam,
                      &resolved_term);
      if (resolved_term != *search_term) {
        *alternate_term = resolved_term;
      }
    }
  }
}

// Returns the size of the surroundings to be sent to the server for search term
// resolution.
int ContextualSearchDelegate::GetSearchTermSurroundingSize() {
  const std::string param_value = variations::GetVariationParamValue(
      kContextualSearchFieldTrialName,
      kContextualSearchSurroundingSizeParamName);
  int param_length;
  if (!param_value.empty() && base::StringToInt(param_value, &param_length))
    return param_length;
  return kContextualSearchDefaultContentSize;
}

// Extract the Start/End of the mentions in the surrounding text
// for selection-expansion.
void ContextualSearchDelegate::ExtractMentionsStartEnd(
    const base::ListValue& mentions_list,
    int* startResult,
    int* endResult) {
  int int_value;
  if (mentions_list.GetInteger(0, &int_value))
    *startResult = std::max(0, int_value);
  if (mentions_list.GetInteger(1, &int_value))
    *endResult = std::max(0, int_value);
}

// Returns the size of the surroundings to be sent to Icing.
int ContextualSearchDelegate::GetIcingSurroundingSize() {
  std::string param_string = variations::GetVariationParamValue(
      kContextualSearchFieldTrialName,
      kContextualSearchIcingSurroundingSizeParamName);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kContextualSearchIcingSurroundingSizeParamName)) {
    param_string = base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
        kContextualSearchIcingSurroundingSizeParamName);
  }
  int param_value;
  if (!param_string.empty() && base::StringToInt(param_string, &param_value))
    return param_value;
  return kContextualSearchDefaultIcingSurroundingSize;
}

base::string16 ContextualSearchDelegate::SurroundingTextForIcing(
    const base::string16& surrounding_text,
    int padding_each_side,
    size_t* start,
    size_t* end) {
  base::string16 result_text = surrounding_text;
  size_t start_offset = *start;
  size_t end_offset = *end;
  size_t padding_each_side_pinned =
      padding_each_side >= 0 ? padding_each_side : 0;
  // Now trim the context so the portions before or after the selection
  // are within the given limit.
  if (start_offset > padding_each_side_pinned) {
    // Trim the start.
    int trim = start_offset - padding_each_side_pinned;
    result_text = result_text.substr(trim);
    start_offset -= trim;
    end_offset -= trim;
  }
  if (result_text.length() > end_offset + padding_each_side_pinned) {
    // Trim the end.
    result_text = result_text.substr(0, end_offset + padding_each_side_pinned);
  }
  *start = start_offset;
  *end = end_offset;
  return result_text;
}
