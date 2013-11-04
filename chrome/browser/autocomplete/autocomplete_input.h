// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_INPUT_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_INPUT_H_

#include <string>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/strings/string16.h"
#include "url/gurl.h"
#include "url/url_parse.h"

// The user input for an autocomplete query.  Allows copying.
class AutocompleteInput {
 public:
  // Note that the type below may be misleading.  For example, "http:/" alone
  // cannot be opened as a URL, so it is marked as a QUERY; yet the user
  // probably intends to type more and have it eventually become a URL, so we
  // need to make sure we still run it through inline autocomplete.
  enum Type {
    INVALID,        // Empty input
    UNKNOWN,        // Valid input whose type cannot be determined
    URL,            // Input autodetected as a URL
    QUERY,          // Input autodetected as a query
    FORCED_QUERY,   // Input forced to be a query by an initial '?'
  };

  // Enumeration of the possible match query types. Callers who only need some
  // of the matches for a particular input can get answers more quickly by
  // specifying that upfront.
  enum MatchesRequested {
    // Only the best match in the whole result set matters.  Providers should at
    // most return synchronously-available matches, and if possible do even less
    // work, so that it's safe to ask for these repeatedly in the course of one
    // higher-level "synchronous" query.
    BEST_MATCH,

    // Only synchronous matches should be returned.
    SYNCHRONOUS_MATCHES,

    // All matches should be fetched.
    ALL_MATCHES,
  };

  // The type of page currently displayed.
  // Note: when adding an element to this enum, please add it at the end
  // and update omnibox_event.proto::PageClassification and
  // omnibox_edit_model.cc::ClassifyPage() too.
  enum PageClassification {
    // An invalid URL; shouldn't happen.
    INVALID_SPEC = 0,

    // chrome://newtab/.  This can be either the built-in version or a
    // replacement new tab page from an extension.  Note that when Instant
    // Extended is enabled, the new tab page will be reported as either
    // INSTANT_NEW_TAB_PAGE_WITH_OMNIBOX_AS_STARTING_FOCUS or
    // INSTANT_NEW_TAB_PAGE_WITH_FAKEBOX_AS_STARTING_FOCUS below,
    // unless an extension is replacing the new tab page, in which case
    // it will still be reported as NEW_TAB_PAGE.
    NEW_TAB_PAGE = 1,

    // about:blank.
    BLANK = 2,

    // The user's home page.  Note that if the home page is set to any
    // of the new tab page versions or to about:blank, then we'll
    // classify the page into those categories, not HOME_PAGE.
    HOME_PAGE = 3,

    // The catch-all entry of everything not included somewhere else
    // on this list.
    OTHER = 4,

    // The user is on a search result page that's doing search term
    // replacement, meaning the search terms should've appeared in the omnibox
    // before the user started editing it, not the URL of the page.
    SEARCH_RESULT_PAGE_DOING_SEARCH_TERM_REPLACEMENT = 6,

    // The new tab page in which this omnibox interaction first started
    // with the user having focus in the omnibox.
    INSTANT_NEW_TAB_PAGE_WITH_OMNIBOX_AS_STARTING_FOCUS = 7,

    // The new tab page in which this omnibox interaction first started
    // with the user having focus in the fakebox.
    INSTANT_NEW_TAB_PAGE_WITH_FAKEBOX_AS_STARTING_FOCUS = 8,

    // The user is on a search result page that's not doing search term
    // replacement, meaning the URL of the page should've appeared in the
    // omnibox before the user started editing it, not the search terms.
    SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT = 9
  };

  AutocompleteInput();
  // |text| and |cursor_position| represent the input query and location of
  // the cursor with the query respectively.  |cursor_position| may be set to
  // string16::npos if the input |text| doesn't come directly from the user's
  // typing.
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
  // If |matches_requested| is BEST_MATCH or SYNCHRONOUS_MATCHES the controller
  // asks the providers to only return matches which are synchronously
  // available, which should mean that all providers will be done immediately.
  AutocompleteInput(const string16& text,
                    size_t cursor_position,
                    const string16& desired_tld,
                    const GURL& current_url,
                    PageClassification current_page_classification,
                    bool prevent_inline_autocomplete,
                    bool prefer_keyword,
                    bool allow_exact_keyword_match,
                    MatchesRequested matches_requested);
  ~AutocompleteInput();

  // If type is |FORCED_QUERY| and |text| starts with '?', it is removed.
  // Returns number of leading characters removed.
  static size_t RemoveForcedQueryStringIfNecessary(Type type, string16* text);

  // Converts |type| to a string representation.  Used in logging.
  static std::string TypeToString(Type type);

  // Parses |text| and returns the type of input this will be interpreted as.
  // The components of the input are stored in the output parameter |parts|, if
  // it is non-NULL. The scheme is stored in |scheme| if it is non-NULL. The
  // canonicalized URL is stored in |canonicalized_url|; however, this URL is
  // not guaranteed to be valid, especially if the parsed type is, e.g., QUERY.
  static Type Parse(const string16& text,
                    const string16& desired_tld,
                    url_parse::Parsed* parts,
                    string16* scheme,
                    GURL* canonicalized_url);

  // Parses |text| and fill |scheme| and |host| by the positions of them.
  // The results are almost as same as the result of Parse(), but if the scheme
  // is view-source, this function returns the positions of scheme and host
  // in the URL qualified by "view-source:" prefix.
  static void ParseForEmphasizeComponents(const string16& text,
                                          url_parse::Component* scheme,
                                          url_parse::Component* host);

  // Code that wants to format URLs with a format flag including
  // net::kFormatUrlOmitTrailingSlashOnBareHostname risk changing the meaning if
  // the result is then parsed as AutocompleteInput.  Such code can call this
  // function with the URL and its formatted string, and it will return a
  // formatted string with the same meaning as the original URL (i.e. it will
  // re-append a slash if necessary).
  static string16 FormattedStringWithEquivalentMeaning(
      const GURL& url,
      const string16& formatted_url);

  // Returns the number of non-empty components in |parts| besides the host.
  static int NumNonHostComponents(const url_parse::Parsed& parts);

  // Returns whether |text| begins "http:" or "view-source:http:".
  static bool HasHTTPScheme(const string16& text);

  // User-provided text to be completed.
  const string16& text() const { return text_; }

  // Returns 0-based cursor position within |text_| or string16::npos if not
  // used.
  size_t cursor_position() const { return cursor_position_; }

  // Use of this setter is risky, since no other internal state is updated
  // besides |text_|, |cursor_position_| and |parts_|.  Only callers who know
  // that they're not changing the type/scheme/etc. should use this.
  void UpdateText(const string16& text,
                  size_t cursor_position,
                  const url_parse::Parsed& parts);

  // The current URL, or an invalid GURL if query refinement is not desired.
  const GURL& current_url() const { return current_url_; }

  // The type of page that is currently behind displayed and how it is
  // displayed (e.g., with search term replacement or without).
  AutocompleteInput::PageClassification current_page_classification() const {
    return current_page_classification_;
  }

  // The type of input supplied.
  Type type() const { return type_; }

  // Returns parsed URL components.
  const url_parse::Parsed& parts() const { return parts_; }

  // The scheme parsed from the provided text; only meaningful when type_ is
  // URL.
  const string16& scheme() const { return scheme_; }

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

  // See description of enum for details.
  MatchesRequested matches_requested() const { return matches_requested_; }

  // Resets all internal variables to the null-constructed state.
  void Clear();

 private:
  FRIEND_TEST_ALL_PREFIXES(AutocompleteProviderTest, GetDestinationURL);

  // NOTE: Whenever adding a new field here, please make sure to update Clear()
  // method.
  string16 text_;
  size_t cursor_position_;
  GURL current_url_;
  AutocompleteInput::PageClassification current_page_classification_;
  Type type_;
  url_parse::Parsed parts_;
  string16 scheme_;
  GURL canonicalized_url_;
  bool prevent_inline_autocomplete_;
  bool prefer_keyword_;
  bool allow_exact_keyword_match_;
  MatchesRequested matches_requested_;
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_INPUT_H_
