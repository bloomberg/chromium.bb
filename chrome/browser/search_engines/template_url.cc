// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engines/template_url.h"

#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/search_engines/search_engine_type.h"
#include "chrome/browser/search_engines/search_terms_data.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/installer/util/google_update_settings.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/favicon_size.h"
// TODO(pastarmovj): Remove google_update_settings and user_metrics when the
// CollectRLZMetrics function is not needed anymore.

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
static const char kGoogleOriginalQueryForSuggestionParameter[] =
    "google:originalQueryForSuggestion";
static const char kGoogleRLZParameter[] = "google:RLZ";
// Same as kSearchTermsParameter, with no escaping.
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

TemplateURLRef::TemplateURLRef() {
  Set(std::string(), 0, 0);
}

TemplateURLRef::TemplateURLRef(const std::string& url,
                               int index_offset,
                               int page_offset)
    : url_(url),
      index_offset_(index_offset),
      page_offset_(page_offset),
      parsed_(false),
      valid_(false),
      supports_replacements_(false) {
}

void TemplateURLRef::Set(const std::string& url,
                         int index_offset,
                         int page_offset) {
  url_ = url;
  index_offset_ = index_offset;
  page_offset_ = page_offset;
  InvalidateCachedValues();
}

TemplateURLRef::~TemplateURLRef() {
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
  // Remove the parameter from the string.
  url->erase(start, end - start + 1);
  if (parameter == kSearchTermsParameter) {
    replacements->push_back(Replacement(SEARCH_TERMS, start));
  } else if (parameter == kCountParameter) {
    if (!optional)
      url->insert(start, kDefaultCount);
  } else if (parameter == kStartIndexParameter) {
    if (!optional) {
      url->insert(start, base::IntToString(index_offset_));
    }
  } else if (parameter == kStartPageParameter) {
    if (!optional) {
      url->insert(start, base::IntToString(page_offset_));
    }
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
  } else if (parameter == kGoogleOriginalQueryForSuggestionParameter) {
    replacements->push_back(Replacement(GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
                                        start));
  } else if (parameter == kGoogleRLZParameter) {
    replacements->push_back(Replacement(GOOGLE_RLZ, start));
  } else if (parameter == kGoogleUnescapedSearchTermsParameter) {
    replacements->push_back(Replacement(GOOGLE_UNESCAPED_SEARCH_TERMS, start));
  } else {
    // It can be some garbage but can also be a javascript block. Put it back.
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
    parsed_url_ = ParseURL(url_, &replacements_, &valid_);
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
  std::string url_string = url_;
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

// static
void TemplateURLRef::SetGoogleBaseURL(std::string* google_base_url) {
  UIThreadSearchTermsData::SetGoogleBaseURL(google_base_url);
}

std::string TemplateURLRef::ReplaceSearchTerms(
    const TemplateURL& host,
    const string16& terms,
    int accepted_suggestion,
    const string16& original_query_for_suggestion) const {
  UIThreadSearchTermsData search_terms_data;
  return ReplaceSearchTermsUsingTermsData(host,
                                          terms,
                                          accepted_suggestion,
                                          original_query_for_suggestion,
                                          search_terms_data);
}

std::string TemplateURLRef::ReplaceSearchTermsUsingTermsData(
    const TemplateURL& host,
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
  // If the search terms are in query - escape them respecting the encoding.
  if (is_in_query) {
    // Encode the search terms so that we know the encoding.
    const std::vector<std::string>& encodings = host.input_encodings();
    for (size_t i = 0; i < encodings.size(); ++i) {
      if (EscapeQueryParamValue(terms,
                                encodings[i].c_str(), true,
                                &encoded_terms)) {
        if (!original_query_for_suggestion.empty()) {
          EscapeQueryParamValue(original_query_for_suggestion,
                                encodings[i].c_str(),
                                true,
                                &encoded_original_query);
        }
        input_encoding = encodings[i];
        break;
      }
    }
    if (input_encoding.empty()) {
      encoded_terms = EscapeQueryParamValueUTF8(terms, true);
      if (!original_query_for_suggestion.empty()) {
        encoded_original_query =
            EscapeQueryParamValueUTF8(original_query_for_suggestion, true);
      }
      input_encoding = "UTF-8";
    }
  } else {
    encoded_terms = UTF8ToUTF16(EscapePath(UTF16ToUTF8(terms)));
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
          url.insert(i->index, StringPrintf("aq=%d&", accepted_suggestion));
        break;

      case GOOGLE_BASE_URL:
        url.insert(i->index, search_terms_data.GoogleBaseURLValue());
        break;

      case GOOGLE_BASE_SUGGEST_URL:
        url.insert(i->index, search_terms_data.GoogleBaseSuggestURLValue());
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
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
        string16 rlz_string = search_terms_data.GetRlzParameterValue();
        if (!rlz_string.empty()) {
          rlz_string = L"rlz=" + rlz_string + L"&";
          url.insert(i->index, UTF16ToUTF8(rlz_string));
        }
#endif
        break;
      }

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

bool TemplateURLRef::SupportsReplacement() const {
  UIThreadSearchTermsData search_terms_data;
  return SupportsReplacementUsingTermsData(search_terms_data);
}

bool TemplateURLRef::SupportsReplacementUsingTermsData(
    const SearchTermsData& search_terms_data) const {
  ParseIfNecessaryUsingTermsData(search_terms_data);
  return valid_ && supports_replacements_;
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
  if (!valid_ || replacements_.empty())
    return UTF8ToUTF16(url_);

  string16 result = UTF8ToUTF16(url_);
  ReplaceSubstringsAfterOffset(&result, 0,
                               ASCIIToUTF16(kSearchTermsParameterFull),
                               ASCIIToUTF16(kDisplaySearchTerms));

  ReplaceSubstringsAfterOffset(
      &result, 0,
      ASCIIToUTF16(kGoogleUnescapedSearchTermsParameterFull),
      ASCIIToUTF16(kDisplayUnescapedSearchTerms));

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

string16 TemplateURLRef::SearchTermToString16(const TemplateURL& host,
                                              const std::string& term) const {
  const std::vector<std::string>& encodings = host.input_encodings();
  string16 result;

  std::string unescaped =
      UnescapeURLComponent(term, UnescapeRule::REPLACE_PLUS_WITH_SPACE |
                                 UnescapeRule::URL_SPECIAL_CHARS);
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

// static
bool TemplateURLRef::SameUrlRefs(const TemplateURLRef* ref1,
 const TemplateURLRef* ref2) {
  return ref1 == ref2 || (ref1 && ref2 && ref1->url() == ref2->url());
}

void TemplateURLRef::CollectRLZMetrics() const {
#if defined(OS_WIN) && defined(GOOGLE_CHROME_BUILD)
  ParseIfNecessary();
  for (size_t i = 0; i < replacements_.size(); ++i) {
    // We are interesed in searches that were supposed to send the RLZ token.
    if (replacements_[i].type == GOOGLE_RLZ) {
      string16 brand;
      // We only have RLZ tocken on a branded browser version.
      if (GoogleUpdateSettings::GetBrand(&brand) && !brand.empty() &&
           !GoogleUpdateSettings::IsOrganic(brand)) {
        // Now we know we should have had RLZ token check if there was one.
        if (url().find("rlz=") != std::string::npos)
          UserMetrics::RecordAction(UserMetricsAction("SearchWithRLZ"));
        else
          UserMetrics::RecordAction(UserMetricsAction("SearchWithoutRLZ"));
      }
      return;
    }
  }
#endif
}

void TemplateURLRef::InvalidateCachedValues() const {
  supports_replacements_ = valid_ = parsed_ = false;
  host_.clear();
  path_.clear();
  search_term_key_.clear();
  replacements_.clear();
}

// TemplateURL ----------------------------------------------------------------

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

// static
bool TemplateURL::SupportsReplacement(const TemplateURL* turl) {
  UIThreadSearchTermsData search_terms_data;
  return SupportsReplacementUsingTermsData(turl, search_terms_data);
}

// static
bool TemplateURL::SupportsReplacementUsingTermsData(
    const TemplateURL* turl,
    const SearchTermsData& search_terms_data) {
  return turl && turl->url() &&
      turl->url()->SupportsReplacementUsingTermsData(search_terms_data);
}

TemplateURL::TemplateURL()
    : autogenerate_keyword_(false),
      keyword_generated_(false),
      show_in_default_list_(false),
      safe_for_autoreplace_(false),
      id_(0),
      date_created_(base::Time::Now()),
      created_by_policy_(false),
      usage_count_(0),
      search_engine_type_(SEARCH_ENGINE_OTHER),
      logo_id_(kNoSearchEngineLogo),
      prepopulate_id_(0) {
}

TemplateURL::~TemplateURL() {
}

string16 TemplateURL::AdjustedShortNameForLocaleDirection() const {
  string16 bidi_safe_short_name = short_name_;
  base::i18n::AdjustStringForLocaleDirection(&bidi_safe_short_name);
  return bidi_safe_short_name;
}

void TemplateURL::SetSuggestionsURL(const std::string& suggestions_url,
                                    int index_offset,
                                    int page_offset) {
  suggestions_url_.Set(suggestions_url, index_offset, page_offset);
}

void TemplateURL::SetURL(const std::string& url,
                         int index_offset,
                         int page_offset) {
  url_.Set(url, index_offset, page_offset);
}

void TemplateURL::SetInstantURL(const std::string& url,
                                int index_offset,
                                int page_offset) {
  instant_url_.Set(url, index_offset, page_offset);
}

void TemplateURL::set_keyword(const string16& keyword) {
  // Case sensitive keyword matching is confusing. As such, we force all
  // keywords to be lower case.
  keyword_ = l10n_util::ToLower(keyword);
  autogenerate_keyword_ = false;
}

string16 TemplateURL::keyword() const {
  EnsureKeyword();
  return keyword_;
}

void TemplateURL::EnsureKeyword() const {
  if (autogenerate_keyword_ && !keyword_generated_) {
    // Generate a keyword and cache it.
    keyword_ = TemplateURLModel::GenerateKeyword(
        TemplateURLModel::GenerateSearchURL(this).GetWithEmptyPath(), true);
    keyword_generated_ = true;
  }
}

bool TemplateURL::ShowInDefaultList() const {
  return show_in_default_list() && url() && url()->SupportsReplacement();
}

void TemplateURL::SetFavIconURL(const GURL& url) {
  for (std::vector<ImageRef>::iterator i = image_refs_.begin();
       i != image_refs_.end(); ++i) {
    if (i->type == "image/x-icon" &&
        i->width == kFaviconSize && i->height == kFaviconSize) {
      if (!url.is_valid())
        image_refs_.erase(i);
      else
        i->url = url;
      return;
    }
  }
  // Don't have one yet, add it.
  if (url.is_valid()) {
    add_image_ref(
        TemplateURL::ImageRef("image/x-icon", kFaviconSize,
                              kFaviconSize, url));
  }
}

GURL TemplateURL::GetFavIconURL() const {
  for (std::vector<ImageRef>::const_iterator i = image_refs_.begin();
       i != image_refs_.end(); ++i) {
    if ((i->type == "image/x-icon" || i->type == "image/vnd.microsoft.icon")
        && i->width == kFaviconSize && i->height == kFaviconSize) {
      return i->url;
    }
  }
  return GURL();
}

void TemplateURL::InvalidateCachedValues() const {
  url_.InvalidateCachedValues();
  suggestions_url_.InvalidateCachedValues();
  if (autogenerate_keyword_) {
    keyword_.clear();
    keyword_generated_ = false;
  }
}

std::string TemplateURL::GetExtensionId() const {
  DCHECK(IsExtensionKeyword());
  return GURL(url_.url()).host();
}

bool TemplateURL::IsExtensionKeyword() const {
  return GURL(url_.url()).SchemeIs(chrome::kExtensionScheme);
}
