// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_H_

#include <stddef.h>

#include <map>

#include "base/basictypes.h"
#include "components/autocomplete/autocomplete_match.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "url/gurl.h"

class AutocompleteInput;
class AutocompleteProvider;
class TemplateURLService;

// All matches from all providers for a particular query.  This also tracks
// what the default match should be if the user doesn't manually select another
// match.
class AutocompleteResult {
 public:
  typedef ACMatches::const_iterator const_iterator;
  typedef ACMatches::iterator iterator;

  // The "Selection" struct is the information we need to select the same match
  // in one result set that was selected in another.
  struct Selection {
    Selection()
        : provider_affinity(NULL),
          is_history_what_you_typed_match(false) {
    }

    // Clear the selection entirely.
    void Clear();

    // True when the selection is empty.
    bool empty() const {
      return destination_url.is_empty() && !provider_affinity &&
          !is_history_what_you_typed_match;
    }

    // The desired destination URL.
    GURL destination_url;

    // The desired provider.  If we can't find a match with the specified
    // |destination_url|, we'll use the best match from this provider.
    const AutocompleteProvider* provider_affinity;

    // True when this is the HistoryURLProvider's "what you typed" match.  This
    // can't be tracked using |destination_url| because its URL changes on every
    // keystroke, so if this is set, we'll preserve the selection by simply
    // choosing the new "what you typed" entry and ignoring |destination_url|.
    bool is_history_what_you_typed_match;
  };

  // Max number of matches we'll show from the various providers.
  static const size_t kMaxMatches;

  AutocompleteResult();
  ~AutocompleteResult();

  // Copies matches from |old_matches| to provide a consistant result set. See
  // comments in code for specifics.
  void CopyOldMatches(const AutocompleteInput& input,
                      const AutocompleteResult& old_matches,
                      TemplateURLService* template_url_service);

  // Adds a new set of matches to the result set.  Does not re-sort.
  void AppendMatches(const ACMatches& matches);

  // Removes duplicates, puts the list in sorted order and culls to leave only
  // the best kMaxMatches matches.  Sets the default match to the best match
  // and updates the alternate nav URL.
  void SortAndCull(const AutocompleteInput& input,
                   TemplateURLService* template_url_service);

  // Returns true if at least one match was copied from the last result.
  bool HasCopiedMatches() const;

  // Vector-style accessors/operators.
  size_t size() const;
  bool empty() const;
  const_iterator begin() const;
  iterator begin();
  const_iterator end() const;
  iterator end();

  // Returns the match at the given index.
  const AutocompleteMatch& match_at(size_t index) const;
  AutocompleteMatch* match_at(size_t index);

  // Get the default match for the query (not necessarily the first).  Returns
  // end() if there is no default match.
  const_iterator default_match() const { return default_match_; }

  // Returns true if the top match is a verbatim search or URL match (see
  // IsVerbatimType() in autocomplete_match.h), and the next match is not also
  // some kind of verbatim match.  In this case, the top match will be hidden,
  // and nothing in the dropdown will appear selected by default; hitting enter
  // will navigate to the (hidden) default match, while pressing the down arrow
  // key will select the first visible match, which is actually the second match
  // in the result set.
  //
  // Hiding the top match in these cases is possible because users should
  // already know what will happen on hitting enter from the omnibox text
  // itself, without needing to see the same text appear again, selected, just
  // below their typing.  Instead, by hiding the verbatim match, there is one
  // less line to skip over in order to visually scan downwards to see other
  // suggested matches.  This makes it more likely that users will see and
  // select useful non-verbatim matches.  (Note that hiding the verbatim match
  // this way is similar to how most other browsers' address bars behave.)
  //
  // We avoid hiding when the top two matches are both verbatim in order to
  // avoid potential confusion if a user were to see the second match just below
  // their typing and assume it would be the default action.
  //
  // Note that if the top match should be hidden and it is the only match,
  // the dropdown should be closed.
  bool ShouldHideTopMatch() const;

  // Returns true if the top match is a verbatim search or URL match (see
  // IsVerbatimType() in autocomplete_match.h), and the next match is not also
  // some kind of verbatim match.
  bool TopMatchIsStandaloneVerbatimMatch() const;

  const GURL& alternate_nav_url() const { return alternate_nav_url_; }

  // Clears the matches for this result set.
  void Reset();

  void Swap(AutocompleteResult* other);

#ifndef NDEBUG
  // Does a data integrity check on this result.
  void Validate() const;
#endif

  // Compute the "alternate navigation URL" for a given match. This is obtained
  // by interpreting the user input directly as a URL. See comments on
  // |alternate_nav_url_|.
  static GURL ComputeAlternateNavUrl(const AutocompleteInput& input,
                                     const AutocompleteMatch& match);

  // Sort |matches| by destination, taking into account demotions based on
  // |page_classification| when resolving ties about which of several
  // duplicates to keep.  The matches are also deduplicated.  If
  // |set_duplicate_matches| is true, the duplicate matches are stored in the
  // |duplicate_matches| vector of the corresponding AutocompleteMatch.
  static void DedupMatchesByDestination(
      metrics::OmniboxEventProto::PageClassification page_classification,
      bool set_duplicate_matches,
      ACMatches* matches);

 private:
  friend class AutocompleteProviderTest;

  typedef std::map<AutocompleteProvider*, ACMatches> ProviderToMatches;

#if defined(OS_ANDROID)
  // iterator::difference_type is not defined in the STL that we compile with on
  // Android.
  typedef int matches_difference_type;
#else
  typedef ACMatches::iterator::difference_type matches_difference_type;
#endif

  // Returns true if |matches| contains a match with the same destination as
  // |match|.
  static bool HasMatchByDestination(const AutocompleteMatch& match,
                                    const ACMatches& matches);

  // operator=() by another name.
  void CopyFrom(const AutocompleteResult& rhs);

  // Populates |provider_to_matches| from |matches_|.
  void BuildProviderToMatches(ProviderToMatches* provider_to_matches) const;

  // Copies matches into this result. |old_matches| gives the matches from the
  // last result, and |new_matches| the results from this result.
  void MergeMatchesByProvider(
      metrics::OmniboxEventProto::PageClassification page_classification,
      const ACMatches& old_matches,
      const ACMatches& new_matches);

  ACMatches matches_;

  const_iterator default_match_;

  // The "alternate navigation URL", if any, for this result set.  This is a URL
  // to try offering as a navigational option in case the user navigated to the
  // URL of the default match but intended something else.  For example, if the
  // user's local intranet contains site "foo", and the user types "foo", we
  // default to searching for "foo" when the user may have meant to navigate
  // there.  In cases like this, the default match will point to the "search for
  // 'foo'" result, and this will contain "http://foo/".
  GURL alternate_nav_url_;

  DISALLOW_COPY_AND_ASSIGN(AutocompleteResult);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_RESULT_H_
