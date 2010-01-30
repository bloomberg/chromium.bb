// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/linked_ptr.h"
#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

class DictionaryValue;
class FilePath;
class ListValue;
class PrefService;

////////////////////////////////////////////////////////////////////////////////
//
// Blacklist Class
//
// Represents a blacklist used to protect user from privacy and annoyances.
// A blacklist is essentially a map from resource-match patterns to filter-
// attributes. Each time a resources matches a pattern the filter-attributes
// are used to determine how the browser handles the matching resource.
////////////////////////////////////////////////////////////////////////////////
class Blacklist : public base::RefCountedThreadSafe<Blacklist> {
 public:
  class Entry;
  class Provider;

  typedef std::vector<linked_ptr<Entry> > EntryList;
  typedef std::vector<linked_ptr<Provider> > ProviderList;

  // Filter attributes (more to come):
  static const unsigned int kBlockAll;
  static const unsigned int kBlockCookies;
  static const unsigned int kDontSendReferrer;
  static const unsigned int kDontSendUserAgent;
  static const unsigned int kBlockUnsecure;

  // Aggregate filter types:
  static const unsigned int kBlockRequest;
  static const unsigned int kBlockResponse;
  static const unsigned int kModifySentHeaders;
  static const unsigned int kModifyReceivedHeaders;

  // Blacklist entries come from a provider, defined by a name and source URL.
  class Provider {
   public:
    Provider() {}
    Provider(const std::string& name, const std::string& url,
             const std::wstring& pref_path)
        : name_(name),
          pref_path_(pref_path),
          url_(url) {}

    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }

    const std::wstring& pref_path() const { return pref_path_; }
    void set_pref_path(const std::wstring& pref_path) {
      pref_path_ = pref_path;
    }

    const std::string& url() const { return url_; }
    void set_url(const std::string& url) { url_ = url; }

   private:
    std::string name_;
    std::wstring pref_path_;
    std::string url_;
  };

  // A single blacklist entry which is returned when a URL matches one of
  // the patterns. Entry objects are owned by the Blacklist that stores them.
  class Entry {
   public:
    // Construct with given pattern.
    Entry(const std::string& pattern, const Provider* provider,
          bool is_exception);

    // Returns the pattern which this entry matches.
    const std::string& pattern() const { return pattern_; }

    // Bitfield of filter-attributes matching the pattern.
    unsigned int attributes() const { return attributes_; }

    // True if this entry is an exception to the blacklist.
    bool is_exception() const { return is_exception_; }

    // Provider of this blacklist entry, used for assigning blame ;)
    const Provider* provider() const { return provider_; }

    // Returns true of the given URL is blocked, assumes it matches the
    // pattern of this entry.
    bool IsBlocked(const GURL&) const;

    void AddAttributes(unsigned int attributes);

   private:
    unsigned int attributes_;

    // True if this entry is an exception to the blacklist.
    bool is_exception_;
    std::string pattern_;

    // Points to the provider of this entry, the providers are all
    // owned by the blacklist.
    const Provider* provider_;
  };

  // A request may match one or more Blacklist rules. The Match class packages
  // all the matching entries behind a single interface with access to the
  // underlying set of entries so that we can display provider information.
  // Often a match must be applied after a URLRequest has started, so it gets
  // tagged with the Match object to avoid doing lookups more than once per
  // request.
  class Match : public URLRequest::UserData {
   public:
    // Functions that return combined results from all entries.
    unsigned int attributes() const {
      return (matching_attributes_ & (~exception_attributes_));
    }
    bool IsBlocked(const GURL&) const;

    // Access to individual entries, mostly for display/logging purposes.
    const std::vector<const Entry*>& entries() const {
      return matching_entries_;
    }

   private:
    Match();
    void AddEntry(const Entry* entry);

    std::vector<const Entry*> matching_entries_;
    std::vector<const Entry*> exception_entries_;

    // Precomputed ORed attributes of matching/exception entries.
    unsigned int matching_attributes_;
    unsigned int exception_attributes_;

    friend class Blacklist;  // Only blacklist constructs and sets these.
  };

  // Constructs a blacklist and populates it from the preferences associated
  // with this profile.
  explicit Blacklist(PrefService* prefs);

#ifdef UNIT_TEST
  // Constructs an empty blacklist.
  Blacklist() : prefs_(NULL) {}
#endif

  // Destructor.
  ~Blacklist();

  // Adds a new entry to the blacklist. It is now owned by the blacklist.
  void AddEntry(Entry* entry);

  // Adds a new provider to the blacklist. It is now owned by the blacklist.
  void AddProvider(Provider* provider);

  EntryList::const_iterator entries_begin() const {
    return blacklist_.begin();
  }

  EntryList::const_iterator entries_end() const {
    return blacklist_.end();
  }

  ProviderList::const_iterator providers_begin() const {
    return providers_.begin();
  }

  ProviderList::const_iterator providers_end() const {
    return providers_.end();
  }

  // Returns a pointer to a Match structure holding all matching entries.
  // If no matching Entry is found, returns null. Ownership belongs to the
  // caller.
  Match* FindMatch(const GURL&) const;

  static void RegisterUserPrefs(PrefService* user_prefs);

  // Helper to remove cookies from a header.
  static std::string StripCookies(const std::string&);

 private:
  // Converts a GURL into the string to match against.
  static std::string GetURLAsLookupString(const GURL& url);

  // Loads the list of entries for a given provider. Returns true on success.
  bool LoadEntryPreference(const ListValue& pref, const Provider* provider);

  // Loads patterns from preferences. Returns true on success.
  bool LoadPreferences();

  // Loads a provider and subsequently all of its entries. Returns true on
  // success. If an error occurs reading a provider or one of its entries,
  // the complete provider is dropped.
  bool LoadProviderPreference(const DictionaryValue& pref,
                              const std::wstring& path);

  // Matches a pattern to a core URL which is host/path with all the other
  // optional parts (scheme, user, password, port) stripped away.
  static bool Matches(const std::string& pattern, const std::string& url);

  EntryList blacklist_;
  ProviderList providers_;

  // Preferences where blacklist entries are stored.
  PrefService* prefs_;

  FRIEND_TEST(BlacklistTest, Generic);
  FRIEND_TEST(BlacklistTest, PatternMatch);
  DISALLOW_COPY_AND_ASSIGN(Blacklist);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_H_
