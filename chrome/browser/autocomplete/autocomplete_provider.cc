// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_provider.h"

#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/net/url_fixer_upper.h"
#include "chrome/common/pref_names.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_util.h"
#include "url/gurl.h"

// static
const size_t AutocompleteProvider::kMaxMatches = 3;

AutocompleteProvider::AutocompleteProvider(
    AutocompleteProviderListener* listener,
    Profile* profile,
    Type type)
    : profile_(profile),
      listener_(listener),
      done_(true),
      type_(type) {
}

// static
const char* AutocompleteProvider::TypeToString(Type type) {
  switch (type) {
    case TYPE_BOOKMARK:
      return "Bookmark";
    case TYPE_BUILTIN:
      return "Builtin";
    case TYPE_CONTACT:
      return "Contact";
    case TYPE_EXTENSION_APP:
      return "ExtensionApp";
    case TYPE_HISTORY_QUICK:
      return "HistoryQuick";
    case TYPE_HISTORY_URL:
      return "HistoryURL";
    case TYPE_KEYWORD:
      return "Keyword";
    case TYPE_SEARCH:
      return "Search";
    case TYPE_SHORTCUTS:
      return "Shortcuts";
    case TYPE_ZERO_SUGGEST:
      return "ZeroSuggest";
    default:
      NOTREACHED() << "Unhandled AutocompleteProvider::Type " << type;
      return "Unknown";
  }
}

void AutocompleteProvider::Stop(bool clear_cached_results) {
  done_ = true;
}

const char* AutocompleteProvider::GetName() const {
  return TypeToString(type_);
}

metrics::OmniboxEventProto_ProviderType AutocompleteProvider::
    AsOmniboxEventProviderType() const {
  switch (type_) {
    case TYPE_BOOKMARK:
      return metrics::OmniboxEventProto::BOOKMARK;
    case TYPE_BUILTIN:
      return metrics::OmniboxEventProto::BUILTIN;
    case TYPE_CONTACT:
      return metrics::OmniboxEventProto::CONTACT;
    case TYPE_EXTENSION_APP:
      return metrics::OmniboxEventProto::EXTENSION_APPS;
    case TYPE_HISTORY_QUICK:
      return metrics::OmniboxEventProto::HISTORY_QUICK;
    case TYPE_HISTORY_URL:
      return metrics::OmniboxEventProto::HISTORY_URL;
    case TYPE_KEYWORD:
      return metrics::OmniboxEventProto::KEYWORD;
    case TYPE_SEARCH:
      return metrics::OmniboxEventProto::SEARCH;
    case TYPE_SHORTCUTS:
      return metrics::OmniboxEventProto::SHORTCUTS;
    case TYPE_ZERO_SUGGEST:
      return metrics::OmniboxEventProto::ZERO_SUGGEST;
    default:
      NOTREACHED() << "Unhandled AutocompleteProvider::Type " << type_;
      return metrics::OmniboxEventProto::UNKNOWN_PROVIDER;
  }
}

void AutocompleteProvider::DeleteMatch(const AutocompleteMatch& match) {
  DLOG(WARNING) << "The AutocompleteProvider '" << GetName()
                << "' has not implemented DeleteMatch.";
}

void AutocompleteProvider::AddProviderInfo(ProvidersInfo* provider_info) const {
}

void AutocompleteProvider::ResetSession() {
}

base::string16 AutocompleteProvider::StringForURLDisplay(const GURL& url,
                                                   bool check_accept_lang,
                                                   bool trim_http) const {
  std::string languages = (check_accept_lang && profile_) ?
      profile_->GetPrefs()->GetString(prefs::kAcceptLanguages) : std::string();
  return net::FormatUrl(url, languages,
      net::kFormatUrlOmitAll & ~(trim_http ? 0 : net::kFormatUrlOmitHTTP),
      net::UnescapeRule::SPACES, NULL, NULL, NULL);
}

AutocompleteProvider::~AutocompleteProvider() {
  Stop(false);
}

void AutocompleteProvider::UpdateStarredStateOfMatches() {
  if (matches_.empty())
    return;

  if (!profile_)
    return;

  BookmarkModel* bookmark_model = BookmarkModelFactory::GetForProfile(profile_);
  if (!bookmark_model || !bookmark_model->loaded())
    return;

  for (ACMatches::iterator i(matches_.begin()); i != matches_.end(); ++i)
    i->starred = bookmark_model->IsBookmarked(i->destination_url);
}

// static
bool AutocompleteProvider::FixupUserInput(AutocompleteInput* input) {
  const base::string16& input_text = input->text();
  // Fixup and canonicalize user input.
  const GURL canonical_gurl(URLFixerUpper::FixupURL(UTF16ToUTF8(input_text),
                                                    std::string()));
  std::string canonical_gurl_str(canonical_gurl.possibly_invalid_spec());
  if (canonical_gurl_str.empty()) {
    // This probably won't happen, but there are no guarantees.
    return false;
  }

  // If the user types a number, GURL will convert it to a dotted quad.
  // However, if the parser did not mark this as a URL, then the user probably
  // didn't intend this interpretation.  Since this can break history matching
  // for hostname beginning with numbers (e.g. input of "17173" will be matched
  // against "0.0.67.21" instead of the original "17173", failing to find
  // "17173.com"), swap the original hostname in for the fixed-up one.
  if ((input->type() != AutocompleteInput::URL) &&
      canonical_gurl.HostIsIPAddress()) {
    std::string original_hostname =
        UTF16ToUTF8(input_text.substr(input->parts().host.begin,
                                      input->parts().host.len));
    const url_parse::Parsed& parts =
        canonical_gurl.parsed_for_possibly_invalid_spec();
    // parts.host must not be empty when HostIsIPAddress() is true.
    DCHECK(parts.host.is_nonempty());
    canonical_gurl_str.replace(parts.host.begin, parts.host.len,
                               original_hostname);
  }
  base::string16 output = UTF8ToUTF16(canonical_gurl_str);
  // Don't prepend a scheme when the user didn't have one.  Since the fixer
  // upper only prepends the "http" scheme, that's all we need to check for.
  if (!AutocompleteInput::HasHTTPScheme(input_text))
    TrimHttpPrefix(&output);

  // Make the number of trailing slashes on the output exactly match the input.
  // Examples of why not doing this would matter:
  // * The user types "a" and has this fixed up to "a/".  Now no other sites
  //   beginning with "a" will match.
  // * The user types "file:" and has this fixed up to "file://".  Now inline
  //   autocomplete will append too few slashes, resulting in e.g. "file:/b..."
  //   instead of "file:///b..."
  // * The user types "http:/" and has this fixed up to "http:".  Now inline
  //   autocomplete will append too many slashes, resulting in e.g.
  //   "http:///c..." instead of "http://c...".
  // NOTE: We do this after calling TrimHttpPrefix() since that can strip
  // trailing slashes (if the scheme is the only thing in the input).  It's not
  // clear that the result of fixup really matters in this case, but there's no
  // harm in making sure.
  const size_t last_input_nonslash =
      input_text.find_last_not_of(ASCIIToUTF16("/\\"));
  const size_t num_input_slashes =
      (last_input_nonslash == base::string16::npos) ?
      input_text.length() : (input_text.length() - 1 - last_input_nonslash);
  const size_t last_output_nonslash =
      output.find_last_not_of(ASCIIToUTF16("/\\"));
  const size_t num_output_slashes =
      (last_output_nonslash == base::string16::npos) ?
      output.length() : (output.length() - 1 - last_output_nonslash);
  if (num_output_slashes < num_input_slashes)
    output.append(num_input_slashes - num_output_slashes, '/');
  else if (num_output_slashes > num_input_slashes)
    output.erase(output.length() - num_output_slashes + num_input_slashes);

  url_parse::Parsed parts;
  URLFixerUpper::SegmentURL(output, &parts);
  input->UpdateText(output, base::string16::npos, parts);
  return !output.empty();
}

// static
size_t AutocompleteProvider::TrimHttpPrefix(base::string16* url) {
  // Find any "http:".
  if (!AutocompleteInput::HasHTTPScheme(*url))
    return 0;
  size_t scheme_pos =
      url->find(ASCIIToUTF16(content::kHttpScheme) + char16(':'));
  DCHECK_NE(base::string16::npos, scheme_pos);

  // Erase scheme plus up to two slashes.
  size_t prefix_end = scheme_pos + strlen(content::kHttpScheme) + 1;
  const size_t after_slashes = std::min(url->length(), prefix_end + 2);
  while ((prefix_end < after_slashes) && ((*url)[prefix_end] == '/'))
    ++prefix_end;
  url->erase(scheme_pos, prefix_end - scheme_pos);
  return (scheme_pos == 0) ? prefix_end : 0;
}

