// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request.h"

class FilePath;

////////////////////////////////////////////////////////////////////////////////
//
// Blacklist Class
//
// Represents a blacklist used to protect user from privacy and annoyances.
// A blacklist is essentially a map from resource-match patterns to filter-
// attributes. Each time a resources matches a pattern the filter-attributes
// are used to determine how the browser handles the matching resource.
//
// TODO(idanan): Implement this efficiently.
// To get things started, the initial implementation is as simple as
// it gets and cannot scale to large blacklists but it should be enough
// for testing on the order of a hundred or so entries.
//
////////////////////////////////////////////////////////////////////////////////
class Blacklist {
 public:
  // Filter attributes (more to come):
  static const unsigned int kBlockAll = 1;
  static const unsigned int kDontSendCookies = 1 << 1;
  static const unsigned int kDontStoreCookies = 1 << 2;
  static const unsigned int kDontPersistCookies = 1 << 3;
  static const unsigned int kDontSendReferrer = 1 << 4;
  static const unsigned int kDontSendUserAgent = 1 << 5;
  static const unsigned int kBlockByType = 1 << 6;
  static const unsigned int kBlockUnsecure = 1 << 7;

  // Aggregate filter types:
  static const unsigned int kBlockRequest = kBlockAll | kBlockUnsecure;
  static const unsigned int kBlockResponse = kBlockByType;
  static const unsigned int kModifySentHeaders =
      kDontSendCookies | kDontSendUserAgent | kDontSendReferrer;
  static const unsigned int kModifyReceivedHeaders =
      kDontPersistCookies | kDontStoreCookies;
  static const unsigned int kFilterByHeaders = kModifyReceivedHeaders |
      kBlockByType;

  // Key used to access data attached to URLRequest objects.
  static const void* const kRequestDataKey;

  // Takes a string an returns the matching attribute, 0 if none matches.
  static unsigned int String2Attribute(const std::string&);

  // Blacklist entries come from a provider, defined by a name and source URL.
  class Provider {
   public:
    Provider() {}
    Provider(const char* name, const char* url) : name_(name), url_(url) {}
    const std::string& name() const { return name_; }
    const std::string& url() const { return url_; }
    void set_name(const std::string& name) { name_ = name; }
    void set_url(const std::string& url) { url_ = url; }
   private:
    std::string name_;
    std::string url_;
  };

  // A single blacklist entry which is returned when a URL matches one of
  // the patterns. Entry objects are owned by the Blacklist that stores them.
  class Entry {
   public:
    // Returns the pattern which this entry matches.
    const std::string& pattern() const { return pattern_; }

    // Bitfield of filter-attributes matching the pattern.
    unsigned int attributes() const { return attributes_; }

    // Provider of this blacklist entry, used for assigning blame ;)
    const Provider* provider() const { return provider_; }

    // Returns true if the given type matches one of the types for which
    // the filter-attributes of this pattern apply. This needs only to be
    // checked for content-type specific rules, as determined by calling
    // attributes().
    bool MatchType(const std::string&) const;

    // Returns true of the given URL is blocked, assumes it matches the
    // pattern of this entry.
    bool IsBlocked(const GURL&) const;

   private:
    // Construct with given pattern.
    explicit Entry(const std::string& pattern, const Provider* provider);

    void AddAttributes(unsigned int attributes);
    void AddType(const std::string& type);

    // Merge the attributes and types of the given entry with this one.
    void Merge(const Entry& entry);

    // Swap the given vector content for the type vector for quick loading.
    void SwapTypes(std::vector<std::string>* types);

    std::string pattern_;
    unsigned int attributes_;
    std::vector<std::string> types_;

    // Points to the provider of this entry, the providers are all
    // owned by the blacklist.
    const Provider* provider_;

    friend class Blacklist;
    friend class BlacklistIO;
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
    unsigned int attributes() const { return attributes_; }
    bool MatchType(const std::string&) const;
    bool IsBlocked(const GURL&) const;

    // Access to individual entries, mostly for display/logging purposes.
    const std::vector<const Entry*>& entries() const { return entries_; }

   private:
    Match();
    void AddEntry(const Entry* entry);
    std::vector<const Entry*> entries_;
    unsigned int attributes_;  // Precomputed ORed attributes of entries.

    friend class Blacklist;  // Only blacklist constructs and sets these.
  };

  // Constructs a Blacklist given the filename of the persistent version.
  //
  // For startup efficiency, and because the blacklist must be available
  // before any http request is made (including the homepage, if one is
  // set to be loaded at startup), it is important to load the blacklist
  // from a local source as efficiently as possible. For this reason, the
  // combined rules from all active blacklists are stored in one local file.
  explicit Blacklist(const FilePath& path);

  // Destructor.
  ~Blacklist();

  // Returns a pointer to a Match structure holding all matching entries.
  // If no matching Entry is found, returns null. Ownership belongs to the
  // caller.
  Match* findMatch(const GURL&) const;

  // Helper to remove cookies from a header.
  static std::string StripCookies(const std::string&);

  // Helper to remove cookie expiration from a header.
  static std::string StripCookieExpiry(const std::string&);

 private:
  // Matches a pattern to a core URL which is host/path with all the other
  // optional parts (scheme, user, password, port) stripped away. Used only
  // internally but made static so that access can be given to tests.
  static bool Matches(const std::string& pattern, const std::string& url);

  std::vector<Entry*> blacklist_;
  std::vector<Provider*> providers_;

  FRIEND_TEST(BlacklistTest, Generic);
  FRIEND_TEST(BlacklistTest, PatternMatch);
  DISALLOW_COPY_AND_ASSIGN(Blacklist);
};

#endif  // CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_H_
