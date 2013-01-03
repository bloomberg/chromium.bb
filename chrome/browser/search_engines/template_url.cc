// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url.h"

#include "base/format_macros.h"
#include "base/guid.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
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

const char kGoogleAcceptedSuggestionParameter[] = "google:acceptedSuggestion";
const char kGoogleAssistedQueryStatsParameter[] = "google:assistedQueryStats";

// Host/Domain Google searches are relative to.
const char kGoogleBaseURLParameter[] = "google:baseURL";
const char kGoogleBaseURLParameterFull[] = "{google:baseURL}";

// Like google:baseURL, but for the Search Suggest capability.
const char kGoogleBaseSuggestURLParameter[] = "google:baseSuggestURL";
const char kGoogleBaseSuggestURLParameterFull[] = "{google:baseSuggestURL}";
const char kGoogleCursorPositionParameter[] = "google:cursorPosition";
const char kGoogleInstantEnabledParameter[] = "google:instantEnabledParameter";
const char kGoogleInstantExtendedEnabledParameter[] =
    "google:instantExtendedEnabledParameter";
const char kGoogleInstantExtendedEnabledKey[] =
    "google:instantExtendedEnabledKey";
const char kGoogleOriginalQueryForSuggestionParameter[] =
    "google:originalQueryForSuggestion";
const char kGoogleRLZParameter[] = "google:RLZ";
const char kGoogleSearchClient[] = "google:searchClient";
const char kGoogleSearchFieldtrialParameter[] =
    "google:searchFieldtrialParameter";
const char kGoogleSourceIdParameter[] = "google:sourceId";
const char kGoogleSuggestAPIKeyParameter[] = "google:suggestAPIKeyParameter";

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
bool TryEncoding(const string16& terms,
                 const string16& original_query,
                 const char* encoding,
                 bool is_in_query,
                 string16* escaped_terms,
                 string16* escaped_original_query) {
  DCHECK(escaped_terms);
  DCHECK(escaped_original_query);
  std::string encoded_terms;
  if (!base::UTF16ToCodepage(terms, encoding,
      base::OnStringConversionError::SKIP, &encoded_terms))
    return false;
  *escaped_terms = UTF8ToUTF16(is_in_query ?
      net::EscapeQueryParamValue(encoded_terms, true) :
      net::EscapePath(encoded_terms));
  if (original_query.empty())
    return true;
  std::string encoded_original_query;
  if (!base::UTF16ToCodepage(original_query, encoding,
      base::OnStringConversionError::SKIP, &encoded_original_query))
    return false;
  *escaped_original_query =
      UTF8ToUTF16(net::EscapeQueryParamValue(encoded_original_query, true));
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

}  // namespace


// TemplateURLRef::SearchTermsArgs --------------------------------------------

TemplateURLRef::SearchTermsArgs::SearchTermsArgs(const string16& search_terms)
    : search_terms(search_terms),
      accepted_suggestion(NO_SUGGESTIONS_AVAILABLE),
      cursor_position(string16::npos) {
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
      search_term_key_location_(url_parse::Parsed::QUERY),
      prepopulated_(false) {
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
    case INDEXED: return owner_->GetURL(index_in_owner_);
    default:      NOTREACHED(); return std::string();  // NOLINT
  }
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
    const SearchTermsArgs& search_terms_args) const {
  UIThreadSearchTermsData search_terms_data(owner_->profile());
  return ReplaceSearchTermsUsingTermsData(search_terms_args, search_terms_data);
}

std::string TemplateURLRef::ReplaceSearchTermsUsingTermsData(
    const SearchTermsArgs& search_terms_args,
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessaryUsingTermsData(search_terms_data);
  if (!valid_)
    return std::string();

  if (replacements_.empty())
    return parsed_url_;

  // Determine if the search terms are in the query or before. We're escaping
  // space as '+' in the former case and as '%20' in the latter case.
  bool is_in_query = true;
  for (Replacements::iterator i = replacements_.begin();
       i != replacements_.end(); ++i) {
    if (i->type == SEARCH_TERMS) {
      string16::size_type query_start = parsed_url_.find('?');
      is_in_query = query_start != string16::npos &&
          (static_cast<string16::size_type>(i->index) > query_start);
      break;
    }
  }

  string16 encoded_terms;
  string16 encoded_original_query;
  std::string input_encoding;
  // Encode the search terms so that we know the encoding.
  for (std::vector<std::string>::const_iterator i(
           owner_->input_encodings().begin());
       i != owner_->input_encodings().end(); ++i) {
    if (TryEncoding(search_terms_args.search_terms,
                    search_terms_args.original_query, i->c_str(),
                    is_in_query, &encoded_terms, &encoded_original_query)) {
      input_encoding = *i;
      break;
    }
  }
  if (input_encoding.empty()) {
    input_encoding = "UTF-8";
    if (!TryEncoding(search_terms_args.search_terms,
                     search_terms_args.original_query,
                     input_encoding.c_str(), is_in_query, &encoded_terms,
                     &encoded_original_query))
      NOTREACHED();
  }

  std::string url = parsed_url_;

  // replacements_ is ordered in ascending order, as such we need to iterate
  // from the back.
  for (Replacements::reverse_iterator i = replacements_.rbegin();
       i != replacements_.rend(); ++i) {
    switch (i->type) {
      case ENCODING:
        url.insert(i->index, input_encoding);
        break;

      case GOOGLE_ASSISTED_QUERY_STATS:
        if (!search_terms_args.assisted_query_stats.empty()) {
          // Get the base URL without substituting AQS to avoid infinite
          // recursion.  We need the URL to find out if it meets all
          // AQS requirements (e.g. HTTPS protocol check).
          // See TemplateURLRef::SearchTermsArgs for more details.
          SearchTermsArgs search_terms_args_without_aqs(search_terms_args);
          search_terms_args_without_aqs.assisted_query_stats.clear();
          GURL base_url(ReplaceSearchTermsUsingTermsData(
              search_terms_args_without_aqs, search_terms_data));
          if (base_url.SchemeIs(chrome::kHttpsScheme)) {
            url.insert(i->index,
                       "aqs=" + search_terms_args.assisted_query_stats + "&");
          }
        }
        break;

      case GOOGLE_ACCEPTED_SUGGESTION: {
        std::string value("f");
        if (search_terms_args.accepted_suggestion >= 0)
          value = base::IntToString(search_terms_args.accepted_suggestion);
        url.insert(i->index, "aq=" + value + "&");
        break;
      }

      case GOOGLE_BASE_URL:
        url.insert(i->index, search_terms_data.GoogleBaseURLValue());
        break;

      case GOOGLE_BASE_SUGGEST_URL:
        url.insert(i->index, search_terms_data.GoogleBaseSuggestURLValue());
        break;

      case GOOGLE_CURSOR_POSITION:
        if (search_terms_args.cursor_position != string16::npos)
          url.insert(i->index,
                     base::StringPrintf("cp=%" PRIuS "&",
                                        search_terms_args.cursor_position));
        break;

      case GOOGLE_INSTANT_ENABLED:
        url.insert(i->index, search_terms_data.InstantEnabledParam());
        break;

      case GOOGLE_INSTANT_EXTENDED_ENABLED:
        url.insert(i->index, search_terms_data.InstantExtendedEnabledParam());
        break;

      case GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION:
        if (search_terms_args.accepted_suggestion >= 0 ||
            !search_terms_args.assisted_query_stats.empty()) {
          url.insert(i->index, "oq=" + UTF16ToUTF8(encoded_original_query) +
                               "&");
        }
        break;

      case GOOGLE_RLZ: {
        // On platforms that don't have RLZ, we still want this branch
        // to happen so that we replace the RLZ template with the
        // empty string.  (If we don't handle this case, we hit a
        // NOTREACHED below.)
        string16 rlz_string = search_terms_data.GetRlzParameterValue();
        if (!rlz_string.empty()) {
          url.insert(i->index, "rlz=" + UTF16ToUTF8(rlz_string) + "&");
        }
        break;
      }

      case GOOGLE_SEARCH_CLIENT: {
        std::string client = search_terms_data.GetSearchClient();
        if (!client.empty())
          url.insert(i->index, "client=" + client + "&");
        break;
      }

      case GOOGLE_SEARCH_FIELDTRIAL_GROUP:
        // We are not currently running any fieldtrials that modulate the search
        // url.  If we do, then we'd have some conditional insert such as:
        // url.insert(i->index, used_www ? "gcx=w&" : "gcx=c&");
        break;

      case GOOGLE_UNESCAPED_SEARCH_TERMS: {
        std::string unescaped_terms;
        base::UTF16ToCodepage(search_terms_args.search_terms,
                              input_encoding.c_str(),
                              base::OnStringConversionError::SKIP,
                              &unescaped_terms);
        url.insert(i->index, std::string(unescaped_terms.begin(),
                                         unescaped_terms.end()));
        break;
      }

      case LANGUAGE:
        url.insert(i->index, search_terms_data.GetApplicationLocale());
        break;

      case SEARCH_TERMS:
        url.insert(i->index, UTF16ToUTF8(encoded_terms));
        break;

      default:
        NOTREACHED();
        break;
    }
  }

  return url;
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

string16 TemplateURLRef::DisplayURL() const {
  ParseIfNecessary();
  string16 result(UTF8ToUTF16(GetURL()));
  if (valid_ && !replacements_.empty()) {
    ReplaceSubstringsAfterOffset(&result, 0,
                                 ASCIIToUTF16(kSearchTermsParameterFull),
                                 ASCIIToUTF16(kDisplaySearchTerms));
    ReplaceSubstringsAfterOffset(&result, 0,
        ASCIIToUTF16(kGoogleUnescapedSearchTermsParameterFull),
        ASCIIToUTF16(kDisplayUnescapedSearchTerms));
  }
  return result;
}

// static
std::string TemplateURLRef::DisplayURLToURLRef(
    const string16& display_url) {
  string16 result = display_url;
  ReplaceSubstringsAfterOffset(&result, 0, ASCIIToUTF16(kDisplaySearchTerms),
                               ASCIIToUTF16(kSearchTermsParameterFull));
  ReplaceSubstringsAfterOffset(
      &result, 0,
      ASCIIToUTF16(kDisplayUnescapedSearchTerms),
      ASCIIToUTF16(kGoogleUnescapedSearchTermsParameterFull));
  return UTF16ToUTF8(result);
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

string16 TemplateURLRef::SearchTermToString16(const std::string& term) const {
  const std::vector<std::string>& encodings = owner_->input_encodings();
  string16 result;

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
  result = UTF8ToUTF16(term);
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

bool TemplateURLRef::ExtractSearchTermsFromURL(const GURL& url,
                                               string16* search_terms) const {
  DCHECK(search_terms);
  search_terms->clear();

  ParseIfNecessary();

  // We need a search term in the template URL to extract something.
  if (search_term_key_.empty())
    return false;

  // TODO(beaudoin): Support patterns of the form http://foo/{searchTerms}/
  // See crbug.com/153798

  // Fill-in the replacements. We don't care about search terms in the pattern,
  // so we use the empty string.
  GURL pattern(ReplaceSearchTerms(SearchTermsArgs(string16())));
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
  while (url_parse::ExtractQueryKeyValue(params.c_str(), &query, &key,
                                         &value)) {
    if (key.is_nonempty()) {
      if (params.substr(key.begin, key.len) == search_term_key_) {
        // Extract the search term.
        *search_terms = net::UnescapeAndDecodeUTF8URLComponent(
            params.substr(value.begin, value.len),
            net::UnescapeRule::SPACES |
                net::UnescapeRule::URL_SPECIAL_CHARS |
                net::UnescapeRule::REPLACE_PLUS_WITH_SPACE,
            NULL);
        return true;
      }
    }
  }
  return false;
}

void TemplateURLRef::InvalidateCachedValues() const {
  supports_replacements_ = valid_ = parsed_ = false;
  host_.clear();
  path_.clear();
  search_term_key_.clear();
  replacements_.clear();
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
  } else if ((parameter == kStartIndexParameter) ||
             (parameter == kStartPageParameter)) {
    // We don't support these.
    if (!optional)
      url->insert(start, "1");
  } else if (parameter == kLanguageParameter) {
    replacements->push_back(Replacement(LANGUAGE, start));
  } else if (parameter == kInputEncodingParameter) {
    replacements->push_back(Replacement(ENCODING, start));
  } else if (parameter == kOutputEncodingParameter) {
    if (!optional)
      url->insert(start, kOutputEncodingType);
  } else if (parameter == kGoogleAcceptedSuggestionParameter) {
    replacements->push_back(Replacement(GOOGLE_ACCEPTED_SUGGESTION, start));
  } else if (parameter == kGoogleAssistedQueryStatsParameter) {
    replacements->push_back(Replacement(GOOGLE_ASSISTED_QUERY_STATS, start));
  } else if (parameter == kGoogleBaseURLParameter) {
    replacements->push_back(Replacement(GOOGLE_BASE_URL, start));
  } else if (parameter == kGoogleBaseSuggestURLParameter) {
    replacements->push_back(Replacement(GOOGLE_BASE_SUGGEST_URL, start));
  } else if (parameter == kGoogleCursorPositionParameter) {
    replacements->push_back(Replacement(GOOGLE_CURSOR_POSITION, start));
  } else if (parameter == kGoogleInstantEnabledParameter) {
    replacements->push_back(Replacement(GOOGLE_INSTANT_ENABLED, start));
  } else if (parameter == kGoogleInstantExtendedEnabledParameter) {
    replacements->push_back(Replacement(GOOGLE_INSTANT_EXTENDED_ENABLED,
                                        start));
  } else if (parameter == kGoogleInstantExtendedEnabledKey) {
    url->insert(start, google_util::kInstantExtendedAPIParam);
  } else if (parameter == kGoogleOriginalQueryForSuggestionParameter) {
    replacements->push_back(Replacement(GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
                                        start));
  } else if (parameter == kGoogleRLZParameter) {
    replacements->push_back(Replacement(GOOGLE_RLZ, start));
  } else if (parameter == kGoogleSearchClient) {
    replacements->push_back(Replacement(GOOGLE_SEARCH_CLIENT, start));
  } else if (parameter == kGoogleSearchFieldtrialParameter) {
    replacements->push_back(Replacement(GOOGLE_SEARCH_FIELDTRIAL_GROUP, start));
  } else if (parameter == kGoogleSuggestAPIKeyParameter) {
    url->insert(start,
                net::EscapeQueryParamValue(google_apis::GetAPIKey(), false));
  } else if (parameter == kGoogleSourceIdParameter) {
#if defined(OS_ANDROID)
    url->insert(start, "sourceid=chrome-mobile&");
#else
    url->insert(start, "sourceid=chrome&");
#endif
  } else if (parameter == kGoogleUnescapedSearchTermsParameter) {
    replacements->push_back(Replacement(GOOGLE_UNESCAPED_SEARCH_TERMS, start));
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
    parsed_ = true;
    parsed_url_ = ParseURL(GetURL(), &replacements_, &valid_);
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
      keyword_(ASCIIToUTF16("dummy")),
      url_("x") {
}

TemplateURLData::~TemplateURLData() {
}

void TemplateURLData::SetKeyword(const string16& keyword) {
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
      url_ref_(ALLOW_THIS_IN_INITIALIZER_LIST(this), TemplateURLRef::SEARCH),
      suggestions_url_ref_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                           TemplateURLRef::SUGGEST),
      instant_url_ref_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                       TemplateURLRef::INSTANT) {
  SetPrepopulateId(data_.prepopulate_id);
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

string16 TemplateURL::AdjustedShortNameForLocaleDirection() const {
  string16 bidi_safe_short_name = data_.short_name;
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
  return !IsExtensionKeyword() && url_ref_.HasGoogleBaseURLs() &&
      google_util::IsGoogleHostname(UTF16ToUTF8(data_.keyword()),
                                    google_util::DISALLOW_SUBDOMAIN);
}

bool TemplateURL::HasSameKeywordAs(const TemplateURL& other) const {
  return (data_.keyword() == other.data_.keyword()) ||
      (IsGoogleSearchURLWithReplaceableKeyword() &&
       other.IsGoogleSearchURLWithReplaceableKeyword());
}

std::string TemplateURL::GetExtensionId() const {
  DCHECK(IsExtensionKeyword());
  return GURL(data_.url()).host();
}

bool TemplateURL::IsExtensionKeyword() const {
  return GURL(data_.url()).SchemeIs(extensions::kExtensionScheme);
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
    const GURL& url, string16* search_terms) {
  DCHECK(search_terms);
  search_terms->clear();

  // Then try to match with every pattern.
  for (size_t i = 0; i < URLCount(); ++i) {
    TemplateURLRef ref(this, i);
    if (ref.ExtractSearchTermsFromURL(url, search_terms)) {
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
    DCHECK(!IsExtensionKeyword());
    GURL url(TemplateURLService::GenerateSearchURL(this));
    if (url.is_valid())
      data_.SetKeyword(TemplateURLService::GenerateKeyword(url));
  }
}
