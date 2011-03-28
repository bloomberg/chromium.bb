// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_MODEL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_MODEL_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class GURL;
class Extension;
class PrefService;
class Profile;
class PrefSetObserver;
class SearchHostToURLsMap;
class SearchTermsData;
class TemplateURLModelObserver;
class TemplateURLRef;

namespace history {
struct URLVisitedDetails;
}

// TemplateURLModel is the backend for keywords. It's used by
// KeywordAutocomplete.
//
// TemplateURLModel stores a vector of TemplateURLs. The TemplateURLs are
// persisted to the database maintained by WebDataService. *ALL* mutations
// to the TemplateURLs must funnel through TemplateURLModel. This allows
// TemplateURLModel to notify listeners of changes as well as keep the
// database in sync.
//
// There is a TemplateURLModel per Profile.
//
// TemplateURLModel does not load the vector of TemplateURLs in its
// constructor (except for testing). Use the Load method to trigger a load.
// When TemplateURLModel has completed loading, observers are notified via
// OnTemplateURLModelChanged as well as the TEMPLATE_URL_MODEL_LOADED
// notification message.
//
// TemplateURLModel takes ownership of any TemplateURL passed to it. If there
// is a WebDataService, deletion is handled by WebDataService, otherwise
// TemplateURLModel handles deletion.

class TemplateURLModel : public WebDataServiceConsumer,
                         public NotificationObserver {
 public:
  typedef std::map<std::string, std::string> QueryTerms;
  typedef std::vector<const TemplateURL*> TemplateURLVector;

  // Struct used for initializing the data store with fake data.
  // Each initializer is mapped to a TemplateURL.
  struct Initializer {
    const char* const keyword;
    const char* const url;
    const char* const content;
  };

  explicit TemplateURLModel(Profile* profile);
  // The following is for testing.
  TemplateURLModel(const Initializer* initializers, const int count);
  virtual ~TemplateURLModel();

  // Generates a suitable keyword for the specified url.  Returns an empty
  // string if a keyword couldn't be generated.  If |autodetected| is true, we
  // don't generate keywords for a variety of situations where we would probably
  // not want to auto-add keywords, such as keywords for searches on pages that
  // themselves come from form submissions.
  static string16 GenerateKeyword(const GURL& url, bool autodetected);

  // Removes any unnecessary characters from a user input keyword.
  // This removes the leading scheme, "www." and any trailing slash.
  static string16 CleanUserInputKeyword(const string16& keyword);

  // Returns the search url for t_url.  Returns an empty GURL if t_url has no
  // url().
  static GURL GenerateSearchURL(const TemplateURL* t_url);

  // Just like GenerateSearchURL except that it takes SearchTermsData to supply
  // the data for some search terms. Most of the time GenerateSearchURL should
  // be called.
  static GURL GenerateSearchURLUsingTermsData(
      const TemplateURL* t_url,
      const SearchTermsData& search_terms_data);

  // Returns true if there is no TemplateURL that conflicts with the
  // keyword/url pair, or there is one but it can be replaced. If there is an
  // existing keyword that can be replaced and template_url_to_replace is
  // non-NULL, template_url_to_replace is set to the keyword to replace.
  //
  // url gives the url of the search query. The url is used to avoid generating
  // a TemplateURL for an existing TemplateURL that shares the same host.
  bool CanReplaceKeyword(const string16& keyword,
                         const GURL& url,
                         const TemplateURL** template_url_to_replace);

  // Returns (in |matches|) all keywords beginning with |prefix|, sorted
  // shortest-first. If support_replacement_only is true, only keywords that
  // support replacement are returned.
  void FindMatchingKeywords(const string16& prefix,
                            bool support_replacement_only,
                            std::vector<string16>* matches) const;

  // Looks up |keyword| and returns the element it maps to.  Returns NULL if
  // the keyword was not found.
  // The caller should not try to delete the returned pointer; the data store
  // retains ownership of it.
  const TemplateURL* GetTemplateURLForKeyword(const string16& keyword) const;

  // Returns the first TemplateURL found with a URL using the specified |host|,
  // or NULL if there are no such TemplateURLs
  const TemplateURL* GetTemplateURLForHost(const std::string& host) const;

  // Adds a new TemplateURL to this model. TemplateURLModel will own the
  // reference, and delete it when the TemplateURL is removed.
  void Add(TemplateURL* template_url);

  // Removes the keyword from the model. This deletes the supplied TemplateURL.
  // This fails if the supplied template_url is the default search provider.
  void Remove(const TemplateURL* template_url);

  // Removes all auto-generated keywords that were created in the specified
  // range.
  void RemoveAutoGeneratedBetween(base::Time created_after,
                                  base::Time created_before);

  // Removes all auto-generated keywords that were created on or after the
  // date passed in.
  void RemoveAutoGeneratedSince(base::Time created_after);

  // If the given extension has an omnibox keyword, adds a TemplateURL for that
  // keyword. Only 1 keyword is allowed for a given extension. If the keyword
  // already exists for this extension, does nothing.
  void RegisterExtensionKeyword(const Extension* extension);

  // Removes the TemplateURL containing the keyword for the given extension,
  // if any.
  void UnregisterExtensionKeyword(const Extension* extension);

  // Returns the TemplateURL associated with the keyword for this extension.
  // This works by checking the extension ID, not the keyword, so it will work
  // even if the user changed the keyword.
  const TemplateURL* GetTemplateURLForExtension(
      const Extension* extension) const;

  // Returns the set of URLs describing the keywords. The elements are owned
  // by TemplateURLModel and should not be deleted.
  TemplateURLVector GetTemplateURLs() const;

  // Increment the usage count of a keyword.
  // Called when a URL is loaded that was generated from a keyword.
  void IncrementUsageCount(const TemplateURL* url);

  // Resets the title, keyword and search url of the specified TemplateURL.
  // The TemplateURL is marked as not replaceable.
  void ResetTemplateURL(const TemplateURL* url,
                        const string16& title,
                        const string16& keyword,
                        const std::string& search_url);

  // Return true if the given |url| can be made the default.
  bool CanMakeDefault(const TemplateURL* url);

  // Set the default search provider.  |url| may be null.
  // This will assert if the default search is managed; the UI should not be
  // invoking this method in that situation.
  void SetDefaultSearchProvider(const TemplateURL* url);

  // Returns the default search provider. If the TemplateURLModel hasn't been
  // loaded, the default search provider is pulled from preferences.
  //
  // NOTE: At least in unittest mode, this may return NULL.
  const TemplateURL* GetDefaultSearchProvider();

  // Returns true if the default search is managed through group policy.
  bool is_default_search_managed() const { return is_default_search_managed_; }

  // Observers used to listen for changes to the model.
  // TemplateURLModel does NOT delete the observers when deleted.
  void AddObserver(TemplateURLModelObserver* observer);
  void RemoveObserver(TemplateURLModelObserver* observer);

  // Loads the keywords. This has no effect if the keywords have already been
  // loaded.
  // Observers are notified when loading completes via the method
  // OnTemplateURLModelChanged.
  void Load();

  // Whether or not the keywords have been loaded.
  bool loaded() { return loaded_; }

  // Notification that the keywords have been loaded.
  // This is invoked from WebDataService, and should not be directly
  // invoked.
  virtual void OnWebDataServiceRequestDone(WebDataService::Handle h,
                                           const WDTypedResult* result);

  // Returns the locale-direction-adjusted short name for the given keyword.
  // Also sets the out param to indicate whether the keyword belongs to an
  // extension.
  string16 GetKeywordShortName(const string16& keyword,
                               bool* is_extension_keyword);

  // NotificationObserver method. TemplateURLModel listens for three
  // notification types:
  // . NOTIFY_HISTORY_URL_VISITED: adds keyword search terms if the visit
  //   corresponds to a keyword.
  // . NOTIFY_GOOGLE_URL_UPDATED: updates mapping for any keywords containing
  //   a google base url replacement term.
  // . PREF_CHANGED: checks whether the default search engine has changed.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Profile* profile() const { return profile_; }

  void SetSearchEngineDialogSlot(int slot) {
    search_engine_dialog_chosen_slot_ = slot;
  }

  int GetSearchEngineDialogSlot() const {
    return search_engine_dialog_chosen_slot_;
  }

  // Registers the preferences used to save a TemplateURL to prefs.
  static void RegisterUserPrefs(PrefService* prefs);

 protected:
  // Cover method for the method of the same name on the HistoryService.
  // url is the one that was visited with the given search terms.
  //
  // This exists and is virtual for testing.
  virtual void SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                           const GURL& url,
                                           const string16& term);

 private:
  FRIEND_TEST_ALL_PREFIXES(TemplateURLModelTest, BuildQueryTerms);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLModelTest, TestManagedDefaultSearch);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLModelTest,
                           UpdateKeywordSearchTermsForURL);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLModelTest,
                           DontUpdateKeywordSearchForNonReplaceable);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLModelTest, ChangeGoogleBaseValue);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLModelTest, MergeDeletesUnusedProviders);
  friend class TemplateURLModelTestUtil;

  typedef std::map<string16, const TemplateURL*> KeywordToTemplateMap;

  // Helper functor for FindMatchingKeywords(), for finding the range of
  // keywords which begin with a prefix.
  class LessWithPrefix;

  void Init(const Initializer* initializers, int num_initializers);

  void RemoveFromMaps(const TemplateURL* template_url);

  // Removes the supplied template_url from the keyword maps. This searches
  // through all entries in the keyword map and does not generate the host or
  // keyword. This is used when the cached content of the TemplateURL changes.
  void RemoveFromKeywordMapByPointer(const TemplateURL* template_url);

  void AddToMaps(const TemplateURL* template_url);

  // Sets the keywords. This is used once the keywords have been loaded.
  // This does NOT notify the delegate or the database.
  void SetTemplateURLs(const std::vector<TemplateURL*>& urls);

  // Transitions to the loaded state.
  void ChangeToLoadedState();

  // If there is a notification service, sends TEMPLATE_URL_MODEL_LOADED
  // notification.
  void NotifyLoaded();

  // Saves enough of url to preferences so that it can be loaded from
  // preferences on start up.
  void SaveDefaultSearchProviderToPrefs(const TemplateURL* url);

  // Creates a TemplateURL that was previously saved to prefs via
  // SaveDefaultSearchProviderToPrefs or set via policy.
  // Returns true if successful, false otherwise.
  // If the user or the policy has opted for no default search, this
  // returns true but default_provider is set to NULL.
  // |*is_managed| specifies whether the default is managed via policy.
  bool LoadDefaultSearchProviderFromPrefs(
      scoped_ptr<TemplateURL>* default_provider,
      bool* is_managed);

  // Returns true if there is no TemplateURL that has a search url with the
  // specified host, or the only TemplateURLs matching the specified host can
  // be replaced.
  bool CanReplaceKeywordForHost(const std::string& host,
                                const TemplateURL** to_replace);

  // Returns true if the TemplateURL is replaceable. This doesn't look at the
  // uniqueness of the keyword or host and is intended to be called after those
  // checks have been done. This returns true if the TemplateURL doesn't appear
  // in the default list and is marked as safe_for_autoreplace.
  bool CanReplace(const TemplateURL* t_url);

  // Updates the information in |existing_turl| using the information from
  // |new_values|, but the ID for |existing_turl| is retained.
  // Notifying observers is the responsibility of the caller.
  void UpdateNoNotify(const TemplateURL* existing_turl,
                      const TemplateURL& new_values);

  // Returns the preferences we use.
  PrefService* GetPrefs();

  // Iterates through the TemplateURLs to see if one matches the visited url.
  // For each TemplateURL whose url matches the visited url
  // SetKeywordSearchTermsForURL is invoked.
  void UpdateKeywordSearchTermsForURL(
      const history::URLVisitedDetails& details);

  // If necessary, generates a visit for the site http:// + t_url.keyword().
  void AddTabToSearchVisit(const TemplateURL& t_url);

  // Adds each of the query terms in the specified url whose key and value are
  // non-empty to query_terms. If a query key appears multiple times, the value
  // is set to an empty string. Returns true if there is at least one key that
  // does not occur multiple times.
  static bool BuildQueryTerms(
      const GURL& url,
      std::map<std::string,std::string>* query_terms);

  // Invoked when the Google base URL has changed. Updates the mapping for all
  // TemplateURLs that have a replacement term of {google:baseURL} or
  // {google:baseSuggestURL}.
  void GoogleBaseURLChanged();

  // Update the default search.  Called at initialization or when a managed
  // preference has changed.
  void UpdateDefaultSearch();

  // Returns the default search specified in the prepopulated data, if it
  // exists.  If not, returns first URL in |template_urls_|, or NULL if that's
  // empty.
  const TemplateURL* FindNewDefaultSearchProvider();

  // Set the default search provider even if it is managed. |url| may be null.
  // Caller is responsible for notifying observers.
  void SetDefaultSearchProviderNoNotify(const TemplateURL* url);

  // Adds a new TemplateURL to this model. TemplateURLModel will own the
  // reference, and delete it when the TemplateURL is removed.
  // Caller is responsible for notifying observers.
  void AddNoNotify(TemplateURL* template_url);

  // Removes the keyword from the model. This deletes the supplied TemplateURL.
  // This fails if the supplied template_url is the default search provider.
  // Caller is responsible for notifying observers.
  void RemoveNoNotify(const TemplateURL* template_url);

  // Notify the observers that the model has changed.  This is done only if the
  // model is loaded.
  void NotifyObservers();

  NotificationRegistrar registrar_;

  // Mapping from keyword to the TemplateURL.
  KeywordToTemplateMap keyword_to_template_map_;

  TemplateURLVector template_urls_;

  ObserverList<TemplateURLModelObserver> model_observers_;

  // Maps from host to set of TemplateURLs whose search url host is host.
  SearchHostToURLsMap provider_map_;

  // Used to obtain the WebDataService.
  // When Load is invoked, if we haven't yet loaded, the WebDataService is
  // obtained from the Profile. This allows us to lazily access the database.
  Profile* profile_;

  // Whether the keywords have been loaded.
  bool loaded_;

  // Did loading fail? This is only valid if loaded_ is true.
  bool load_failed_;

  // If non-zero, we're waiting on a load.
  WebDataService::Handle load_handle_;

  // Service used to store entries.
  scoped_refptr<WebDataService> service_;

  // All visits that occurred before we finished loading. Once loaded
  // UpdateKeywordSearchTermsForURL is invoked for each element of the vector.
  std::vector<history::URLVisitedDetails> visits_to_add_;

  // Once loaded, the default search provider.  This is a pointer to a
  // TemplateURL owned by template_urls_.
  const TemplateURL* default_search_provider_;

  // Used for UX testing. Gives the slot in the search engine dialog that was
  // chosen as the default search engine.
  int search_engine_dialog_chosen_slot_;

  // The initial search provider extracted from preferences. This is only valid
  // if we haven't been loaded or loading failed.
  scoped_ptr<TemplateURL> initial_default_search_provider_;

  // Whether the default search is managed via policy.
  bool is_default_search_managed_;

  // The set of preferences observer we use to find if the default search
  // preferences have changed.
  scoped_ptr<PrefSetObserver> default_search_prefs_;

  // ID assigned to next TemplateURL added to this model. This is an ever
  // increasing integer that is initialized from the database.
  TemplateURLID next_id_;

  // List of extension IDs waiting for Load to have keywords registered.
  std::vector<std::string> pending_extension_ids_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLModel);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_MODEL_H_
