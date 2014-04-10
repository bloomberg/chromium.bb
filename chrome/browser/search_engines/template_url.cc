// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
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
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/mime_util.h"
#include "ui/base/l10n/l10n_util.h"

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
const char kGoogleCurrentPageUrlParameter[] = "google:currentPageUrl";
const char kGoogleCursorPositionParameter[] = "google:cursorPosition";
const char kGoogleForceInstantResultsParameter[] = "google:forceInstantResults";
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
const char kGoogleRLZParameter[] = "google:RLZ";
const char kGoogleSearchClient[] = "google:searchClient";
const char kGoogleSearchFieldtrialParameter[] =
    "google:searchFieldtrialParameter";
const char kGoogleSourceIdParameter[] = "google:sourceId";
const char kGoogleSuggestAPIKeyParameter[] = "google:suggestAPIKeyParameter";
const char kGoogleSuggestClient[] = "google:suggestClient";
const char kGoogleSuggestRequestId[] = "google:suggestRid";

// Same as kSearchTermsParameter, with no escaping.
const char kGoogleUnescapedSearchTermsParameter[] =
    "google:unescapedSearchTerms";
const char kGoogleUnescapedSearchTermsParameterFull[] =
    "{google:unescapedSearchTerms}";

const char kGoogleImageSearchSource[] = "google:imageSearchSource";
const char kGoogleImageThumbnailParameter[] = "google:imageThumbnail";
const char kGoogleImageURLParameter[] = "google:imageURL";
const char kGoogleImageOriginalWidth[] = "google:imageOriginalWidth";
const char kGoogleImageOriginalHeight[] = "google:imageOriginalHeight";

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
  url_parse::Component query, key, value;
  query.len = static_cast<int>(params.size());
  while (url_parse::ExtractQueryKeyValue(params.c_str(), &query, &key,
                                         &value)) {
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

// Returns the string to use for replacements of type
// GOOGLE_IMAGE_SEARCH_SOURCE.
std::string GetGoogleImageSearchSource() {
  chrome::VersionInfo version_info;
  if (version_info.is_valid()) {
    std::string version(version_info.Name() + " " + version_info.Version());
    if (version_info.IsOfficialBuild())
      version += " (Official)";
    version += " " + version_info.OSType();
    std::string modifier(version_info.GetVersionStringModifier());
    if (!modifier.empty())
      version += " " + modifier;
    return version;
  }
  return "unknown";
}

bool IsTemplateParameterString(const std::string& param) {
  return (param.length() > 2) && (*(param.begin()) == kStartParameter) &&
      (*(param.rbegin()) == kEndParameter);
}

bool ShowingSearchTermsOnSRP() {
  return chrome::IsInstantExtendedAPIEnabled() &&
      chrome::IsQueryExtractionEnabled();
}

}  // namespace


// TemplateURLRef::SearchTermsArgs --------------------------------------------

TemplateURLRef::SearchTermsArgs::SearchTermsArgs(
    const base::string16& search_terms)
    : search_terms(search_terms),
      accepted_suggestion(NO_SUGGESTIONS_AVAILABLE),
      cursor_position(base::string16::npos),
      omnibox_start_margin(-1),
      page_classification(AutocompleteInput::INVALID_SPEC),
      bookmark_bar_pinned(false),
      append_extra_query_params(false),
      force_instant_results(false),
      from_app_list(false) {
}

TemplateURLRef::SearchTermsArgs::~SearchTermsArgs() {
}


// TemplateURLRef -------------------------------------------------------------

TemplateURLRef::TemplateURLRef(TemplateURL* owner, Type type)
    : owner_(owner),
      type_(type),
      index_in_owner_(-1),
      parsed_(false),
      valid_(false),
      supports_replacements_(false),
      search_term_key_location_(url_parse::Parsed::QUERY),
      prepopulated_(false),
      showing_search_terms_(ShowingSearchTermsOnSRP()) {
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
      search_term_key_location_(url_parse::Parsed::QUERY),
      prepopulated_(false),
      showing_search_terms_(ShowingSearchTermsOnSRP()) {
  DCHECK(owner_);
  DCHECK_LT(index_in_owner_, owner_->URLCount());
}

TemplateURLRef::~TemplateURLRef() {
}

std::string TemplateURLRef::GetURL() const {
  switch (type_) {
    case SEARCH:  return owner_->url();
    case SUGGEST: return owner_->suggestions_url();
    case INSTANT: return owner_->instant_url();
    case IMAGE:   return owner_->image_url();
    case NEW_TAB: return owner_->new_tab_url();
    case INDEXED: return owner_->GetURL(index_in_owner_);
    default:      NOTREACHED(); return std::string();  // NOLINT
  }
}

std::string TemplateURLRef::GetPostParamsString() const {
  switch (type_) {
    case INDEXED:
    case SEARCH:  return owner_->search_url_post_params();
    case SUGGEST: return owner_->suggestions_url_post_params();
    case INSTANT: return owner_->instant_url_post_params();
    case NEW_TAB: return std::string();
    case IMAGE:   return owner_->image_url_post_params();
    default:      NOTREACHED(); return std::string();  // NOLINT
  }
}

bool TemplateURLRef::UsesPOSTMethodUsingTermsData(
    const SearchTermsData* search_terms_data) const {
  if (search_terms_data)
    ParseIfNecessaryUsingTermsData(*search_terms_data);
  else
    ParseIfNecessary();
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

bool TemplateURLRef::SupportsReplacement() const {
  UIThreadSearchTermsData search_terms_data(owner_->profile());
  return SupportsReplacementUsingTermsData(search_terms_data);
}

bool TemplateURLRef::SupportsReplacementUsingTermsData(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessaryUsingTermsData(search_terms_data);
  return valid_ && supports_replacements_;
}

std::string TemplateURLRef::ReplaceSearchTerms(
    const SearchTermsArgs& search_terms_args,
    PostContent* post_content) const {
  UIThreadSearchTermsData search_terms_data(owner_->profile());
  return ReplaceSearchTermsUsingTermsData(search_terms_args, search_terms_data,
                                          post_content);
}

std::string TemplateURLRef::ReplaceSearchTermsUsingTermsData(
    const SearchTermsArgs& search_terms_args,
    const SearchTermsData& search_terms_data,
    PostContent* post_content) const {
  ParseIfNecessaryUsingTermsData(search_terms_data);
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

bool TemplateURLRef::IsValid() const {
  UIThreadSearchTermsData search_terms_data(owner_->profile());
  return IsValidUsingTermsData(search_terms_data);
}

bool TemplateURLRef::IsValidUsingTermsData(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessaryUsingTermsData(search_terms_data);
  return valid_;
}

base::string16 TemplateURLRef::DisplayURL() const {
  ParseIfNecessary();
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

const std::string& TemplateURLRef::GetHost() const {
  ParseIfNecessary();
  return host_;
}

const std::string& TemplateURLRef::GetPath() const {
  ParseIfNecessary();
  return path_;
}

const std::string& TemplateURLRef::GetSearchTermKey() const {
  ParseIfNecessary();
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

bool TemplateURLRef::HasGoogleBaseURLs() const {
  ParseIfNecessary();
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
    url_parse::Parsed::ComponentType* search_terms_component,
    url_parse::Component* search_terms_position) const {
  DCHECK(search_terms);
  search_terms->clear();

  ParseIfNecessaryUsingTermsData(search_terms_data);

  // We need a search term in the template URL to extract something.
  if (search_term_key_.empty())
    return false;

  // TODO(beaudoin): Support patterns of the form http://foo/{searchTerms}/
  // See crbug.com/153798

  // Fill-in the replacements. We don't care about search terms in the pattern,
  // so we use the empty string.
  // Currently we assume the search term only shows in URL, not in post params.
  GURL pattern(ReplaceSearchTermsUsingTermsData(
      SearchTermsArgs(base::string16()), search_terms_data, NULL));
  // Host, path and port must match.
  if (url.port() != pattern.port() ||
      url.host() != host_ ||
      url.path() != path_) {
    return false;
  }

  // Parameter must be present either in the query or the ref.
  const std::string& params(
      (search_term_key_location_ == url_parse::Parsed::QUERY) ?
          url.query() : url.ref());

  url_parse::Component query, key, value;
  query.len = static_cast<int>(params.size());
  bool key_found = false;
  while (url_parse::ExtractQueryKeyValue(params.c_str(), &query, &key,
                                         &value)) {
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
                net::UnescapeRule::REPLACE_PLUS_WITH_SPACE,
            NULL);
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
  } else if (parameter == kGoogleImageOriginalHeight) {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_ORIGINAL_HEIGHT, start));
  } else if (parameter == kGoogleImageOriginalWidth) {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_ORIGINAL_WIDTH, start));
  } else if (parameter == kGoogleImageSearchSource) {
    url->insert(start, GetGoogleImageSearchSource());
  } else if (parameter == kGoogleImageThumbnailParameter) {
    replacements->push_back(
        Replacement(TemplateURLRef::GOOGLE_IMAGE_THUMBNAIL, start));
  } else if (parameter == kGoogleImageURLParameter) {
    replacements->push_back(Replacement(TemplateURLRef::GOOGLE_IMAGE_URL,
                                        start));
  } else if (parameter == kGoogleForceInstantResultsParameter) {
    replacements->push_back(Replacement(GOOGLE_FORCE_INSTANT_RESULTS, start));
  } else if (parameter == kGoogleInstantExtendedEnabledParameter) {
    replacements->push_back(Replacement(GOOGLE_INSTANT_EXTENDED_ENABLED,
                                        start));
  } else if (parameter == kGoogleInstantExtendedEnabledKey) {
    url->insert(start, google_util::kInstantExtendedAPIParam);
  } else if (parameter == kGoogleNTPIsThemedParameter) {
    replacements->push_back(Replacement(GOOGLE_NTP_IS_THEMED, start));
  } else if (parameter == kGoogleOmniboxStartMarginParameter) {
    replacements->push_back(Replacement(GOOGLE_OMNIBOX_START_MARGIN, start));
  } else if (parameter == kGoogleOriginalQueryForSuggestionParameter) {
    replacements->push_back(Replacement(GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
                                        start));
  } else if (parameter == kGooglePageClassificationParameter) {
    replacements->push_back(Replacement(GOOGLE_PAGE_CLASSIFICATION, start));
  } else if (parameter == kGoogleRLZParameter) {
    replacements->push_back(Replacement(GOOGLE_RLZ, start));
  } else if (parameter == kGoogleSearchClient) {
    replacements->push_back(Replacement(GOOGLE_SEARCH_CLIENT, start));
  } else if (parameter == kGoogleSearchFieldtrialParameter) {
    replacements->push_back(Replacement(GOOGLE_SEARCH_FIELDTRIAL_GROUP, start));
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

void TemplateURLRef::ParseIfNecessary() const {
  UIThreadSearchTermsData search_terms_data(owner_->profile());
  ParseIfNecessaryUsingTermsData(search_terms_data);
}

void TemplateURLRef::ParseIfNecessaryUsingTermsData(
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
  search_term_key_location_ = url_parse::Parsed::REF;

  GURL url(url_string);
  if (!url.is_valid())
    return;

  std::string query_key = FindSearchTermsKey(url.query());
  std::string ref_key = FindSearchTermsKey(url.ref());
  if (query_key.empty() == ref_key.empty())
    return;  // No key or multiple keys found.  We only handle having one key.
  search_term_key_ = query_key.empty() ? ref_key : query_key;
  search_term_key_location_ = query_key.empty() ?
      url_parse::Parsed::REF : url_parse::Parsed::QUERY;
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
          GURL base_url(ReplaceSearchTermsUsingTermsData(
              search_terms_args_without_aqs, search_terms_data, NULL));
          if (base_url.SchemeIs(content::kHttpsScheme)) {
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
        if (showing_search_terms_) {
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
                          chrome::ForceInstantResultsParam(
                              search_terms_args.force_instant_results),
                          *i,
                          &url);
        break;

      case GOOGLE_INSTANT_EXTENDED_ENABLED:
        DCHECK(!i->is_post_param);
        HandleReplacement(std::string(),
                          chrome::InstantExtendedEnabledParam(type_ == SEARCH),
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
        if (search_terms_args.omnibox_start_margin >= 0) {
          HandleReplacement(
              "es_sm",
              base::IntToString(search_terms_args.omnibox_start_margin),
              *i,
              &url);
        }
        break;

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
            AutocompleteInput::INVALID_SPEC) {
          HandleReplacement(
              "pgcl", base::IntToString(search_terms_args.page_classification),
              *i, &url);
        }
        break;

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

      default:
        NOTREACHED();
        break;
    }
  }

  if (!post_params_.empty())
    EncodeFormData(post_params_, post_content);

  return url;
}


// TemplateURLData ------------------------------------------------------------

TemplateURLData::TemplateURLData()
    : show_in_default_list(false),
      safe_for_autoreplace(false),
      id(0),
      date_created(base::Time::Now()),
      last_modified(base::Time::Now()),
      created_by_policy(false),
      usage_count(0),
      prepopulate_id(0),
      sync_guid(base::GenerateGUID()),
      keyword_(base::ASCIIToUTF16("dummy")),
      url_("x") {
}

TemplateURLData::~TemplateURLData() {
}

void TemplateURLData::SetKeyword(const base::string16& keyword) {
  DCHECK(!keyword.empty());

  // Case sensitive keyword matching is confusing. As such, we force all
  // keywords to be lower case.
  keyword_ = base::i18n::ToLower(keyword);
}

void TemplateURLData::SetURL(const std::string& url) {
  DCHECK(!url.empty());
  url_ = url;
}


// TemplateURL ----------------------------------------------------------------

TemplateURL::TemplateURL(Profile* profile, const TemplateURLData& data)
    : profile_(profile),
      data_(data),
      url_ref_(this, TemplateURLRef::SEARCH),
      suggestions_url_ref_(this,
                           TemplateURLRef::SUGGEST),
      instant_url_ref_(this,
                       TemplateURLRef::INSTANT),
      image_url_ref_(this, TemplateURLRef::IMAGE),
      new_tab_url_ref_(this, TemplateURLRef::NEW_TAB) {
  SetPrepopulateId(data_.prepopulate_id);

  if (data_.search_terms_replacement_key ==
      kGoogleInstantExtendedEnabledKeyFull) {
    data_.search_terms_replacement_key = google_util::kInstantExtendedAPIParam;
  }
}

TemplateURL::~TemplateURL() {
}

// static
GURL TemplateURL::GenerateFaviconURL(const GURL& url) {
  DCHECK(url.is_valid());
  GURL::Replacements rep;

  const char favicon_path[] = "/favicon.ico";
  int favicon_path_len = arraysize(favicon_path) - 1;

  rep.SetPath(favicon_path, url_parse::Component(0, favicon_path_len));
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return url.ReplaceComponents(rep);
}

base::string16 TemplateURL::AdjustedShortNameForLocaleDirection() const {
  base::string16 bidi_safe_short_name = data_.short_name;
  base::i18n::AdjustStringForLocaleDirection(&bidi_safe_short_name);
  return bidi_safe_short_name;
}

bool TemplateURL::ShowInDefaultList() const {
  return data_.show_in_default_list && url_ref_.SupportsReplacement();
}

bool TemplateURL::SupportsReplacement() const {
  UIThreadSearchTermsData search_terms_data(profile_);
  return SupportsReplacementUsingTermsData(search_terms_data);
}

bool TemplateURL::SupportsReplacementUsingTermsData(
    const SearchTermsData& search_terms_data) const {
  return url_ref_.SupportsReplacementUsingTermsData(search_terms_data);
}

bool TemplateURL::IsGoogleSearchURLWithReplaceableKeyword() const {
  return (GetType() == NORMAL) && url_ref_.HasGoogleBaseURLs() &&
      google_util::IsGoogleHostname(base::UTF16ToUTF8(data_.keyword()),
                                    google_util::DISALLOW_SUBDOMAIN);
}

bool TemplateURL::HasSameKeywordAs(const TemplateURL& other) const {
  return (data_.keyword() == other.data_.keyword()) ||
      (IsGoogleSearchURLWithReplaceableKeyword() &&
       other.IsGoogleSearchURLWithReplaceableKeyword());
}

TemplateURL::Type TemplateURL::GetType() const {
  if (extension_info_)
    return NORMAL_CONTROLLED_BY_EXTENSION;
  return GURL(data_.url()).SchemeIs(extensions::kExtensionScheme) ?
      OMNIBOX_API_EXTENSION : NORMAL;
}

std::string TemplateURL::GetExtensionId() const {
  DCHECK_NE(NORMAL, GetType());
  return extension_info_ ?
      extension_info_->extension_id : GURL(data_.url()).host();
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
    base::string16* search_terms) {
  UIThreadSearchTermsData search_terms_data(profile_);
  return ExtractSearchTermsFromURLUsingTermsData(url, search_terms,
                                                 search_terms_data);
}

bool TemplateURL::ExtractSearchTermsFromURLUsingTermsData(
    const GURL& url,
    base::string16* search_terms,
    const SearchTermsData& search_terms_data) {
  return FindSearchTermsInURL(url, search_terms_data, search_terms, NULL, NULL);
}


bool TemplateURL::IsSearchURL(const GURL& url) {
  UIThreadSearchTermsData search_terms_data(profile_);
  return IsSearchURLUsingTermsData(url, search_terms_data);
}

bool TemplateURL::IsSearchURLUsingTermsData(
    const GURL& url,
    const SearchTermsData& search_terms_data) {
  base::string16 search_terms;
  return ExtractSearchTermsFromURLUsingTermsData(
      url, &search_terms, search_terms_data) && !search_terms.empty();
}

bool TemplateURL::HasSearchTermsReplacementKey(const GURL& url) const {
  // Look for the key both in the query and the ref.
  std::string params[] = {url.query(), url.ref()};

  for (int i = 0; i < 2; ++i) {
    url_parse::Component query, key, value;
    query.len = static_cast<int>(params[i].size());
    while (url_parse::ExtractQueryKeyValue(params[i].c_str(), &query, &key,
                                           &value)) {
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
    GURL* result) {
  UIThreadSearchTermsData search_terms_data(profile_);
  // TODO(beaudoin): Use AQS from |search_terms_args| too.
  url_parse::Parsed::ComponentType search_term_component;
  url_parse::Component search_terms_position;
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

  std::string old_params((search_term_component == url_parse::Parsed::REF) ?
      url.ref() : url.query());
  std::string new_params(old_params, 0, search_terms_position.begin);
  new_params += base::UTF16ToUTF8(search_terms_args.search_terms);
  new_params += old_params.substr(search_terms_position.end());
  url_canon::StdStringReplacements<std::string> replacements;
  if (search_term_component == url_parse::Parsed::REF)
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

void TemplateURL::CopyFrom(const TemplateURL& other) {
  if (this == &other)
    return;

  profile_ = other.profile_;
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

void TemplateURL::ResetKeywordIfNecessary(bool force) {
  if (IsGoogleSearchURLWithReplaceableKeyword() || force) {
    DCHECK(GetType() != OMNIBOX_API_EXTENSION);
    GURL url(TemplateURLService::GenerateSearchURL(this));
    if (url.is_valid())
      data_.SetKeyword(TemplateURLService::GenerateKeyword(url));
  }
}

bool TemplateURL::FindSearchTermsInURL(
    const GURL& url,
    const SearchTermsData& search_terms_data,
    base::string16* search_terms,
    url_parse::Parsed::ComponentType* search_term_component,
    url_parse::Component* search_terms_position) {
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
