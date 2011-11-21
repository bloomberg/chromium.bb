// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/search_engines/search_host_to_urls_map.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Extension;
class GURL;
class PrefService;
class PrefSetObserver;
class Profile;
class SearchHostToURLsMap;
class SearchTermsData;
class SyncData;
class TemplateURLServiceObserver;

namespace history {
struct URLVisitedDetails;
}

// TemplateURLService is the backend for keywords. It's used by
// KeywordAutocomplete.
//
// TemplateURLService stores a vector of TemplateURLs. The TemplateURLs are
// persisted to the database maintained by WebDataService. *ALL* mutations
// to the TemplateURLs must funnel through TemplateURLService. This allows
// TemplateURLService to notify listeners of changes as well as keep the
// database in sync.
//
// There is a TemplateURLService per Profile.
//
// TemplateURLService does not load the vector of TemplateURLs in its
// constructor (except for testing). Use the Load method to trigger a load.
// When TemplateURLService has completed loading, observers are notified via
// OnTemplateURLServiceChanged as well as the TEMPLATE_URL_SERVICE_LOADED
// notification message.
//
// TemplateURLService takes ownership of any TemplateURL passed to it. If there
// is a WebDataService, deletion is handled by WebDataService, otherwise
// TemplateURLService handles deletion.

class TemplateURLService : public WebDataServiceConsumer,
                           public ProfileKeyedService,
                           public content::NotificationObserver,
                           public SyncableService {
 public:
  typedef std::map<std::string, std::string> QueryTerms;
  typedef std::vector<const TemplateURL*> TemplateURLVector;
  // Type for a static function pointer that acts as a time source.
  typedef base::Time(TimeProvider)();
  typedef std::map<std::string, SyncData> SyncDataMap;

  // Struct used for initializing the data store with fake data.
  // Each initializer is mapped to a TemplateURL.
  struct Initializer {
    const char* const keyword;
    const char* const url;
    const char* const content;
  };

  explicit TemplateURLService(Profile* profile);
  // The following is for testing.
  TemplateURLService(const Initializer* initializers, const int count);
  virtual ~TemplateURLService();

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

  // Returns that TemplateURL with the specified GUID, or NULL if not found.
  // The caller should not try to delete the returned pointer; the data store
  // retains ownership of it.
  const TemplateURL* GetTemplateURLForGUID(const std::string& sync_guid) const;

  // Returns the first TemplateURL found with a URL using the specified |host|,
  // or NULL if there are no such TemplateURLs
  const TemplateURL* GetTemplateURLForHost(const std::string& host) const;

  // Adds a new TemplateURL to this model. TemplateURLService will own the
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
  // by TemplateURLService and should not be deleted.
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

  // Returns the default search provider. If the TemplateURLService hasn't been
  // loaded, the default search provider is pulled from preferences.
  //
  // NOTE: At least in unittest mode, this may return NULL.
  const TemplateURL* GetDefaultSearchProvider();

  // Returns true if the default search is managed through group policy.
  bool is_default_search_managed() const { return is_default_search_managed_; }

  // Returns the default search specified in the prepopulated data, if it
  // exists.  If not, returns first URL in |template_urls_|, or NULL if that's
  // empty. The returned object is owned by TemplateURLService and can be
  // destroyed at any time so should be used right after the call.
  const TemplateURL* FindNewDefaultSearchProvider();

  // Observers used to listen for changes to the model.
  // TemplateURLService does NOT delete the observers when deleted.
  void AddObserver(TemplateURLServiceObserver* observer);
  void RemoveObserver(TemplateURLServiceObserver* observer);

  // Loads the keywords. This has no effect if the keywords have already been
  // loaded.
  // Observers are notified when loading completes via the method
  // OnTemplateURLServiceChanged.
  void Load();

#if defined(UNIT_TEST)
  void set_loaded(bool value) { loaded_ = value; }
#endif

  // Whether or not the keywords have been loaded.
  bool loaded() { return loaded_; }

  // Notification that the keywords have been loaded.
  // This is invoked from WebDataService, and should not be directly
  // invoked.
  virtual void OnWebDataServiceRequestDone(
      WebDataService::Handle h,
      const WDTypedResult* result) OVERRIDE;

  // Returns the locale-direction-adjusted short name for the given keyword.
  // Also sets the out param to indicate whether the keyword belongs to an
  // extension.
  string16 GetKeywordShortName(const string16& keyword,
                               bool* is_extension_keyword);

  // content::NotificationObserver method. TemplateURLService listens for three
  // notification types:
  // . NOTIFY_HISTORY_URL_VISITED: adds keyword search terms if the visit
  //   corresponds to a keyword.
  // . NOTIFY_GOOGLE_URL_UPDATED: updates mapping for any keywords containing
  //   a google base url replacement term.
  // . PREF_CHANGED: checks whether the default search engine has changed.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // SyncableService implementation.

  // Returns all syncable TemplateURLs from this model as SyncData. This should
  // include every search engine and no Extension keywords.
  virtual SyncDataList GetAllSyncData(syncable::ModelType type) const OVERRIDE;
  // Process new search engine changes from Sync, merging them into our local
  // data. This may send notifications if local search engines are added,
  // updated or removed.
  virtual SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const SyncChangeList& change_list) OVERRIDE;
  // Merge initial search engine data from Sync and push any local changes up
  // to Sync. This may send notifications if local search engines are added,
  // updated or removed.
  virtual SyncError MergeDataAndStartSyncing(
      syncable::ModelType type,
      const SyncDataList& initial_sync_data,
      SyncChangeProcessor* sync_processor) OVERRIDE;
  virtual void StopSyncing(syncable::ModelType type) OVERRIDE;

  // Processes a local TemplateURL change for Sync. |turl| is the TemplateURL
  // that has been modified, and |type| is the Sync ChangeType that took place.
  // This may send a new SyncChange to the cloud. If our model has not yet been
  // associated with Sync, or if this is triggered by a Sync change, then this
  // does nothing.
  void ProcessTemplateURLChange(const TemplateURL* turl,
                                SyncChange::SyncChangeType type);

  Profile* profile() const { return profile_; }

  void SetSearchEngineDialogSlot(int slot) {
    search_engine_dialog_chosen_slot_ = slot;
  }

  int GetSearchEngineDialogSlot() const {
    return search_engine_dialog_chosen_slot_;
  }

  // Returns a SyncData with a sync representation of the search engine data
  // from |turl|.
  static SyncData CreateSyncDataFromTemplateURL(const TemplateURL& turl);

  // Returns a heap-allocated TemplateURL, populated by |sync_data|'s fields.
  // This does the opposite of CreateSyncDataFromTemplateURL. The caller owns
  // the returned TemplateURL*.
  static TemplateURL* CreateTemplateURLFromSyncData(const SyncData& sync_data);

  // Returns a map mapping Sync GUIDs to pointers to SyncData.
  static SyncDataMap CreateGUIDToSyncDataMap(const SyncDataList& sync_data);

#if defined(UNIT_TEST)
  // Set a different time provider function, such as
  // base::MockTimeProvider::StaticNow, when testing calls to base::Time::Now.
  void set_time_provider(TimeProvider* time_provider) {
    time_provider_ = time_provider;
  }
#endif

 protected:
  // Cover method for the method of the same name on the HistoryService.
  // url is the one that was visited with the given search terms.
  //
  // This exists and is virtual for testing.
  virtual void SetKeywordSearchTermsForURL(const TemplateURL* t_url,
                                           const GURL& url,
                                           const string16& term);

 private:
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, BuildQueryTerms);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, TestManagedDefaultSearch);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest,
                           UpdateKeywordSearchTermsForURL);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest,
                           DontUpdateKeywordSearchForNonReplaceable);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, ChangeGoogleBaseValue);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceTest, MergeDeletesUnusedProviders);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceSyncTest,
                           CreateSyncDataFromTemplateURL);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceSyncTest,
                           CreateTemplateURLFromSyncData);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceSyncTest, UniquifyKeyword);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceSyncTest,
                           ResolveSyncKeywordConflict);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceSyncTest,
                           FindDuplicateOfSyncTemplateURL);
  FRIEND_TEST_ALL_PREFIXES(TemplateURLServiceSyncTest,
                           MergeSyncAndLocalURLDuplicates);

  friend class TemplateURLServiceTestUtil;

  typedef std::map<string16, const TemplateURL*> KeywordToTemplateMap;
  typedef std::map<std::string, const TemplateURL*> GUIDToTemplateMap;

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

  // If there is a notification service, sends TEMPLATE_URL_SERVICE_LOADED
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
      std::map<std::string, std::string>* query_terms);

  // Invoked when the Google base URL has changed. Updates the mapping for all
  // TemplateURLs that have a replacement term of {google:baseURL} or
  // {google:baseSuggestURL}.
  void GoogleBaseURLChanged();

  // Update the default search.  Called at initialization or when a managed
  // preference has changed.
  void UpdateDefaultSearch();

  // Set the default search provider even if it is managed. |url| may be null.
  // Caller is responsible for notifying observers.
  void SetDefaultSearchProviderNoNotify(const TemplateURL* url);

  // Adds a new TemplateURL to this model. TemplateURLService will own the
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

  // Removes from the vector any template URL that was created because of
  // policy.  These TemplateURLs are freed and removed from the database.
  // Sets default_search_provider to NULL if it was one of them, unless it is
  // the same as the current default from preferences and it is managed.
  void RemoveProvidersCreatedByPolicy(
      std::vector<TemplateURL*>* template_urls,
      const TemplateURL** default_search_provider,
      const TemplateURL* default_from_prefs);

  // Resets the sync GUID of the specified TemplateURL and persists the change
  // to the database. This does not notify observers.
  void ResetTemplateURLGUID(const TemplateURL* url, const std::string& guid);

  // Attempts to generate a unique keyword for |turl| based on its original
  // keyword. If its keyword is already unique, that is returned. Otherwise, it
  // tries to return the autogenerated keyword if that is unique to the Service,
  // and finally it repeatedly appends special characters to the keyword until
  // it is unique to the Service.
  string16 UniquifyKeyword(const TemplateURL& turl) const;

  // Given a TemplateURL from Sync (cloud), resolves any keyword conflicts by
  // checking the local keywords and uniquifying either the cloud keyword or a
  // conflicting local keyword (whichever is older). If the cloud TURL is
  // changed, then an appropriate SyncChange is appended to |change_list|. If
  // a local TURL is changed, the service is updated with the new keyword. If
  // there was no conflict to begin with, this does nothing. In the case of tied
  // last_modified dates, |sync_turl| wins. Returns true iff there was a
  // conflict.
  bool ResolveSyncKeywordConflict(TemplateURL* sync_turl,
                                  SyncChangeList* change_list);

  // Returns a TemplateURL from the service that has the same keyword and search
  // URL as |sync_turl|, if it exists.
  const TemplateURL* FindDuplicateOfSyncTemplateURL(
      const TemplateURL& sync_turl);

  // Given a TemplateURL from the cloud and a local matching duplicate found by
  // FindDuplcateOfSyncTemplateURL, merges the two. If |sync_url| is newer, this
  // replaces |local_url| with |sync_url| using the service's Remove and Add.
  // If |local_url| is newer, this copies the GUID from |sync_url| over to
  // |local_url| and adds an update to change_list to notify the server of the
  // change. This method takes ownership of |sync_url|, and adds it to the model
  // if it is newer, so the caller must release it if need be.
  void MergeSyncAndLocalURLDuplicates(TemplateURL* sync_url,
                                      TemplateURL* local_url,
                                      SyncChangeList* change_list);

  // Checks a newly added TemplateURL from Sync by its sync_guid and sets it as
  // the default search provider if we were waiting for it.
  void SetDefaultSearchProviderIfNewlySynced(const std::string& guid);

  // Retrieve the pending default search provider according to Sync. Returns
  // NULL if there was no pending search provider from Sync.
  const TemplateURL* GetPendingSyncedDefaultSearchProvider();

  // Goes through a vector of TemplateURLs and ensure that both the in-memory
  // and database copies have valid sync_guids. This is to fix crbug.com/102038,
  // where old entries were being pushed to Sync without a sync_guid.
  void PatchMissingSyncGUIDs(std::vector<TemplateURL*>* template_urls);

  content::NotificationRegistrar registrar_;

  // Mapping from keyword to the TemplateURL.
  KeywordToTemplateMap keyword_to_template_map_;

  // Mapping from Sync GUIDs to the TemplateURL.
  GUIDToTemplateMap guid_to_template_map_;

  TemplateURLVector template_urls_;

  ObserverList<TemplateURLServiceObserver> model_observers_;

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

  // Function returning current time in base::Time units.
  TimeProvider* time_provider_;

  // Do we have an active association between the TemplateURLs and sync models?
  // Set in MergeDataAndStartSyncing, reset in StopSyncing. While this is not
  // set, we ignore any local search engine changes (when we start syncing we
  // will look up the most recent values anyways).
  bool models_associated_;

  // Whether we're currently processing changes from the syncer. While this is
  // true, we ignore any local search engine changes, since we triggered them.
  bool processing_syncer_changes_;

  // Sync's SyncChange handler. We push all our changes through this.
  SyncChangeProcessor* sync_processor_;

  // Whether or not we are waiting on the default search provider to come in
  // from Sync. This is to facilitate the fact that changes to the value of
  // prefs::kSyncedDefaultSearchProviderGUID do not always come before the
  // TemplateURL entry it refers to, and to handle the case when we want to use
  // the Synced default when the default search provider becomes unmanaged.
  bool pending_synced_default_search_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLService);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_H_
