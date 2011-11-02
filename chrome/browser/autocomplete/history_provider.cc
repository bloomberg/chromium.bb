// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/history_url_provider.h"

#include <string>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete.h"
#include "chrome/browser/autocomplete/autocomplete_match.h"
#include "chrome/browser/history/history.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_util.h"

HistoryProvider::HistoryProvider(ACProviderListener* listener,
                                 Profile* profile,
                                 const char* name)
    : AutocompleteProvider(listener, profile, name) {
}

void HistoryProvider::DeleteMatch(const AutocompleteMatch& match) {
  DCHECK(done_);
  DCHECK(profile_);
  DCHECK(match.deletable);

  HistoryService* const history_service =
      profile_->GetHistoryService(Profile::EXPLICIT_ACCESS);

  // Delete the match from the history DB.
  DCHECK(history_service);
  DCHECK(match.destination_url.is_valid());
  history_service->DeleteURL(match.destination_url);

  // Delete the match from the current set of matches.
  bool found = false;
  for (ACMatches::iterator i(matches_.begin()); i != matches_.end(); ++i) {
    if (i->destination_url == match.destination_url && i->type == match.type) {
      found = true;
      if (i->is_history_what_you_typed_match || i->starred) {
        // We can't get rid of What-You-Typed or Bookmarked matches,
        // but we can make them look like they have no backing data.
        i->deletable = false;
        i->description.clear();
        i->description_class.clear();
      } else {
        matches_.erase(i);
      }
      break;
    }
  }
  DCHECK(found) << "Asked to delete a URL that isn't in our set of matches";
  listener_->OnProviderUpdate(true);
}

// static
bool HistoryProvider::FixupUserInput(AutocompleteInput* input) {
  const string16& input_text = input->text();
  // Fixup and canonicalize user input.
  // NOTE: This purposefully doesn't take input.desired_tld() into account; if
  // it did, then holding "ctrl" would change all the results from the provider,
  // not just the What You Typed Result.
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
  string16 output = UTF8ToUTF16(canonical_gurl_str);
  // Don't prepend a scheme when the user didn't have one.  Since the fixer
  // upper only prepends the "http" scheme, that's all we need to check for.
  if (canonical_gurl.SchemeIs(chrome::kHttpScheme) &&
      !url_util::FindAndCompareScheme(UTF16ToUTF8(input_text),
                                      chrome::kHttpScheme, NULL))
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
  const size_t num_input_slashes = (last_input_nonslash == string16::npos) ?
      input_text.length() : (input_text.length() - 1 - last_input_nonslash);
  const size_t last_output_nonslash =
      output.find_last_not_of(ASCIIToUTF16("/\\"));
  const size_t num_output_slashes =
      (last_output_nonslash == string16::npos) ?
      output.length() : (output.length() - 1 - last_output_nonslash);
  if (num_output_slashes < num_input_slashes)
    output.append(num_input_slashes - num_output_slashes, '/');
  else if (num_output_slashes > num_input_slashes)
    output.erase(output.length() - num_output_slashes + num_input_slashes);

  url_parse::Parsed parts;
  URLFixerUpper::SegmentURL(output, &parts);
  input->UpdateText(output, parts);
  return !output.empty();
}

// static
size_t HistoryProvider::TrimHttpPrefix(string16* url) {
  // Find any "http:".
  if (!HasHTTPScheme(*url))
    return 0;
  size_t scheme_pos =
      url->find(ASCIIToUTF16(chrome::kHttpScheme) + char16(':'));
  DCHECK_NE(string16::npos, scheme_pos);

  // Erase scheme plus up to two slashes.
  size_t prefix_end = scheme_pos + strlen(chrome::kHttpScheme) + 1;
  const size_t after_slashes = std::min(url->length(), prefix_end + 2);
  while ((prefix_end < after_slashes) && ((*url)[prefix_end] == '/'))
    ++prefix_end;
  url->erase(scheme_pos, prefix_end - scheme_pos);
  return (scheme_pos == 0) ? prefix_end : 0;
}

// static
bool HistoryProvider::PreventInlineAutocomplete(
    const AutocompleteInput& input) {
  return input.prevent_inline_autocomplete() ||
        (!input.text().empty() &&
         IsWhitespace(input.text()[input.text().length() - 1]));
}
