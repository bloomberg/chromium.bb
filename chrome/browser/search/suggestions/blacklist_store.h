// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_SUGGESTIONS_BLACKLIST_STORE_H_
#define CHROME_BROWSER_SEARCH_SUGGESTIONS_BLACKLIST_STORE_H_

#include "base/macros.h"
#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "url/gurl.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace suggestions {

// A helper class for reading, writing and modifying a small blacklist stored
// in the Profile preferences. It also handles SuggestionsProfile
// filtering based on the stored blacklist.
class BlacklistStore {
 public:
  explicit BlacklistStore(PrefService* profile_prefs);
  virtual ~BlacklistStore();

  // Returns true if successful or |url| was already in the blacklist.
  virtual bool BlacklistUrl(const GURL& url);

  // Sets |url| to the first URL from the blacklist. Returns false if the
  // blacklist is empty.
  virtual bool GetFirstUrlFromBlacklist(GURL* url);

  // Removes |url| from the stored blacklist. Returns true if successful or if
  // |url| is not in the blacklist.
  virtual bool RemoveUrl(const GURL& url);

  // Applies the blacklist to |suggestions|.
  virtual void FilterSuggestions(SuggestionsProfile* suggestions);

  // Register BlacklistStore related prefs in the Profile prefs.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 protected:
  // Test seam. For simplicity of mock creation.
  BlacklistStore() {}

  // Loads the blacklist data from the Profile preferences into
  // |blacklist|. If there is a problem with loading, the pref value is
  // cleared, false is returned and |blacklist| is cleared. If successful,
  // |blacklist| will contain the loaded data and true is returned.
  bool LoadBlacklist(SuggestionsBlacklist* blacklist);

  // Stores the provided |blacklist| to the Profile preferences, using
  // a base64 encoding of its protobuf serialization.
  bool StoreBlacklist(const SuggestionsBlacklist& blacklist);

  // Clears any blacklist data from the profile's preferences.
  void ClearBlacklist();

 private:
  // The pref service used to persist the suggestions blacklist.
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(BlacklistStore);
};

}  // namespace suggestions

#endif  // CHROME_BROWSER_SEARCH_SUGGESTIONS_BLACKLIST_STORE_H_
