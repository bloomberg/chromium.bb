// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url.h"

#include "base/i18n/case_conversion.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_field_trial.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_service.h"
#include "chrome/common/guid.h"
#include "chrome/common/url_constants.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"

// The TemplateURLRef has any number of terms that need to be replaced. Each of
// the terms is enclosed in braces. If the character preceeding the final
// brace is a ?, it indicates the term is optional and can be replaced with
// an empty string.
static const char kStartParameter = '{';
static const char kEndParameter = '}';
static const char kOptional = '?';

// Known parameters found in the URL.
static const char kSearchTermsParameter[] = "searchTerms";
static const char kSearchTermsParameterFull[] = "{searchTerms}";
static const char kCountParameter[] = "count";
static const char kStartIndexParameter[] = "startIndex";
static const char kStartPageParameter[] = "startPage";
static const char kLanguageParameter[] = "language";
static const char kInputEncodingParameter[] = "inputEncoding";
static const char kOutputEncodingParameter[] = "outputEncoding";

static const char kGoogleAcceptedSuggestionParameter[] =
    "google:acceptedSuggestion";
// Host/Domain Google searches are relative to.
static const char kGoogleBaseURLParameter[] = "google:baseURL";
static const char kGoogleBaseURLParameterFull[] = "{google:baseURL}";
// Like google:baseURL, but for the Search Suggest capability.
static const char kGoogleBaseSuggestURLParameter[] =
    "google:baseSuggestURL";
static const char kGoogleBaseSuggestURLParameterFull[] =
    "{google:baseSuggestURL}";
static const char kGoogleInstantEnabledParameter[] =
    "google:instantEnabledParameter";
static const char kGoogleOriginalQueryForSuggestionParameter[] =
    "google:originalQueryForSuggestion";
static const char kGoogleRLZParameter[] = "google:RLZ";
// Same as kSearchTermsParameter, with no escaping.
static const char kGoogleSearchFieldtrialParameter[] =
    "google:searchFieldtrialParameter";
static const char kGoogleUnescapedSearchTermsParameter[] =
    "google:unescapedSearchTerms";
static const char kGoogleUnescapedSearchTermsParameterFull[] =
    "{google:unescapedSearchTerms}";

// Display value for kSearchTermsParameter.
static const char kDisplaySearchTerms[] = "%s";

// Display value for kGoogleUnescapedSearchTermsParameter.
static const char kDisplayUnescapedSearchTerms[] = "%S";

// Used if the count parameter is not optional. Indicates we want 10 search
// results.
static const char kDefaultCount[] = "10";

// Used if the parameter kOutputEncodingParameter is required.
static const char kOutputEncodingType[] = "UTF-8";


// TemplateURLRef -------------------------------------------------------------

TemplateURLRef::TemplateURLRef(TemplateURL* owner, Type type)
    : owner_(owner),
      type_(type),
      parsed_(false),
      valid_(false),
      supports_replacements_(false),
      prepopulated_(false) {
  DCHECK(owner_);
}

TemplateURLRef::~TemplateURLRef() {
}

std::string TemplateURLRef::GetURL() const {
  switch (type_) {
    case SEARCH:  return owner_->url();
    case SUGGEST: return owner_->suggestions_url();
    case INSTANT: return owner_->instant_url();
    default:      NOTREACHED(); return std::string();
  }
}

bool TemplateURLRef::SupportsReplacement() const {
  UIThreadSearchTermsData search_terms_data;
  return SupportsReplacementUsingTermsData(search_terms_data);
}

bool TemplateURLRef::SupportsReplacementUsingTermsData(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessaryUsingTermsData(search_terms_data);
  return valid_ && supports_replacements_;
}

std::string TemplateURLRef::ReplaceSearchTerms(
    const string16& terms,
    int accepted_suggestion,
    const string16& original_query_for_suggestion) const {
  UIThreadSearchTermsData search_terms_data;
  search_terms_data.set_profile(owner_->profile());
  return ReplaceSearchTermsUsingTermsData(terms, accepted_suggestion,
      original_query_for_suggestion, search_terms_data);
}

std::string TemplateURLRef::ReplaceSearchTermsUsingTermsData(
    const string16& terms,
    int accepted_suggestion,
    const string16& original_query_for_suggestion,
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
    if (net::EscapeQueryParamValue(terms, i->c_str(), is_in_query,
        &encoded_terms)) {
      if (is_in_query && !original_query_for_suggestion.empty()) {
        net::EscapeQueryParamValue(original_query_for_suggestion, i->c_str(),
                                   true, &encoded_original_query);
      }
      input_encoding = *i;
      break;
    }
  }
  if (input_encoding.empty()) {
    encoded_terms = net::EscapeQueryParamValueUTF8(terms, is_in_query);
    if (is_in_query && !original_query_for_suggestion.empty()) {
      encoded_original_query =
          net::EscapeQueryParamValueUTF8(original_query_for_suggestion, true);
    }
    input_encoding = "UTF-8";
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

      case GOOGLE_ACCEPTED_SUGGESTION:
        if (accepted_suggestion == NO_SUGGESTION_CHOSEN)
          url.insert(i->index, "aq=f&");
        else if (accepted_suggestion != NO_SUGGESTIONS_AVAILABLE)
          url.insert(i->index,
                     base::StringPrintf("aq=%d&", accepted_suggestion));
        break;

      case GOOGLE_BASE_URL:
        url.insert(i->index, search_terms_data.GoogleBaseURLValue());
        break;

      case GOOGLE_BASE_SUGGEST_URL:
        url.insert(i->index, search_terms_data.GoogleBaseSuggestURLValue());
        break;

      case GOOGLE_INSTANT_ENABLED:
        url.insert(i->index, search_terms_data.InstantEnabledParam());
        break;

      case GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION:
        if (accepted_suggestion >= 0)
          url.insert(i->index, "oq=" + UTF16ToUTF8(encoded_original_query) +
                               "&");
        break;

      case GOOGLE_RLZ: {
        // On platforms that don't have RLZ, we still want this branch
        // to happen so that we replace the RLZ template with the
        // empty string.  (If we don't handle this case, we hit a
        // NOTREACHED below.)
#if defined(ENABLE_RLZ)
        string16 rlz_string = search_terms_data.GetRlzParameterValue();
        if (!rlz_string.empty()) {
          url.insert(i->index, "rlz=" + UTF16ToUTF8(rlz_string) + "&");
        }
#endif
        break;
      }

      case GOOGLE_SEARCH_FIELDTRIAL_GROUP:
        if (AutocompleteFieldTrial::InSuggestFieldTrial()) {
          // Add something like sugexp=chrome,mod=5 to the URL request.
          url.insert(i->index, "sugexp=chrome,mod=" +
              AutocompleteFieldTrial::GetSuggestGroupName() + "&");
        }
        break;

      case GOOGLE_UNESCAPED_SEARCH_TERMS: {
        std::string unescaped_terms;
        base::UTF16ToCodepage(terms, input_encoding.c_str(),
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
  UIThreadSearchTermsData search_terms_data;
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
  } else if (parameter == kGoogleBaseURLParameter) {
    replacements->push_back(Replacement(GOOGLE_BASE_URL, start));
  } else if (parameter == kGoogleBaseSuggestURLParameter) {
    replacements->push_back(Replacement(GOOGLE_BASE_SUGGEST_URL, start));
  } else if (parameter == kGoogleInstantEnabledParameter) {
    replacements->push_back(Replacement(GOOGLE_INSTANT_ENABLED, start));
  } else if (parameter == kGoogleOriginalQueryForSuggestionParameter) {
    replacements->push_back(Replacement(GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
                                        start));
  } else if (parameter == kGoogleRLZParameter) {
    replacements->push_back(Replacement(GOOGLE_RLZ, start));
  } else if (parameter == kGoogleSearchFieldtrialParameter) {
    replacements->push_back(Replacement(GOOGLE_SEARCH_FIELDTRIAL_GROUP, start));
  } else if (parameter == kGoogleUnescapedSearchTermsParameter) {
    replacements->push_back(Replacement(GOOGLE_UNESCAPED_SEARCH_TERMS, start));
  } else {
    // If it's a prepopulated URL, we know that it's safe to remove unknown
    // parameters. Otherwise it could be some garbage but can also be a
    // javascript block. Put it back.
    if (!prepopulated_)
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
  UIThreadSearchTermsData search_terms_data;
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

  GURL url(url_string);
  if (!url.is_valid())
    return;

  std::string query_string = url.query();
  if (query_string.empty())
    return;

  url_parse::Component query, key, value;
  query.len = static_cast<int>(query_string.size());
  while (url_parse::ExtractQueryKeyValue(query_string.c_str(), &query, &key,
                                         &value)) {
    if (key.is_nonempty() && value.is_nonempty()) {
      std::string value_string = query_string.substr(value.begin, value.len);
      if (value_string.find(kSearchTermsParameterFull, 0) !=
          std::string::npos ||
          value_string.find(kGoogleUnescapedSearchTermsParameterFull, 0) !=
          std::string::npos) {
        search_term_key_ = query_string.substr(key.begin, key.len);
        host_ = url.host();
        path_ = url.path();
        break;
      }
    }
  }
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
      sync_guid(guid::GenerateGUID()),
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

TemplateURL::TemplateURL(const TemplateURL& other)
    : profile_(other.profile_),
      data_(other.data_),
      url_ref_(ALLOW_THIS_IN_INITIALIZER_LIST(this), TemplateURLRef::SEARCH),
      suggestions_url_ref_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                           TemplateURLRef::SUGGEST),
      instant_url_ref_(ALLOW_THIS_IN_INITIALIZER_LIST(this),
                       TemplateURLRef::INSTANT) {
  SetPrepopulateId(data_.prepopulate_id);
}

TemplateURL& TemplateURL::operator=(const TemplateURL& other) {
  if (this == &other)
    return *this;

  profile_ = other.profile_;
  data_ = other.data_;
  url_ref_.InvalidateCachedValues();
  suggestions_url_ref_.InvalidateCachedValues();
  instant_url_ref_.InvalidateCachedValues();
  SetPrepopulateId(data_.prepopulate_id);
  return *this;
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
  UIThreadSearchTermsData search_terms_data;
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
  return GURL(data_.url()).SchemeIs(chrome::kExtensionScheme);
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

void TemplateURL::InvalidateCachedValues() {
  url_ref_.InvalidateCachedValues();
  suggestions_url_ref_.InvalidateCachedValues();
  instant_url_ref_.InvalidateCachedValues();
  ResetKeywordIfNecessary(false);
}
