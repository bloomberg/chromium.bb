// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_H_
#pragma once

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/time.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "googleurl/src/gurl.h"

class Profile;
class SearchTermsData;
class TemplateURL;


// TemplateURLRef -------------------------------------------------------------

// A TemplateURLRef represents a single URL within the larger TemplateURL class
// (which represents an entire "search engine", see below).  If
// SupportsReplacement() is true, this URL has placeholders in it, for which
// callers can substitute values to get a "real" URL using ReplaceSearchTerms().
//
// TemplateURLRefs always have a non-NULL |owner_| TemplateURL, which they
// access in order to get at important data like the underlying URL string or
// the associated Profile.
class TemplateURLRef {
 public:
  // Magic numbers to pass to ReplaceSearchTerms() for the |accepted_suggestion|
  // parameter.  Most callers aren't using Suggest capabilities and should just
  // pass NO_SUGGESTIONS_AVAILABLE.
  // NOTE: Because positive values are meaningful, make sure these are negative!
  enum AcceptedSuggestion {
    NO_SUGGESTION_CHOSEN = -1,
    NO_SUGGESTIONS_AVAILABLE = -2,
  };

  // Which kind of URL within our owner we are.  This allows us to get at the
  // correct string field.
  enum Type {
    SEARCH,
    SUGGEST,
    INSTANT,
  };

  TemplateURLRef(TemplateURL* owner, Type type);
  ~TemplateURLRef();

  // Returns the raw URL. None of the parameters will have been replaced.
  std::string GetURL() const;

  // Returns true if this URL supports replacement.
  bool SupportsReplacement() const;

  // Like SupportsReplacement but usable on threads other than the UI thread.
  bool SupportsReplacementUsingTermsData(
      const SearchTermsData& search_terms_data) const;

  // Returns a string that is the result of replacing the search terms in
  // the url with the specified value.  We use our owner's input encoding.
  //
  // If this TemplateURLRef does not support replacement (SupportsReplacement
  // returns false), an empty string is returned.
  std::string ReplaceSearchTerms(
      const string16& terms,
      int accepted_suggestion,
      const string16& original_query_for_suggestion) const;

  // Just like ReplaceSearchTerms except that it takes SearchTermsData to supply
  // the data for some search terms. Most of the time ReplaceSearchTerms should
  // be called.
  std::string ReplaceSearchTermsUsingTermsData(
      const string16& terms,
      int accepted_suggestion,
      const string16& original_query_for_suggestion,
      const SearchTermsData& search_terms_data) const;

  // Returns true if the TemplateURLRef is valid. An invalid TemplateURLRef is
  // one that contains unknown terms, or invalid characters.
  bool IsValid() const;

  // Like IsValid but usable on threads other than the UI thread.
  bool IsValidUsingTermsData(const SearchTermsData& search_terms_data) const;

  // Returns a string representation of this TemplateURLRef suitable for
  // display. The display format is the same as the format used by Firefox.
  string16 DisplayURL() const;

  // Converts a string as returned by DisplayURL back into a string as
  // understood by TemplateURLRef.
  static std::string DisplayURLToURLRef(const string16& display_url);

  // If this TemplateURLRef is valid and contains one search term, this returns
  // the host/path of the URL, otherwise this returns an empty string.
  const std::string& GetHost() const;
  const std::string& GetPath() const;

  // If this TemplateURLRef is valid and contains one search term, this returns
  // the key of the search term, otherwise this returns an empty string.
  const std::string& GetSearchTermKey() const;

  // Converts the specified term in our owner's encoding to a string16.
  string16 SearchTermToString16(const std::string& term) const;

  // Returns true if this TemplateURLRef has a replacement term of
  // {google:baseURL} or {google:baseSuggestURL}.
  bool HasGoogleBaseURLs() const;

 private:
  friend class TemplateURL;
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, SetPrepopulatedAndParse);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseParameterKnown);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseParameterUnknown);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLEmpty);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNoTemplateEnd);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNoKnownParameters);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLTwoParameters);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLTest, ParseURLNestedParameter);

  // Enumeration of the known types.
  enum ReplacementType {
    ENCODING,
    GOOGLE_ACCEPTED_SUGGESTION,
    GOOGLE_BASE_URL,
    GOOGLE_BASE_SUGGEST_URL,
    GOOGLE_INSTANT_ENABLED,
    GOOGLE_ORIGINAL_QUERY_FOR_SUGGESTION,
    GOOGLE_RLZ,
    GOOGLE_SEARCH_FIELDTRIAL_GROUP,
    GOOGLE_UNESCAPED_SEARCH_TERMS,
    LANGUAGE,
    SEARCH_TERMS,
  };

  // Used to identify an element of the raw url that can be replaced.
  struct Replacement {
    Replacement(ReplacementType type, size_t index)
        : type(type), index(index) {}
    ReplacementType type;
    size_t index;
  };

  // The list of elements to replace.
  typedef std::vector<struct Replacement> Replacements;

  // TemplateURLRef internally caches values to make replacement quick. This
  // method invalidates any cached values.
  void InvalidateCachedValues() const;

  // Parses the parameter in url at the specified offset. start/end specify the
  // range of the parameter in the url, including the braces. If the parameter
  // is valid, url is updated to reflect the appropriate parameter. If
  // the parameter is one of the known parameters an element is added to
  // replacements indicating the type and range of the element. The original
  // parameter is erased from the url.
  //
  // If the parameter is not a known parameter, false is returned. If this is a
  // prepopulated URL, the parameter is erased, otherwise it is left alone.
  bool ParseParameter(size_t start,
                      size_t end,
                      std::string* url,
                      Replacements* replacements) const;

  // Parses the specified url, replacing parameters as necessary. If
  // successful, valid is set to true, and the parsed url is returned. For all
  // known parameters that are encountered an entry is added to replacements.
  // If there is an error parsing the url, valid is set to false, and an empty
  // string is returned.
  std::string ParseURL(const std::string& url,
                       Replacements* replacements,
                       bool* valid) const;

  // If the url has not yet been parsed, ParseURL is invoked.
  // NOTE: While this is const, it modifies parsed_, valid_, parsed_url_ and
  // search_offset_.
  void ParseIfNecessary() const;

  // Like ParseIfNecessary but usable on threads other than the UI thread.
  void ParseIfNecessaryUsingTermsData(
      const SearchTermsData& search_terms_data) const;

  // Extracts the query key and host from the url.
  void ParseHostAndSearchTermKey(
      const SearchTermsData& search_terms_data) const;

  // The TemplateURL that contains us.  This should outlive us.
  TemplateURL* const owner_;

  // What kind of URL we are.
  const Type type_;

  // Whether the URL has been parsed.
  mutable bool parsed_;

  // Whether the url was successfully parsed.
  mutable bool valid_;

  // The parsed URL. All terms have been stripped out of this with
  // replacements_ giving the index of the terms to replace.
  mutable std::string parsed_url_;

  // Do we support replacement?
  mutable bool supports_replacements_;

  // The replaceable parts of url (parsed_url_). These are ordered by index
  // into the string, and may be empty.
  mutable Replacements replacements_;

  // Host, path and key of the search term. These are only set if the url
  // contains one search term.
  mutable std::string host_;
  mutable std::string path_;
  mutable std::string search_term_key_;

  // Whether the contained URL is a pre-populated URL.
  bool prepopulated_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLRef);
};


// TemplateURLData ------------------------------------------------------------

// The data for the TemplateURL.  Separating this into its own class allows most
// users to do SSA-style usage of TemplateURL: construct a TemplateURLData with
// whatever fields are desired, then create an immutable TemplateURL from it.
struct TemplateURLData {
  TemplateURLData();
  ~TemplateURLData();

  // A short description of the template. This is the name we show to the user
  // in various places that use TemplateURLs. For example, the location bar
  // shows this when the user selects a substituting match.
  string16 short_name;

  // The shortcut for this TemplateURL.  |keyword| must be non-empty.
  void SetKeyword(const string16& keyword);
  const string16& keyword() const { return keyword_; }

  // The raw URL for the TemplateURL, which may not be valid as-is (e.g. because
  // it requires substitutions first).  This must be non-empty.
  void SetURL(const std::string& url);
  const std::string& url() const { return url_; }

  // Optional additional raw URLs.
  std::string suggestions_url;
  std::string instant_url;

  // Optional favicon for the TemplateURL.
  GURL favicon_url;

  // URL to the OSD file this came from. May be empty.
  GURL originating_url;

  // Whether this TemplateURL is shown in the default list of search providers.
  // This is just a property and does not indicate whether the TemplateURL has a
  // TemplateURLRef that supports replacement. Use
  // TemplateURL::ShowInDefaultList() to test both.
  bool show_in_default_list;

  // Whether it's safe for auto-modification code (the autogenerator and the
  // code that imports data from other browsers) to replace the TemplateURL.
  // This should be set to false for any TemplateURL the user edits, or any
  // TemplateURL that the user clearly manually edited in the past, like a
  // bookmark keyword from another browser.
  bool safe_for_autoreplace;

  // The list of supported encodings for the search terms. This may be empty,
  // which indicates the terms should be encoded with UTF-8.
  std::vector<std::string> input_encodings;

  // Unique identifier of this TemplateURL. The unique ID is set by the
  // TemplateURLService when the TemplateURL is added to it.
  TemplateURLID id;

  // Date this TemplateURL was created.
  //
  // NOTE: this may be 0, which indicates the TemplateURL was created before we
  // started tracking creation time.
  base::Time date_created;

  // The last time this TemplateURL was modified by a user, since creation.
  //
  // NOTE: Like date_created above, this may be 0.
  base::Time last_modified;

  // True if this TemplateURL was automatically created by the administrator via
  // group policy.
  bool created_by_policy;

  // Number of times this TemplateURL has been explicitly used to load a URL.
  // We don't increment this for uses as the "default search engine" since
  // that's not really "explicit" usage and incrementing would result in pinning
  // the user's default search engine(s) to the top of the list of searches on
  // the New Tab page, de-emphasizing the omnibox as "where you go to search".
  int usage_count;

  // If this TemplateURL comes from prepopulated data the prepopulate_id is > 0.
  int prepopulate_id;

  // The primary unique identifier for Sync. This set on all TemplateURLs
  // regardless of whether they have been associated with Sync.
  std::string sync_guid;

 private:
  // Private so we can enforce using the setters and thus enforce that these
  // fields are never empty.
  string16 keyword_;
  std::string url_;
};


// TemplateURL ----------------------------------------------------------------

// A TemplateURL represents a single "search engine", defined primarily as a
// subset of the Open Search Description Document
// (http://www.opensearch.org/Specifications/OpenSearch) plus some extensions.
// One TemplateURL contains several TemplateURLRefs, which correspond to various
// different capabilities (e.g. doing searches or getting suggestions), as well
// as a TemplateURLData containing other details like the name, keyword, etc.
//
// TemplateURLs are intended to be read-only for most users; the only public
// non-const method is the Profile getter, which returns a non-const Profile*.
// The TemplateURLService, which handles storing and manipulating TemplateURLs,
// is made a friend so that it can be the exception to this pattern.
class TemplateURL {
 public:
  // |profile| may be NULL.  This will affect the results of e.g. calling
  // ReplaceSearchTerms() on the member TemplateURLRefs.
  TemplateURL(Profile* profile, const TemplateURLData& data);
  ~TemplateURL();

  // Generates a favicon URL from the specified url.
  static GURL GenerateFaviconURL(const GURL& url);

  Profile* profile() { return profile_; }
  const TemplateURLData& data() const { return data_; }

  const string16& short_name() const { return data_.short_name; }
  // An accessor for the short_name, but adjusted so it can be appropriately
  // displayed even if it is LTR and the UI is RTL.
  string16 AdjustedShortNameForLocaleDirection() const;

  const string16& keyword() const { return data_.keyword(); }

  const std::string& url() const { return data_.url(); }
  const std::string& suggestions_url() const { return data_.suggestions_url; }
  const std::string& instant_url() const { return data_.instant_url; }
  const GURL& favicon_url() const { return data_.favicon_url; }

  const GURL& originating_url() const { return data_.originating_url; }

  bool show_in_default_list() const { return data_.show_in_default_list; }
  // Returns true if show_in_default_list() is true and this TemplateURL has a
  // TemplateURLRef that supports replacement.
  bool ShowInDefaultList() const;

  bool safe_for_autoreplace() const { return data_.safe_for_autoreplace; }

  const std::vector<std::string>& input_encodings() const {
    return data_.input_encodings;
  }

  TemplateURLID id() const { return data_.id; }

  base::Time date_created() const { return data_.date_created; }
  base::Time last_modified() const { return data_.last_modified; }

  bool created_by_policy() const { return data_.created_by_policy; }

  int usage_count() const { return data_.usage_count; }

  int prepopulate_id() const { return data_.prepopulate_id; }

  const std::string& sync_guid() const { return data_.sync_guid; }

  const TemplateURLRef& url_ref() const { return url_ref_; }
  const TemplateURLRef& suggestions_url_ref() const {
    return suggestions_url_ref_;
  }
  const TemplateURLRef& instant_url_ref() const { return instant_url_ref_; }

  // Returns true if |url| supports replacement.
  bool SupportsReplacement() const;

  // Like SupportsReplacement but usable on threads other than the UI thread.
  bool SupportsReplacementUsingTermsData(
      const SearchTermsData& search_terms_data) const;

  // Returns true if this TemplateURL uses Google base URLs and has a keyword
  // of "google.TLD".  We use this to decide whether we can automatically
  // update the keyword to reflect the current Google base URL TLD.
  bool IsGoogleSearchURLWithReplaceableKeyword() const;

  // Returns true if the keywords match or if
  // IsGoogleSearchURLWithReplaceableKeyword() is true for both TemplateURLs.
  bool HasSameKeywordAs(const TemplateURL& other) const;

  std::string GetExtensionId() const;
  bool IsExtensionKeyword() const;

 private:
  friend class TemplateURLService;

  void CopyFrom(const TemplateURL& other);

  void SetURL(const std::string& url);
  void SetPrepopulateId(int id);

  // Resets the keyword if IsGoogleSearchURLWithReplaceableKeyword() or |force|.
  // The |force| parameter is useful when the existing keyword is known to be
  // a placeholder.  The resulting keyword is generated using
  // TemplateURLService::GenerateSearchURL() and
  // TemplateURLService::GenerateKeyword().
  void ResetKeywordIfNecessary(bool force);

  // Invalidates cached values on this object and its child TemplateURLRefs.
  void InvalidateCachedValues();

  Profile* profile_;
  TemplateURLData data_;
  TemplateURLRef url_ref_;
  TemplateURLRef suggestions_url_ref_;
  TemplateURLRef instant_url_ref_;

  // TODO(sky): Add date last parsed OSD file.

  DISALLOW_COPY_AND_ASSIGN(TemplateURL);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_H_
