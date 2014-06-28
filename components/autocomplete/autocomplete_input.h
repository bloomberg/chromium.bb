// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOCOMPLETE_AUTOCOMPLETE_INPUT_H_
#define COMPONENTS_AUTOCOMPLETE_AUTOCOMPLETE_INPUT_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "url/gurl.h"
#include "url/url_parse.h"

class AutocompleteSchemeClassifier;

// The user input for an autocomplete query.  Allows copying.
class AutocompleteInput {
 public:
  AutocompleteInput();
  // |text| and |cursor_position| represent the input query and location of
  // the cursor with the query respectively.  |cursor_position| may be set to
  // base::string16::npos if the input |text| doesn't come directly from the
  // user's typing.
  //
  // |desired_tld| is the user's desired TLD, if one is not already present in
  // the text to autocomplete.  When this is non-empty, it also implies that
  // "www." should be prepended to the domain where possible. The |desired_tld|
  // should not contain a leading '.' (use "com" instead of ".com").
  //
  // If |current_url| is set to a valid search result page URL, providers can
  // use it to perform query refinement. For example, if it is set to an image
  // search result page, the search provider may generate an image search URL.
  // Query refinement is only used by mobile ports, so only these set
  // |current_url| to a non-empty string.
  //
  // |current_page_classification| represents the type of page the user is
  // viewing and manner in which the user is accessing the omnibox; it's
  // more than simply the URL.  It includes, for example, whether the page
  // is a search result page doing search term replacement or not.
  //
  // |prevent_inline_autocomplete| is true if the generated result set should
  // not require inline autocomplete for the default match.  This is difficult
  // to explain in the abstract; the practical use case is that after the user
  // deletes text in the edit, the HistoryURLProvider should make sure not to
  // promote a match requiring inline autocomplete too highly.
  //
  // |prefer_keyword| should be true when the keyword UI is onscreen; this will
  // bias the autocomplete result set toward the keyword provider when the input
  // string is a bare keyword.
  //
  // |allow_exact_keyword_match| should be false when triggering keyword mode on
  // the input string would be surprising or wrong, e.g. when highlighting text
  // in a page and telling the browser to search for it or navigate to it. This
  // parameter only applies to substituting keywords.
  //
  // If |want_asynchronous_matches| is false the controller asks the providers
  // to only return matches which are synchronously available, which should mean
  // that all providers will be done immediately.
  //
  // |scheme_classifier| is passed to Parse() to help determine the type of
  // input this is; see comments there.
  AutocompleteInput(const base::string16& text,
                    size_t cursor_position,
                    const base::string16& desired_tld,
                    const GURL& current_url,
                    metrics::OmniboxEventProto::PageClassification
                        current_page_classification,
                    bool prevent_inline_autocomplete,
                    bool prefer_keyword,
                    bool allow_exact_keyword_match,
                    bool want_asynchronous_matches,
                    const AutocompleteSchemeClassifier& scheme_classifier);
  ~AutocompleteInput();

  // If type is |FORCED_QUERY| and |text| starts with '?', it is removed.
  // Returns number of leading characters removed.
  static size_t RemoveForcedQueryStringIfNecessary(
      metrics::OmniboxInputType::Type type,
      base::string16* text);

  // Converts |type| to a string representation.  Used in logging.
  static std::string TypeToString(metrics::OmniboxInputType::Type type);

  // Parses |text| (including an optional |desired_tld|) and returns the type of
  // input this will be interpreted as.  |scheme_classifier| is used to check
  // the scheme in |text| is known and registered in the current environment.
  // The components of the input are stored in the output parameter |parts|, if
  // it is non-NULL. The scheme is stored in |scheme| if it is non-NULL. The
  // canonicalized URL is stored in |canonicalized_url|; however, this URL is
  // not guaranteed to be valid, especially if the parsed type is, e.g., QUERY.
  static metrics::OmniboxInputType::Type Parse(
      const base::string16& text,
      const base::string16& desired_tld,
      const AutocompleteSchemeClassifier& scheme_classifier,
      url::Parsed* parts,
      base::string16* scheme,
      GURL* canonicalized_url);

  // Parses |text| and fill |scheme| and |host| by the positions of them.
  // The results are almost as same as the result of Parse(), but if the scheme
  // is view-source, this function returns the positions of scheme and host
  // in the URL qualified by "view-source:" prefix.
  static void ParseForEmphasizeComponents(
      const base::string16& text,
      const AutocompleteSchemeClassifier& scheme_classifier,
      url::Component* scheme,
      url::Component* host);

  // Code that wants to format URLs with a format flag including
  // net::kFormatUrlOmitTrailingSlashOnBareHostname risk changing the meaning if
  // the result is then parsed as AutocompleteInput.  Such code can call this
  // function with the URL and its formatted string, and it will return a
  // formatted string with the same meaning as the original URL (i.e. it will
  // re-append a slash if necessary).  Because this uses Parse() under the hood
  // to determine the meaning of the different strings, callers need to supply a
  // |scheme_classifier| to pass to Parse().
  static base::string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const base::string16& formatted_url,
      const AutocompleteSchemeClassifier& scheme_classifier);

  // Returns the number of non-empty components in |parts| besides the host.
  static int NumNonHostComponents(const url::Parsed& parts);

  // Returns whether |text| begins "http:" or "view-source:http:".
  static bool HasHTTPScheme(const base::string16& text);

  // User-provided text to be completed.
  const base::string16& text() const { return text_; }

  // Returns 0-based cursor position within |text_| or base::string16::npos if
  // not used.
  size_t cursor_position() const { return cursor_position_; }

  // Use of this setter is risky, since no other internal state is updated
  // besides |text_|, |cursor_position_| and |parts_|.  Only callers who know
  // that they're not changing the type/scheme/etc. should use this.
  void UpdateText(const base::string16& text,
                  size_t cursor_position,
                  const url::Parsed& parts);

  // The current URL, or an invalid GURL if query refinement is not desired.
  const GURL& current_url() const { return current_url_; }

  // The type of page that is currently behind displayed and how it is
  // displayed (e.g., with search term replacement or without).
  metrics::OmniboxEventProto::PageClassification current_page_classification()
      const {
    return current_page_classification_;
  }

  // The type of input supplied.
  metrics::OmniboxInputType::Type type() const { return type_; }

  // Returns parsed URL components.
  const url::Parsed& parts() const { return parts_; }

  // The scheme parsed from the provided text; only meaningful when type_ is
  // URL.
  const base::string16& scheme() const { return scheme_; }

  // The input as an URL to navigate to, if possible.
  const GURL& canonicalized_url() const { return canonicalized_url_; }

  // Returns whether inline autocompletion should be prevented.
  bool prevent_inline_autocomplete() const {
    return prevent_inline_autocomplete_;
  }

  // Returns whether, given an input string consisting solely of a substituting
  // keyword, we should score it like a non-substituting keyword.
  bool prefer_keyword() const { return prefer_keyword_; }

  // Returns whether this input is allowed to be treated as an exact
  // keyword match.  If not, the default result is guaranteed not to be a
  // keyword search, even if the input is "<keyword> <search string>".
  bool allow_exact_keyword_match() const { return allow_exact_keyword_match_; }

  // Returns whether providers should be allowed to make asynchronous requests
  // when processing this input.
  bool want_asynchronous_matches() const { return want_asynchronous_matches_; }

  // Resets all internal variables to the null-constructed state.
  void Clear();

 private:
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest, GetDestinationURL);

  // NOTE: Whenever adding a new field here, please make sure to update Clear()
  // method.
  base::string16 text_;
  size_t cursor_position_;
  GURL current_url_;
  metrics::OmniboxEventProto::PageClassification current_page_classification_;
  metrics::OmniboxInputType::Type type_;
  url::Parsed parts_;
  base::string16 scheme_;
  GURL canonicalized_url_;
  bool prevent_inline_autocomplete_;
  bool prefer_keyword_;
  bool allow_exact_keyword_match_;
  bool want_asynchronous_matches_;
};

#endif  // COMPONENTS_AUTOCOMPLETE_AUTOCOMPLETE_INPUT_H_
