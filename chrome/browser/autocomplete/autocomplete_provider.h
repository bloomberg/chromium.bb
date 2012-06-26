// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_PROVIDER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_PROVIDER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/string16.h"
#include "chrome/browser/autocomplete/autocomplete_types.h"
#include "chrome/common/metrics/proto/omnibox_event.pb.h"

class AutocompleteInput;
struct AutocompleteMatch;
class AutocompleteProviderListener;
class GURL;
class Profile;

// A single result provider for the autocomplete system.  Given user input, the
// provider decides what (if any) matches to return, their relevance, and their
// classifications.
class AutocompleteProvider
    : public base::RefCountedThreadSafe<AutocompleteProvider> {
 public:

  AutocompleteProvider(AutocompleteProviderListener* listener,
                       Profile* profile,
                       const char* name);

  // Called to start an autocomplete query.  The provider is responsible for
  // tracking its matches for this query and whether it is done processing the
  // query.  When new matches are available or the provider finishes, it
  // calls the controller's OnProviderUpdate() method.  The controller can then
  // get the new matches using the provider's accessors.
  // Exception: Matches available immediately after starting the query (that
  // is, synchronously) do not cause any notifications to be sent.  The
  // controller is expected to check for these without prompting (since
  // otherwise, starting each provider running would result in a flurry of
  // notifications).
  //
  // Once Stop() has been called, no more notifications should be sent.
  //
  // |minimal_changes| is an optimization that lets the provider do less work
  // when the |input|'s text hasn't changed.  See the body of
  // OmniboxPopupModel::StartAutocomplete().
  virtual void Start(const AutocompleteInput& input, bool minimal_changes) = 0;

  // Called when a provider must not make any more callbacks for the current
  // query. This will be called regardless of whether the provider is already
  // done.
  virtual void Stop();

  // Returns the set of matches for the current query.
  const ACMatches& matches() const { return matches_; }

  // Returns whether the provider is done processing the query.
  bool done() const { return done_; }

  // Returns the name of this provider.
  const std::string& name() const { return name_; }

  // Returns the enum equivalent to the name of this provider.
  metrics::OmniboxEventProto_ProviderType AsOmniboxEventProviderType() const;

  // Called to delete a match and the backing data that produced it.  This
  // match should not appear again in this or future queries.  This can only be
  // called for matches the provider marks as deletable.  This should only be
  // called when no query is running.
  // NOTE: Remember to call OnProviderUpdate() if matches_ is updated.
  virtual void DeleteMatch(const AutocompleteMatch& match);

  // Called when an omnibox event log entry is generated.  This gives
  // a provider the opportunity to add diagnostic information to the
  // logs.  A provider is expected to append a single entry of whatever
  // information it wants to |provider_info|.
  virtual void AddProviderInfo(ProvidersInfo* provider_info) const;

  // A convenience function to call net::FormatUrl() with the current set of
  // "Accept Languages" when check_accept_lang is true.  Otherwise, it's called
  // with an empty list.
  string16 StringForURLDisplay(const GURL& url,
                               bool check_accept_lang,
                               bool trim_http) const;

#ifdef UNIT_TEST
  void set_listener(AutocompleteProviderListener* listener) {
    listener_ = listener;
  }
#endif
  // A suggested upper bound for how many matches a provider should return.
  // TODO(pkasting): http://b/1111299 , http://b/933133 This should go away once
  // we have good relevance heuristics; the controller should handle all
  // culling.
  static const size_t kMaxMatches;

 protected:
  friend class base::RefCountedThreadSafe<AutocompleteProvider>;

  virtual ~AutocompleteProvider();

  // Returns whether |input| begins "http:" or "view-source:http:".
  static bool HasHTTPScheme(const string16& input);

  // Updates the starred state of each of the matches in matches_ from the
  // profile's bookmark bar model.
  void UpdateStarredStateOfMatches();

  // The profile associated with the AutocompleteProvider.  Reference is not
  // owned by us.
  Profile* profile_;

  AutocompleteProviderListener* listener_;
  ACMatches matches_;
  bool done_;

  // The name of this provider.  Used for logging.
  std::string name_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutocompleteProvider);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_AUTOCOMPLETE_PROVIDER_H_
