// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/template_url.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/google/core/browser/google_util.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/search_terms_data.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "net/base/net_util.h"

namespace {

// The TemplateURLRef has any number of terms that need to be replaced. Each of
// the terms is enclosed in braces. If the character preceeding the final
// brace is a ?, it indicates the term is optional and can be replaced with
// an empty string.
const char kStartParameter = '{';
const char kEndParameter = '}';
const char kOptional = '?';

// Known parameters found in the URL.
const char kSearchTermsParameter[] = "searchTerms";
const char kSearchTermsParameterFull[] = "{searchTerms}";
const char kCountParameter[] = "count";
const char kStartIndexParameter[] = "startIndex";
const char kStartPageParameter[] = "startPage";
const char kLanguageParameter[] = "language";
const char kInputEncodingParameter[] = "inputEncoding";
const char kOutputEncodingParameter[] = "outputEncoding";

const char kGoogleAssistedQueryStatsParameter[] = "google:assistedQueryStats";

// Host/Domain Google searches are relative to.
const char kGoogleBaseURLParameter[] = "google:baseURL";
const char kGoogleBaseURLParameterFull[] = "{google:baseURL}";

// Like google:baseURL, but for the Search Suggest capability.
const char kGoogleBaseSuggestURLParameter[] = "google:baseSuggestURL";
const char kGoogleBaseSuggestURLParameterFull[] = "{google:baseSuggestURL}";
const char kGoogleBookmarkBarPinnedParameter[] = "google:bookmarkBarPinned";
const char kGoogleContextualSearchContextData[] =
    "google:contextualSearchContextData";
const char kGoogleContextualSearchVersion[] = "google:contextualSearchVersion";
const char kGoogleCurrentPageUrlParameter[] = "google:currentPageUrl";
const char kGoogleCursorPositionParameter[] = "google:cursorPosition";
const char kGoogleForceInstantResultsParameter[] = "google:forceInstantResults";
const char kGoogleImageSearchSource[] = "google:imageSearchSource";
const char kGoogleImageThumbnailParameter[] = "google:imageThumbnail";
const char kGoogleImageOriginalWidth[] = "google:imageOriginalWidth";
const char kGoogleImageOriginalHeight[] = "google:imageOriginalHeight";
const char kGoogleImageURLParameter[] = "google:imageURL";
const char kGoogleInputTypeParameter[] = "google:inputType";
const char kGoogleInstantExtendedEnabledParameter[] =
    "google:instantExtendedEnabledParameter";
const char kGoogleInstantExtendedEnabledKey[] =
    "google:instantExtendedEnabledKey";
const char kGoogleInstantExtendedEnabledKeyFull[] =
    "{google:instantExtendedEnabledKey}";
const char kGoogleNTPIsThemedParameter[] = "google:ntpIsThemedParameter";
const char kGoogleOmniboxStartMarginParameter[] =
    "google:omniboxStartMarginParameter";
const char kGoogleOriginalQueryForSuggestionParameter[] =
    "google:originalQueryForSuggestion";
const char kGooglePageClassificationParameter[] = "google:pageClassification";
const char kGooglePrefetchQuery[] = "google:prefetchQuery";
const char kGoogleRLZParameter[] = "google:RLZ";
const char kGoogleSearchClient[] = "google:searchClient";
const char kGoogleSearchFieldtrialParameter[] =
    "google:searchFieldtrialParameter";
const char kGoogleSearchVersion[] = "google:searchVersion";
const char kGoogleSessionToken[] = "google:sessionToken";
const char kGoogleSourceIdParameter[] = "google:sourceId";
const char kGoogleSuggestAPIKeyParameter[] = "google:suggestAPIKeyParameter";
const char kGoogleSuggestClient[] = "google:suggestClient";
const char kGoogleSuggestRequestId[] = "google:suggestRid";

// Same as kSearchTermsParameter, with no escaping.
const char kGoogleUnescapedSearchTermsParameter[] =
    "google:unescapedSearchTerms";
const char kGoogleUnescapedSearchTermsParameterFull[] =
    "{google:unescapedSearchTerms}";

// Display value for kSearchTermsParameter.
const char kDisplaySearchTerms[] = "%s";

// Display value for kGoogleUnescapedSearchTermsParameter.
const char kDisplayUnescapedSearchTerms[] = "%S";

// Used if the count parameter is not optional. Indicates we want 10 search
// results.
const char kDefaultCount[] = "10";

// Used if the parameter kOutputEncodingParameter is required.
const char kOutputEncodingType[] = "UTF-8";

// Attempts to encode |terms| and |original_query| in |encoding| and escape
// them.  |terms| may be escaped as path or query depending on |is_in_query|;
// |original_query| is always escaped as query.  Returns whether the encoding
// process succeeded.
bool TryEncoding(const base::string16& terms,
                 const base::string16& original_query,
                 const char* encoding,
                 bool is_in_query,
                 base::string16* escaped_terms,
                 base::string16* escaped_original_query) {
  DCHECK(escaped_terms);
  DCHECK(escaped_original_query);
  std::string encoded_terms;
  if (!base::UTF16ToCodepage(terms, encoding,
      base::OnStringConversionError::SKIP, &encoded_terms))
    return false;
  *escaped_terms = base::UTF8ToUTF16(is_in_query ?
      net::EscapeQueryParamValue(encoded_terms, true) :
      net::EscapePath(encoded_terms));
  if (original_query.empty())
    return true;
  std::string encoded_original_query;
  if (!base::UTF16ToCodepage(original_query, encoding,
      base::OnStringConversionError::SKIP, &encoded_original_query))
    return false;
  *escaped_original_query = base::UTF8ToUTF16(
      net::EscapeQueryParamValue(encoded_original_query, true));
  return true;
}

// Extract query key and host given a list of parameters coming from the URL
// query or ref.
std::string FindSearchTermsKey(const std::string& params) {
  if (params.empty())
    return std::string();
  url::Component query, key, value;
  query.len = static_cast<int>(params.size());
  while (url::ExtractQueryKeyValue(params.c_str(), &query, &key, &value)) {
    if (key.is_nonempty() && value.is_nonempty()) {
      std::string value_string = params.substr(value.begin, value.len);
      if (value_string.find(kSearchTermsParameterFull, 0) !=
          std::string::npos ||
          value_string.find(kGoogleUnescapedSearchTermsParameterFull, 0) !=
          std::string::npos) {
        return params.substr(key.begin, key.len);
      }
    }
  }
  return std::string();
}

bool IsTemplateParameterString(const std::string& param) {
  return (param.length() > 2) && (*(param.begin()) == kStartParameter) &&
      (*(param.rbegin()) == kEndParameter);
}

}  // namespace


// TemplateURLRef::SearchTermsArgs --------------------------------------------

TemplateURLRef::SearchTermsArgs::SearchTermsArgs(
    const base::string16& search_terms)
    : search_terms(search_terms),
      input_type(metrics::OmniboxInputType::INVALID),
      accepted_suggestion(NO_SUGGESTIONS_AVAILABLE),
      cursor_position(base::string16::npos),
      enable_omnibox_start_margin(false),
      page_classification(metrics::OmniboxEventProto::INVALID_SPEC),
      bookmark_bar_pinned(false),
      append_extra_query_params(false),
      force_instant_results(false),
      from_app_list(false),
      contextual_search_params(ContextualSearchParams()) {
}

TemplateURLRef::SearchTermsArgs::~SearchTermsArgs() {
}

TemplateURLRef::SearchTermsArgs::ContextualSearchParams::
    ContextualSearchParams()
    : version(-1),
      start(base::string16::npos),
      end(base::string16::npos),
      resolve(true) {
}

TemplateURLRef::SearchTermsArgs::ContextualSearchParams::
    ContextualSearchParams(
        const int version,
        const std::string& selection,
        const std::string& base_page_url,
        const bool resolve)
    : version(version),
      start(base::string16::npos),
      end(base::string16::npos),
      selection(selection),
      base_page_url(base_page_url),
      resolve(resolve) {
}

TemplateURLRef::SearchTermsArgs::ContextualSearchParams::
    ContextualSearchParams(
        const int version,
        const size_t start,
        const size_t end,
        const std::string& selection,
        const std::string& content,
        const std::string& base_page_url,
        const std::string& encoding,
        const bool resolve)
    : version(version),
      start(start),
      end(end),
      selection(selection),
      content(content),
      base_page_url(base_page_url),
      encoding(encoding),
      resolve(resolve) {
}

TemplateURLRef::SearchTermsArgs::ContextualSearchParams::
    ~ContextualSearchParams() {
}

// TemplateURLRef -------------------------------------------------------------

TemplateURLRef::TemplateURLRef(TemplateURL* owner, Type type)
    : owner_(owner),
      type_(type),
      index_in_owner_(0),
      parsed_(false),
      valid_(false),
      supports_replacements_(false),
      search_term_key_location_(url::Parsed::QUERY),
      prepopulated_(false) {
  DCHECK(owner_);
  DCHECK_NE(INDEXED, type_);
}

TemplateURLRef::TemplateURLRef(TemplateURL* owner, size_t index_in_owner)
    : owner_(owner),
      type_(INDEXED),
      index_in_owner_(index_in_owner),
      parsed_(false),
      valid_(false),
      supports_replacements_(false),
      search_term_key_location_(url::Parsed::QUERY),
      prepopulated_(false) {
  DCHECK(owner_);
  DCHECK_LT(index_in_owner_, owner_->URLCount());
}

TemplateURLRef::~TemplateURLRef() {
}

std::string TemplateURLRef::GetURL() const {
  switch (type_) {
    case SEARCH:            return owner_->url();
    case SUGGEST:           return owner_->suggestions_url();
    case INSTANT:           return owner_->instant_url();
    case IMAGE:             return owner_->image_url();
    case NEW_TAB:           return owner_->new_tab_url();
    case CONTEXTUAL_SEARCH: return owner_->contextual_search_url();
    case INDEXED:           return owner_->GetURL(index_in_owner_);
    default:       NOTREACHED(); return std::string();  // NOLINT
  }
}

std::string TemplateURLRef::GetPostParamsString() const {
  switch (type_) {
    case INDEXED:
    case SEARCH:            return owner_->search_url_post_params();
    case SUGGEST:           return owner_->suggestions_url_post_params();
    case INSTANT:           return owner_->instant_url_post_params();
    case NEW_TAB:           return std::string();
    case CONTEXTUAL_SEARCH: return std::string();
    case IMAGE:             return owner_->image_url_post_params();
    default:      NOTREACHED(); return std::string();  // NOLINT
  }
}

bool TemplateURLRef::UsesPOSTMethod(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return !post_params_.empty();
}

bool TemplateURLRef::EncodeFormData(const PostParams& post_params,
                                    PostContent* post_content) const {
  if (post_params.empty())
    return true;
  if (!post_content)
    return false;

  const char kUploadDataMIMEType[] = "multipart/form-data; boundary=";
  const char kMultipartBoundary[] = "----+*+----%016" PRIx64 "----+*+----";
  // Each name/value pair is stored in a body part which is preceded by a
  // boundary delimiter line. Uses random number generator here to create
  // a unique boundary delimiter for form data encoding.
  std::string boundary = base::StringPrintf(kMultipartBoundary,
                                            base::RandUint64());
  // Sets the content MIME type.
  post_content->first = kUploadDataMIMEType;
  post_content->first += boundary;
  // Encodes the post parameters.
  std::string* post_data = &post_content->second;
  post_data->clear();
  for (PostParams::const_iterator param = post_params.begin();
       param != post_params.end(); ++param) {
    DCHECK(!param->first.empty());
    net::AddMultipartValueForUpload(param->first, param->second, boundary,
                                    std::string(), post_data);
  }
  net::AddMultipartFinalDelimiterForUpload(boundary, post_data);
  return true;
}

bool TemplateURLRef::SupportsReplacement(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return valid_ && supports_replacements_;
}

std::string TemplateURLRef::ReplaceSearchTerms(
    const SearchTermsArgs& search_terms_args,
    const SearchTermsData& search_terms_data,
    PostContent* post_content) const {
  ParseIfNecessary(search_terms_data);
  if (!valid_)
    return std::string();

  std::string url(HandleReplacements(search_terms_args, search_terms_data,
                                     post_content));

  GURL gurl(url);
  if (!gurl.is_valid())
    return url;

  std::vector<std::string> query_params;
  if (search_terms_args.append_extra_query_params) {
    std::string extra_params(
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kExtraSearchQueryParams));
    if (!extra_params.empty())
      query_params.push_back(extra_params);
  }
  if (!search_terms_args.suggest_query_params.empty())
    query_params.push_back(search_terms_args.suggest_query_params);
  if (!gurl.query().empty())
    query_params.push_back(gurl.query());

  if (query_params.empty())
    return url;

  GURL::Replacements replacements;
  std::string query_str = JoinString(query_params, "&");
  replacements.SetQueryStr(query_str);
  return gurl.ReplaceComponents(replacements).possibly_invalid_spec();
}

bool TemplateURLRef::IsValid(const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return valid_;
}

base::string16 TemplateURLRef::DisplayURL(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  base::string16 result(base::UTF8ToUTF16(GetURL()));
  if (valid_ && !replacements_.empty()) {
    ReplaceSubstringsAfterOffset(&result, 0,
                                 base::ASCIIToUTF16(kSearchTermsParameterFull),
                                 base::ASCIIToUTF16(kDisplaySearchTerms));
    ReplaceSubstringsAfterOffset(&result, 0,
        base::ASCIIToUTF16(kGoogleUnescapedSearchTermsParameterFull),
        base::ASCIIToUTF16(kDisplayUnescapedSearchTerms));
  }
  return result;
}

// static
std::string TemplateURLRef::DisplayURLToURLRef(
    const base::string16& display_url) {
  base::string16 result = display_url;
  ReplaceSubstringsAfterOffset(&result, 0,
                               base::ASCIIToUTF16(kDisplaySearchTerms),
                               base::ASCIIToUTF16(kSearchTermsParameterFull));
  ReplaceSubstringsAfterOffset(
      &result, 0,
      base::ASCIIToUTF16(kDisplayUnescapedSearchTerms),
      base::ASCIIToUTF16(kGoogleUnescapedSearchTermsParameterFull));
  return base::UTF16ToUTF8(result);
}

const std::string& TemplateURLRef::GetHost(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return host_;
}

const std::string& TemplateURLRef::GetPath(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return path_;
}

const std::string& TemplateURLRef::GetSearchTermKey(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  return search_term_key_;
}

base::string16 TemplateURLRef::SearchTermToString16(
    const std::string& term) const {
  const std::vector<std::string>& encodings = owner_->input_encodings();
  base::string16 result;

  std::string unescaped = net::UnescapeURLComponent(
      term,
      net::UnescapeRule::REPLACE_PLUS_WITH_SPACE |
      net::UnescapeRule::URL_SPECIAL_CHARS);
  for (size_t i = 0; i < encodings.size(); ++i) {
    if (base::CodepageToUTF16(unescaped, encodings[i].c_str(),
                              base::OnStringConversionError::FAIL, &result))
      return result;
  }

  // Always fall back on UTF-8 if it works.
  if (base::CodepageToUTF16(unescaped, base::kCodepageUTF8,
                            base::OnStringConversionError::FAIL, &result))
    return result;

  // When nothing worked, just use the escaped text. We have no idea what the
  // encoding is. We need to substitute spaces for pluses ourselves since we're
  // not sending it through an unescaper.
  result = base::UTF8ToUTF16(term);
  std::replace(result.begin(), result.end(), '+', ' ');
  return result;
}

bool TemplateURLRef::HasGoogleBaseURLs(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessary(search_terms_data);
  for (size_t i = 0; i < replacements_.size(); ++i) {
    if ((replacements_[i].type == GOOGLE_BASE_URL) ||
        (replacements_[i].type == GOOGLE_BASE_SUGGEST_URL))
      return true;
  }
  return false;
}

bool TemplateURLRef::ExtractSearchTermsFromURL(
    const GURL& url,
    base::string16* search_terms,
    const SearchTermsData& search_terms_data,
    url::Parsed::ComponentType* search_terms_component,
    url::Component* search_terms_position) const {
  DCHECK(search_terms);
  search_terms->clear();

  ParseIfNecessary(search_terms_data);

  // We need a search term in the template URL to extract something.
  if (search_term_key_.empty())
    return false;

  // TODO(beaudoin): Support patterns of the form http://foo/{searchTerms}/
  // See crbug.com/153798

  // Fill-in the replacements. We don't care about search terms in the pattern,
  // so we use the empty string.
  // Currently we assume the search term only shows in URL, not in post params.
  GURL pattern(ReplaceSearchTerms(SearchTermsArgs(base::string16()),
                                  search_terms_data, NULL));
  // Host, path and port must match.
  if (url.port() != pattern.port() ||
      url.host() != host_ ||
      url.path() != path_) {
    return false;
  }

  // Parameter must be present either in the query or the ref.
  const std::string& params(
      (search_term_key_location_ == url::Parsed::QUERY) ?
          url.query() : url.ref());

  url::Component query, key, value;
  query.len = static_cast<int>(params.size());
  bool key_found = false;
  while (url::ExtractQueryKeyValue(params.c_str(), &query, &key, &value)) {
    if (key.is_nonempty()) {
      if (params.substr(key.begin, key.len) == search_term_key_) {
        // Fail if search term key is found twice.
        if (key_found) {
          search_terms->clear();
          return false;
        }
        key_found = true;
        // Extract the search term.
        *search_terms = net::UnescapeAndDecodeUTF8URLComponent(
            params.substr(value.begin, value.len),
            net::UnescapeRule::SPACES |
                net::UnescapeRule::URL_SPECIAL_CHARS |
                net::UnescapeRule::REPLACE_PLUS_WITH_SPACE);
        if (search_terms_component)
          *search_terms_component = search_term_key_location_;
        if (search_terms_position)
          *search_terms_position = value;
      }
    }
  }
  return key_found;
}

void TemplateURLRef::InvalidateCachedValues() const {
  supports_replacements_ = valid_ = parsed_ = false;
  host_.clear();
  path_.clear();
  search_term_key_.clear();
  replacements_.clear();
  post_params_.clear();
}

bool TemplateURLRef::ParseParameter(size_t start,
                                    size_t end,
                                    std::string* url,
                                    Replacements* replacements) const {
  DCHECK(start != std::string::npos &&
         end != std::string::npos && end > start);
  size_t length = end - start - 1;
  bool optional = false;
  if ((*url)[end - 1] == kOptional) {
    optional = true;
    length--;
  }
  std::string parameter(url->substr(start + 1, length));
  std::string full_parameter(url->substr(start, end - start + 1));
  // Remove the parameter from the string.  For parameters who replacement is
  // constant and already known, just replace them directly.  For other cases,
  // like parameters whose values may change over time, use |replacements|.
  url->erase(start, end - start + 1);
  if (parameter == kSearchTermsParameter) {
    replacements->push_back(Replacement(SEARCH_TERMS, start));
  } else if (parameter == kCountParameter) {
    if (!optional)
      url->insert(start, kDefaultCount);
  } else if (parameter == kGoogleAssistedQueryStatsParameter) {
    replacements->push_back(Replacement(GOOGLE_ASSISTED_QUERY_STATS, start));
  } else if (parameter == kGoogleBaseURLParameter) {
    replacements->push_back(Replacement(GOOGLE_BASE_URL, start));
  } else if (parameter == kGoogleBaseSuggestURLParameter) {
    replacements->push_back(Replacement(GOOGLE_BASE_SUGGEST_URL, start));
  } else if (parameter == kGoogleBookmarkBarPinnedParameter) {
    replacements->push_back(Replacement(GOOGLE_BOOKMARK_BAR_PINNED, start));
  } else if (parameter == kGoogleCurrentPageUrlParameter) {
    replacements->push_back(Replacement(GOOGLE_CURRENT_PAGE_URL, start));
  } else if (parameter == kGoogleCursorPositionParameter) {
    replacements->push_back(Replacement(GOOGLE_CURSOR_POSITION, start));
  } else if (parameter == kGoogleForceInstantResultsParameter) {
    replacements->push_back(Replacement(GOOGLE_FORCE_INSTANT_RESULTS, start));
  } else if (parameter == kGoogleImageOriginalHeight) {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_ORIGINAL_HEIGHT, start));
  } else if (parameter == kGoogleImageOriginalWidth) {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_ORIGINAL_WIDTH, start));
  } else if (parameter == kGoogleImageSearchSource) {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_SEARCH_SOURCE, start));
  } else if (parameter == kGoogleImageThumbnailParameter) {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_THUMBNAIL, start));
  } else if (parameter == kGoogleImageURLParameter) {
    replacements->push_back(Replacement(TemplateURLRef::GOOGLE_IMAGE_URL,
                                        start));
  } else if (parameter == kGoogleInputTypeParameter) {
    replacements->push_back(Replacement(TemplateURLRef::GOOGLE_INPUT_TYPE,
                                        start));
  } else if (parameter == kGoogleInstantExtendedEnabledParameter) {
    replacements->push_back(Replacement(GOOGLE_INSTANT_EXTENDED_ENABLED,
                                        start));
  } else if (parameter == kGoogleInstantExtendedEnabledKey) {
    url->insert(start, google_util::kInstantExtendedAPIParam);
  } else if (parameter == kGoogleNTPIsThemedParameter) {
    replacements->push_back(Replacement(GOOGLE_NTP_IS_THEMED, start));
  } else if (parameter == kGoogleOmniboxStartMarginParameter) {
    replacements->push_back(Replacement(GOOGLE_OMNIBOX_START_MARGIN, start));
  } else if (parameter == kGoogleContextualSearchVersion) {
    replacements->push_back(
        Replacement(GOOGLE_CONTEXTUAL_SEARCH_VERSION, start));
  } else if (parameter == kGoogleContextualSearchContextData) {
    replacements->push_back(
        Replacement(GOOGLE_CONTEXTUAL_SEARCH_CONTEXT_DATA, start));
  } else if (parameter == kGoogleOriginalQueryForSuggestionParameter) {
    replacements->push_back(Replacement(GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
                                        start));
  } else if (parameter == kGooglePageClassificationParameter) {
    replacements->push_back(Replacement(GOOGLE_PAGE_CLASSIFICATION, start));
  } else if (parameter == kGooglePrefetchQuery) {
    replacements->push_back(Replacement(GOOGLE_PREFETCH_QUERY, start));
  } else if (parameter == kGoogleRLZParameter) {
    replacements->push_back(Replacement(GOOGLE_RLZ, start));
  } else if (parameter == kGoogleSearchClient) {
    replacements->push_back(Replacement(GOOGLE_SEARCH_CLIENT, start));
  } else if (parameter == kGoogleSearchFieldtrialParameter) {
    replacements->push_back(Replacement(GOOGLE_SEARCH_FIELDTRIAL_GROUP, start));
  } else if (parameter == kGoogleSearchVersion) {
    replacements->push_back(Replacement(GOOGLE_SEARCH_VERSION, start));
  } else if (parameter == kGoogleSessionToken) {
    replacements->push_back(Replacement(GOOGLE_SESSION_TOKEN, start));
  } else if (parameter == kGoogleSourceIdParameter) {
#if defined(OS_ANDROID)
    url->insert(start, "sourceid=chrome-mobile&");
#else
    url->insert(start, "sourceid=chrome&");
#endif
  } else if (parameter == kGoogleSuggestAPIKeyParameter) {
    url->insert(start,
                net::EscapeQueryParamValue(google_apis::GetAPIKey(), false));
  } else if (parameter == kGoogleSuggestClient) {
    replacements->push_back(Replacement(GOOGLE_SUGGEST_CLIENT, start));
  } else if (parameter == kGoogleSuggestRequestId) {
    replacements->push_back(Replacement(GOOGLE_SUGGEST_REQUEST_ID, start));
  } else if (parameter == kGoogleUnescapedSearchTermsParameter) {
    replacements->push_back(Replacement(GOOGLE_UNESCAPED_SEARCH_TERMS, start));
  } else if (parameter == kInputEncodingParameter) {
    replacements->push_back(Replacement(ENCODING, start));
  } else if (parameter == kLanguageParameter) {
    replacements->push_back(Replacement(LANGUAGE, start));
  } else if (parameter == kOutputEncodingParameter) {
    if (!optional)
      url->insert(start, kOutputEncodingType);
  } else if ((parameter == kStartIndexParameter) ||
             (parameter == kStartPageParameter)) {
    // We don't support these.
    if (!optional)
      url->insert(start, "1");
  } else if (!prepopulated_) {
    // If it's a prepopulated URL, we know that it's safe to remove unknown
    // parameters, so just ignore this and return true below. Otherwise it could
    // be some garbage but can also be a javascript block. Put it back.
    url->insert(start, full_parameter);
    return false;
  }
  return true;
}

std::string TemplateURLRef::ParseURL(const std::string& url,
                                     Replacements* replacements,
                                     PostParams* post_params,
                                     bool* valid) const {
  *valid = false;
  std::string parsed_url = url;
  for (size_t last = 0; last != std::string::npos; ) {
    last = parsed_url.find(kStartParameter, last);
    if (last != std::string::npos) {
      size_t template_end = parsed_url.find(kEndParameter, last);
      if (template_end != std::string::npos) {
        // Since we allow Javascript in the URL, {} pairs could be nested. Match
        // only leaf pairs with supported parameters.
        size_t next_template_start = parsed_url.find(kStartParameter, last + 1);
        if (next_template_start == std::string::npos ||
            next_template_start > template_end) {
          // If successful, ParseParameter erases from the string as such no
          // need to update |last|. If failed, move |last| to the end of pair.
          if (!ParseParameter(last, template_end, &parsed_url, replacements)) {
            // |template_end| + 1 may be beyond the end of the string.
            last = template_end;
          }
        } else {
          last = next_template_start;
        }
      } else {
        // Open brace without a closing brace, return.
        return std::string();
      }
    }
  }

  // Handles the post parameters.
  const std::string& post_params_string = GetPostParamsString();
  if (!post_params_string.empty()) {
    typedef std::vector<std::string> Strings;
    Strings param_list;
    base::SplitString(post_params_string, ',', &param_list);

    for (Strings::const_iterator iterator = param_list.begin();
         iterator != param_list.end(); ++iterator) {
      Strings parts;
      // The '=' delimiter is required and the name must be not empty.
      base::SplitString(*iterator, '=', &parts);
      if ((parts.size() != 2U) || parts[0].empty())
        return std::string();

      std::string& value = parts[1];
      size_t replacements_size = replacements->size();
      if (IsTemplateParameterString(value))
        ParseParameter(0, value.length() - 1, &value, replacements);
      post_params->push_back(std::make_pair(parts[0], value));
      // If there was a replacement added, points its index to last added
      // PostParam.
      if (replacements->size() > replacements_size) {
        DCHECK_EQ(replacements_size + 1, replacements->size());
        Replacement* r = &replacements->back();
        r->is_post_param = true;
        r->index = post_params->size() - 1;
      }
    }
    DCHECK(!post_params->empty());
  }

  *valid = true;
  return parsed_url;
}

void TemplateURLRef::ParseIfNecessary(
    const SearchTermsData& search_terms_data) const {
  if (!parsed_) {
    InvalidateCachedValues();
    parsed_ = true;
    parsed_url_ = ParseURL(GetURL(), &replacements_, &post_params_, &valid_);
    supports_replacements_ = false;
    if (valid_) {
      bool has_only_one_search_term = false;
      for (Replacements::const_iterator i = replacements_.begin();
           i != replacements_.end(); ++i) {
        if ((i->type == SEARCH_TERMS) ||
            (i->type == GOOGLE_UNESCAPED_SEARCH_TERMS)) {
          if (has_only_one_search_term) {
            has_only_one_search_term = false;
            break;
          }
          has_only_one_search_term = true;
          supports_replacements_ = true;
        }
      }
      // Only parse the host/key if there is one search term. Technically there
      // could be more than one term, but it's uncommon; so we punt.
      if (has_only_one_search_term)
        ParseHostAndSearchTermKey(search_terms_data);
    }
  }
}

void TemplateURLRef::ParseHostAndSearchTermKey(
    const SearchTermsData& search_terms_data) const {
  std::string url_string(GetURL());
  ReplaceSubstringsAfterOffset(&url_string, 0,
                               kGoogleBaseURLParameterFull,
                               search_terms_data.GoogleBaseURLValue());
  ReplaceSubstringsAfterOffset(&url_string, 0,
                               kGoogleBaseSuggestURLParameterFull,
                               search_terms_data.GoogleBaseSuggestURLValue());

  search_term_key_.clear();
  host_.clear();
  path_.clear();
  search_term_key_location_ = url::Parsed::REF;

  GURL url(url_string);
  if (!url.is_valid())
    return;

  std::string query_key = FindSearchTermsKey(url.query());
  std::string ref_key = FindSearchTermsKey(url.ref());
  if (query_key.empty() == ref_key.empty())
    return;  // No key or multiple keys found.  We only handle having one key.
  search_term_key_ = query_key.empty() ? ref_key : query_key;
  search_term_key_location_ =
      query_key.empty() ? url::Parsed::REF : url::Parsed::QUERY;
  host_ = url.host();
  path_ = url.path();
}

void TemplateURLRef::HandleReplacement(const std::string& name,
                                       const std::string& value,
                                       const Replacement& replacement,
                                       std::string* url) const {
  size_t pos = replacement.index;
  if (replacement.is_post_param) {
    DCHECK_LT(pos, post_params_.size());
    DCHECK(!post_params_[pos].first.empty());
    post_params_[pos].second = value;
  } else {
    url->insert(pos, name.empty() ? value : (name + "=" + value + "&"));
  }
}

std::string TemplateURLRef::HandleReplacements(
    const SearchTermsArgs& search_terms_args,
    const SearchTermsData& search_terms_data,
    PostContent* post_content) const {
  if (replacements_.empty()) {
    if (!post_params_.empty())
      EncodeFormData(post_params_, post_content);
    return parsed_url_;
  }

  // Determine if the search terms are in the query or before. We're escaping
  // space as '+' in the former case and as '%20' in the latter case.
  bool is_in_query = true;
  for (Replacements::iterator i = replacements_.begin();
       i != replacements_.end(); ++i) {
    if (i->type == SEARCH_TERMS) {
      base::string16::size_type query_start = parsed_url_.find('?');
      is_in_query = query_start != base::string16::npos &&
          (static_cast<base::string16::size_type>(i->index) > query_start);
      break;
    }
  }

  std::string input_encoding;
  base::string16 encoded_terms;
  base::string16 encoded_original_query;
  owner_->EncodeSearchTerms(search_terms_args, is_in_query, &input_encoding,
                            &encoded_terms, &encoded_original_query);

  std::string url = parsed_url_;

  // replacements_ is ordered in ascending order, as such we need to iterate
  // from the back.
  for (Replacements::reverse_iterator i = replacements_.rbegin();
       i != replacements_.rend(); ++i) {
    switch (i->type) {
      case ENCODING:
        HandleReplacement(std::string(), input_encoding, *i, &url);
        break;

      case GOOGLE_ASSISTED_QUERY_STATS:
        DCHECK(!i->is_post_param);
        if (!search_terms_args.assisted_query_stats.empty()) {
          // Get the base URL without substituting AQS to avoid infinite
          // recursion.  We need the URL to find out if it meets all
          // AQS requirements (e.g. HTTPS protocol check).
          // See TemplateURLRef::SearchTermsArgs for more details.
          SearchTermsArgs search_terms_args_without_aqs(search_terms_args);
          search_terms_args_without_aqs.assisted_query_stats.clear();
          GURL base_url(ReplaceSearchTerms(
              search_terms_args_without_aqs, search_terms_data, NULL));
          if (base_url.SchemeIs(url::kHttpsScheme)) {
            HandleReplacement(
                "aqs", search_terms_args.assisted_query_stats, *i, &url);
          }
        }
        break;

      case GOOGLE_BASE_URL:
        DCHECK(!i->is_post_param);
        HandleReplacement(
            std::string(), search_terms_data.GoogleBaseURLValue(), *i, &url);
        break;

      case GOOGLE_BASE_SUGGEST_URL:
        DCHECK(!i->is_post_param);
        HandleReplacement(
            std::string(), search_terms_data.GoogleBaseSuggestURLValue(), *i,
            &url);
        break;

      case GOOGLE_BOOKMARK_BAR_PINNED:
        if (search_terms_data.IsShowingSearchTermsOnSearchResultsPages()) {
          // Log whether the bookmark bar is pinned when the user is seeing
          // InstantExtended on the SRP.
          DCHECK(!i->is_post_param);
          HandleReplacement(
              "bmbp", search_terms_args.bookmark_bar_pinned ? "1" : "0", *i,
              &url);
        }
        break;

      case GOOGLE_CURRENT_PAGE_URL:
        DCHECK(!i->is_post_param);
        if (!search_terms_args.current_page_url.empty()) {
          const std::string& escaped_current_page_url =
              net::EscapeQueryParamValue(search_terms_args.current_page_url,
                                         true);
          HandleReplacement("url", escaped_current_page_url, *i, &url);
        }
        break;

      case GOOGLE_CURSOR_POSITION:
        DCHECK(!i->is_post_param);
        if (search_terms_args.cursor_position != base::string16::npos)
          HandleReplacement(
              "cp",
              base::StringPrintf("%" PRIuS, search_terms_args.cursor_position),
              *i,
              &url);
        break;

      case GOOGLE_FORCE_INSTANT_RESULTS:
        DCHECK(!i->is_post_param);
        HandleReplacement(std::string(),
                          search_terms_data.ForceInstantResultsParam(
                              search_terms_args.force_instant_results),
                          *i,
                          &url);
        break;

      case GOOGLE_INPUT_TYPE:
        DCHECK(!i->is_post_param);
        HandleReplacement(
            "oit", base::IntToString(search_terms_args.input_type), *i, &url);
        break;

      case GOOGLE_INSTANT_EXTENDED_ENABLED:
        DCHECK(!i->is_post_param);
        HandleReplacement(std::string(),
                          search_terms_data.InstantExtendedEnabledParam(
                              type_ == SEARCH),
                          *i,
                          &url);
        break;

      case GOOGLE_NTP_IS_THEMED:
        DCHECK(!i->is_post_param);
        HandleReplacement(
            std::string(), search_terms_data.NTPIsThemedParam(), *i, &url);
        break;

      case GOOGLE_OMNIBOX_START_MARGIN:
        DCHECK(!i->is_post_param);
        if (search_terms_args.enable_omnibox_start_margin) {
          int omnibox_start_margin = search_terms_data.OmniboxStartMargin();
          if (omnibox_start_margin >= 0) {
            HandleReplacement("es_sm", base::IntToString(omnibox_start_margin),
                              *i, &url);
          }
        }
        break;

      case GOOGLE_CONTEXTUAL_SEARCH_VERSION:
        if (search_terms_args.contextual_search_params.version >= 0) {
          HandleReplacement(
              "ctxs",
              base::IntToString(
                  search_terms_args.contextual_search_params.version),
              *i,
              &url);
        }
        break;

      case GOOGLE_CONTEXTUAL_SEARCH_CONTEXT_DATA: {
        DCHECK(!i->is_post_param);
        std::string context_data;

        const SearchTermsArgs::ContextualSearchParams& params =
            search_terms_args.contextual_search_params;

        if (params.start != std::string::npos) {
          context_data.append("ctxs_start=" + base::IntToString(
              params.start) + "&");
        }

        if (params.end != std::string::npos) {
          context_data.append("ctxs_end=" + base::IntToString(
              params.end) + "&");
        }

        if (!params.selection.empty())
          context_data.append("q=" + params.selection + "&");

        if (!params.content.empty())
          context_data.append("ctxs_content=" + params.content + "&");

        if (!params.base_page_url.empty())
          context_data.append("ctxsl_url=" + params.base_page_url + "&");

        if (!params.encoding.empty()) {
          context_data.append("ctxs_encoding=" + params.encoding + "&");
        }

        context_data.append(
            params.resolve ? "ctxsl_resolve=1" : "ctxsl_resolve=0");

        HandleReplacement(std::string(), context_data, *i, &url);
        break;
      }

      case GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION:
        DCHECK(!i->is_post_param);
        if (search_terms_args.accepted_suggestion >= 0 ||
            !search_terms_args.assisted_query_stats.empty()) {
          HandleReplacement(
              "oq", base::UTF16ToUTF8(encoded_original_query), *i, &url);
        }
        break;

      case GOOGLE_PAGE_CLASSIFICATION:
        if (search_terms_args.page_classification !=
            metrics::OmniboxEventProto::INVALID_SPEC) {
          HandleReplacement(
              "pgcl", base::IntToString(search_terms_args.page_classification),
              *i, &url);
        }
        break;

      case GOOGLE_PREFETCH_QUERY: {
        const std::string& query = search_terms_args.prefetch_query;
        const std::string& type = search_terms_args.prefetch_query_type;
        if (!query.empty() && !type.empty()) {
          HandleReplacement(
              std::string(), "pfq=" + query + "&qha=" + type + "&", *i, &url);
        }
        break;
      }

      case GOOGLE_RLZ: {
        DCHECK(!i->is_post_param);
        // On platforms that don't have RLZ, we still want this branch
        // to happen so that we replace the RLZ template with the
        // empty string.  (If we don't handle this case, we hit a
        // NOTREACHED below.)
        base::string16 rlz_string = search_terms_data.GetRlzParameterValue(
            search_terms_args.from_app_list);
        if (!rlz_string.empty()) {
          HandleReplacement("rlz", base::UTF16ToUTF8(rlz_string), *i, &url);
        }
        break;
      }

      case GOOGLE_SEARCH_CLIENT: {
        DCHECK(!i->is_post_param);
        std::string client = search_terms_data.GetSearchClient();
        if (!client.empty())
          HandleReplacement("client", client, *i, &url);
        break;
      }

      case GOOGLE_SEARCH_FIELDTRIAL_GROUP:
        // We are not currently running any fieldtrials that modulate the search
        // url.  If we do, then we'd have some conditional insert such as:
        // url.insert(i->index, used_www ? "gcx=w&" : "gcx=c&");
        break;

      case GOOGLE_SEARCH_VERSION:
        if (search_terms_data.EnableAnswersInSuggest())
          HandleReplacement("gs_rn", "42", *i, &url);
        break;

      case GOOGLE_SESSION_TOKEN: {
        std::string token = search_terms_args.session_token;
        if (!token.empty())
          HandleReplacement("psi", token, *i, &url);
        break;
      }

      case GOOGLE_SUGGEST_CLIENT:
        HandleReplacement(
            std::string(), search_terms_data.GetSuggestClient(), *i, &url);
        break;

      case GOOGLE_SUGGEST_REQUEST_ID:
        HandleReplacement(
            std::string(), search_terms_data.GetSuggestRequestIdentifier(), *i,
            &url);
        break;

      case GOOGLE_UNESCAPED_SEARCH_TERMS: {
        std::string unescaped_terms;
        base::UTF16ToCodepage(search_terms_args.search_terms,
                              input_encoding.c_str(),
                              base::OnStringConversionError::SKIP,
                              &unescaped_terms);
        HandleReplacement(std::string(), unescaped_terms, *i, &url);
        break;
      }

      case LANGUAGE:
        HandleReplacement(
            std::string(), search_terms_data.GetApplicationLocale(), *i, &url);
        break;

      case SEARCH_TERMS:
        HandleReplacement(
            std::string(), base::UTF16ToUTF8(encoded_terms), *i, &url);
        break;

      case GOOGLE_IMAGE_THUMBNAIL:
        HandleReplacement(
            std::string(), search_terms_args.image_thumbnail_content, *i, &url);
        break;

      case GOOGLE_IMAGE_URL:
        if (search_terms_args.image_url.is_valid()) {
          HandleReplacement(
              std::string(), search_terms_args.image_url.spec(), *i, &url);
        }
        break;

      case GOOGLE_IMAGE_ORIGINAL_WIDTH:
        if (!search_terms_args.image_original_size.IsEmpty()) {
          HandleReplacement(
              std::string(),
              base::IntToString(search_terms_args.image_original_size.width()),
              *i, &url);
        }
        break;

      case GOOGLE_IMAGE_ORIGINAL_HEIGHT:
        if (!search_terms_args.image_original_size.IsEmpty()) {
          HandleReplacement(
              std::string(),
              base::IntToString(search_terms_args.image_original_size.height()),
              *i, &url);
        }
        break;

      case GOOGLE_IMAGE_SEARCH_SOURCE:
        HandleReplacement(
            std::string(), search_terms_data.GoogleImageSearchSource(), *i,
            &url);
        break;

      default:
        NOTREACHED();
        break;
    }
  }

  if (!post_params_.empty())
    EncodeFormData(post_params_, post_content);

  return url;
}


// TemplateURL ----------------------------------------------------------------

TemplateURL::AssociatedExtensionInfo::AssociatedExtensionInfo(
    Type type,
    const std::string& extension_id)
    : type(type),
      extension_id(extension_id),
      wants_to_be_default_engine(false) {
  DCHECK_NE(NORMAL, type);
}

TemplateURL::AssociatedExtensionInfo::~AssociatedExtensionInfo() {
}

TemplateURL::TemplateURL(const TemplateURLData& data)
    : data_(data),
      url_ref_(this, TemplateURLRef::SEARCH),
      suggestions_url_ref_(this,
                           TemplateURLRef::SUGGEST),
      instant_url_ref_(this,
                       TemplateURLRef::INSTANT),
      image_url_ref_(this, TemplateURLRef::IMAGE),
      new_tab_url_ref_(this, TemplateURLRef::NEW_TAB),
      contextual_search_url_ref_(this, TemplateURLRef::CONTEXTUAL_SEARCH) {
  SetPrepopulateId(data_.prepopulate_id);

  if (data_.search_terms_replacement_key ==
      kGoogleInstantExtendedEnabledKeyFull) {
    data_.search_terms_replacement_key = google_util::kInstantExtendedAPIParam;
  }
}

TemplateURL::~TemplateURL() {
}

// static
base::string16 TemplateURL::GenerateKeyword(const GURL& url) {
  DCHECK(url.is_valid());
  // Strip "www." off the front of the keyword; otherwise the keyword won't work
  // properly.  See http://code.google.com/p/chromium/issues/detail?id=6984 .
  // Special case: if the host was exactly "www." (not sure this can happen but
  // perhaps with some weird intranet and custom DNS server?), ensure we at
  // least don't return the empty string.
  base::string16 keyword(net::StripWWWFromHost(url));
  return keyword.empty() ? base::ASCIIToUTF16("www") : keyword;
}

// static
GURL TemplateURL::GenerateFaviconURL(const GURL& url) {
  DCHECK(url.is_valid());
  GURL::Replacements rep;

  const char favicon_path[] = "/favicon.ico";
  int favicon_path_len = arraysize(favicon_path) - 1;

  rep.SetPath(favicon_path, url::Component(0, favicon_path_len));
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return url.ReplaceComponents(rep);
}

// static
bool TemplateURL::MatchesData(const TemplateURL* t_url,
                              const TemplateURLData* data,
                              const SearchTermsData& search_terms_data) {
  if (!t_url || !data)
    return !t_url && !data;

  return (t_url->short_name() == data->short_name) &&
      t_url->HasSameKeywordAs(*data, search_terms_data) &&
      (t_url->url() == data->url()) &&
      (t_url->suggestions_url() == data->suggestions_url) &&
      (t_url->instant_url() == data->instant_url) &&
      (t_url->image_url() == data->image_url) &&
      (t_url->new_tab_url() == data->new_tab_url) &&
      (t_url->search_url_post_params() == data->search_url_post_params) &&
      (t_url->suggestions_url_post_params() ==
          data->suggestions_url_post_params) &&
      (t_url->instant_url_post_params() == data->instant_url_post_params) &&
      (t_url->image_url_post_params() == data->image_url_post_params) &&
      (t_url->favicon_url() == data->favicon_url) &&
      (t_url->safe_for_autoreplace() == data->safe_for_autoreplace) &&
      (t_url->show_in_default_list() == data->show_in_default_list) &&
      (t_url->input_encodings() == data->input_encodings) &&
      (t_url->alternate_urls() == data->alternate_urls) &&
      (t_url->search_terms_replacement_key() ==
          data->search_terms_replacement_key);
}

base::string16 TemplateURL::AdjustedShortNameForLocaleDirection() const {
  base::string16 bidi_safe_short_name = data_.short_name;
  base::i18n::AdjustStringForLocaleDirection(&bidi_safe_short_name);
  return bidi_safe_short_name;
}

bool TemplateURL::ShowInDefaultList(
    const SearchTermsData& search_terms_data) const {
  return data_.show_in_default_list &&
      url_ref_.SupportsReplacement(search_terms_data);
}

bool TemplateURL::SupportsReplacement(
    const SearchTermsData& search_terms_data) const {
  return url_ref_.SupportsReplacement(search_terms_data);
}

bool TemplateURL::HasGoogleBaseURLs(
    const SearchTermsData& search_terms_data) const {
  return url_ref_.HasGoogleBaseURLs(search_terms_data) ||
      suggestions_url_ref_.HasGoogleBaseURLs(search_terms_data) ||
      instant_url_ref_.HasGoogleBaseURLs(search_terms_data) ||
      image_url_ref_.HasGoogleBaseURLs(search_terms_data) ||
      new_tab_url_ref_.HasGoogleBaseURLs(search_terms_data);
}

bool TemplateURL::IsGoogleSearchURLWithReplaceableKeyword(
    const SearchTermsData& search_terms_data) const {
  return (GetType() == NORMAL) &&
      url_ref_.HasGoogleBaseURLs(search_terms_data) &&
      google_util::IsGoogleHostname(base::UTF16ToUTF8(data_.keyword()),
                                    google_util::DISALLOW_SUBDOMAIN);
}

bool TemplateURL::HasSameKeywordAs(
    const TemplateURLData& other,
    const SearchTermsData& search_terms_data) const {
  return (data_.keyword() == other.keyword()) ||
      (IsGoogleSearchURLWithReplaceableKeyword(search_terms_data) &&
       TemplateURL(other).IsGoogleSearchURLWithReplaceableKeyword(
           search_terms_data));
}

TemplateURL::Type TemplateURL::GetType() const {
  return extension_info_ ? extension_info_->type : NORMAL;
}

std::string TemplateURL::GetExtensionId() const {
  DCHECK(extension_info_);
  return extension_info_->extension_id;
}

size_t TemplateURL::URLCount() const {
  // Add 1 for the regular search URL.
  return data_.alternate_urls.size() + 1;
}

const std::string& TemplateURL::GetURL(size_t index) const {
  DCHECK_LT(index, URLCount());

  return (index < data_.alternate_urls.size()) ?
      data_.alternate_urls[index] : url();
}

bool TemplateURL::ExtractSearchTermsFromURL(
    const GURL& url,
    const SearchTermsData& search_terms_data,
    base::string16* search_terms) {
  return FindSearchTermsInURL(url, search_terms_data, search_terms, NULL, NULL);
}

bool TemplateURL::IsSearchURL(
    const GURL& url,
    const SearchTermsData& search_terms_data) {
  base::string16 search_terms;
  return ExtractSearchTermsFromURL(url, search_terms_data, &search_terms) &&
      !search_terms.empty();
}

bool TemplateURL::HasSearchTermsReplacementKey(const GURL& url) const {
  // Look for the key both in the query and the ref.
  std::string params[] = {url.query(), url.ref()};

  for (int i = 0; i < 2; ++i) {
    url::Component query, key, value;
    query.len = static_cast<int>(params[i].size());
    while (url::ExtractQueryKeyValue(params[i].c_str(), &query, &key, &value)) {
      if (key.is_nonempty() &&
          params[i].substr(key.begin, key.len) ==
              search_terms_replacement_key()) {
        return true;
      }
    }
  }
  return false;
}

bool TemplateURL::ReplaceSearchTermsInURL(
    const GURL& url,
    const TemplateURLRef::SearchTermsArgs& search_terms_args,
    const SearchTermsData& search_terms_data,
    GURL* result) {
  // TODO(beaudoin): Use AQS from |search_terms_args| too.
  url::Parsed::ComponentType search_term_component;
  url::Component search_terms_position;
  base::string16 search_terms;
  if (!FindSearchTermsInURL(url, search_terms_data, &search_terms,
                            &search_term_component, &search_terms_position)) {
    return false;
  }
  DCHECK(search_terms_position.is_nonempty());

  // FindSearchTermsInURL only returns true for search terms in the query or
  // ref, so we can call EncodeSearchTerm with |is_in_query| = true, since query
  // and ref are encoded in the same way.
  std::string input_encoding;
  base::string16 encoded_terms;
  base::string16 encoded_original_query;
  EncodeSearchTerms(search_terms_args, true, &input_encoding,
                    &encoded_terms, &encoded_original_query);

  std::string old_params(
      (search_term_component == url::Parsed::REF) ? url.ref() : url.query());
  std::string new_params(old_params, 0, search_terms_position.begin);
  new_params += base::UTF16ToUTF8(search_terms_args.search_terms);
  new_params += old_params.substr(search_terms_position.end());
  url::StdStringReplacements<std::string> replacements;
  if (search_term_component == url::Parsed::REF)
    replacements.SetRefStr(new_params);
  else
    replacements.SetQueryStr(new_params);
  *result = url.ReplaceComponents(replacements);
  return true;
}

void TemplateURL::EncodeSearchTerms(
    const TemplateURLRef::SearchTermsArgs& search_terms_args,
    bool is_in_query,
    std::string* input_encoding,
    base::string16* encoded_terms,
    base::string16* encoded_original_query) const {

  std::vector<std::string> encodings(input_encodings());
  if (std::find(encodings.begin(), encodings.end(), "UTF-8") == encodings.end())
    encodings.push_back("UTF-8");
  for (std::vector<std::string>::const_iterator i(encodings.begin());
       i != encodings.end(); ++i) {
    if (TryEncoding(search_terms_args.search_terms,
                    search_terms_args.original_query, i->c_str(),
                    is_in_query, encoded_terms, encoded_original_query)) {
      *input_encoding = *i;
      return;
    }
  }
  NOTREACHED();
}

GURL TemplateURL::GenerateSearchURL(
    const SearchTermsData& search_terms_data) const {
  if (!url_ref_.IsValid(search_terms_data))
    return GURL();

  if (!url_ref_.SupportsReplacement(search_terms_data))
    return GURL(url());

  // Use something obscure for the search terms argument so that in the rare
  // case the term replaces the URL it's unlikely another keyword would have the
  // same url.
  // TODO(jnd): Add additional parameters to get post data when the search URL
  // has post parameters.
  return GURL(url_ref_.ReplaceSearchTerms(
      TemplateURLRef::SearchTermsArgs(
          base::ASCIIToUTF16("blah.blah.blah.blah.blah")),
      search_terms_data, NULL));
}

void TemplateURL::CopyFrom(const TemplateURL& other) {
  if (this == &other)
    return;

  data_ = other.data_;
  url_ref_.InvalidateCachedValues();
  suggestions_url_ref_.InvalidateCachedValues();
  instant_url_ref_.InvalidateCachedValues();
  SetPrepopulateId(other.data_.prepopulate_id);
}

void TemplateURL::SetURL(const std::string& url) {
  data_.SetURL(url);
  url_ref_.InvalidateCachedValues();
}

void TemplateURL::SetPrepopulateId(int id) {
  data_.prepopulate_id = id;
  const bool prepopulated = id > 0;
  url_ref_.prepopulated_ = prepopulated;
  suggestions_url_ref_.prepopulated_ = prepopulated;
  instant_url_ref_.prepopulated_ = prepopulated;
}

void TemplateURL::ResetKeywordIfNecessary(
    const SearchTermsData& search_terms_data,
    bool force) {
  if (IsGoogleSearchURLWithReplaceableKeyword(search_terms_data) || force) {
    DCHECK(GetType() != OMNIBOX_API_EXTENSION);
    GURL url(GenerateSearchURL(search_terms_data));
    if (url.is_valid())
      data_.SetKeyword(GenerateKeyword(url));
  }
}

bool TemplateURL::FindSearchTermsInURL(
    const GURL& url,
    const SearchTermsData& search_terms_data,
    base::string16* search_terms,
    url::Parsed::ComponentType* search_term_component,
    url::Component* search_terms_position) {
  DCHECK(search_terms);
  search_terms->clear();

  // Try to match with every pattern.
  for (size_t i = 0; i < URLCount(); ++i) {
    TemplateURLRef ref(this, i);
    if (ref.ExtractSearchTermsFromURL(url, search_terms, search_terms_data,
        search_term_component, search_terms_position)) {
      // If ExtractSearchTermsFromURL() returns true and |search_terms| is empty
      // it means the pattern matched but no search terms were present. In this
      // case we fail immediately without looking for matches in subsequent
      // patterns. This means that given patterns
      //    [ "http://foo/#q={searchTerms}", "http://foo/?q={searchTerms}" ],
      // calling ExtractSearchTermsFromURL() on "http://foo/?q=bar#q=' would
      // return false. This is important for at least Google, where such URLs
      // are invalid.
      return !search_terms->empty();
    }
  }
  return false;
}
