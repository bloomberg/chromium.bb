// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_H_
#pragma once

#include <list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/search_engines/template_url_id.h"
#include "chrome/browser/sync/api/sync_change.h"
#include "chrome/browser/sync/api/syncable_service.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Extension;
class GURL;
class PrefService;
class Profile;
class SearchHostToURLsMap;
class SearchTermsData;
class SyncData;
class SyncErrorFactory;
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
  typedef std::vector<TemplateURL*> TemplateURLVector;
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

  // Generates a suitable keyword for the specified url, which must be valid.
  // This is guaranteed not to return an empty string, since TemplateURLs should
  // never have an empty keyword.
  static string16 GenerateKeyword(const GURL& url);

  // Removes any unnecessary characters from a user input keyword.
  // This removes the leading scheme, "www." and any trailing slash.
  static string16 CleanUserInputKeyword(const string16& keyword);

  // Returns the search url for t_url.  Returns an empty GURL if t_url has no
  // url().
  // NOTE: |t_url| is non-const in this version because of the need to access
  // t_url->profile().
  static GURL GenerateSearchURL(TemplateURL* t_url);

  // Just like GenerateSearchURL except that it takes SearchTermsData to supply
  // the data for some search terms, e.g. so this can be used on threads other
  // than the UI thread.  See the various TemplateURLRef::XXXUsingTermsData()
  // functions.
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
                         TemplateURL** template_url_to_replace);

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
  TemplateURL* GetTemplateURLForKeyword(const string16& keyword);

  // Returns that TemplateURL with the specified GUID, or NULL if not found.
  // The caller should not try to delete the returned pointer; the data store
  // retains ownership of it.
  TemplateURL* GetTemplateURLForGUID(const std::string& sync_guid);

  // Returns the first TemplateURL found with a URL using the specified |host|,
  // or NULL if there are no such TemplateURLs
  TemplateURL* GetTemplateURLForHost(const std::string& host);

  // Takes ownership of |template_url| and adds it to this model.  For obvious
  // reasons, it is illegal to Add() the same |template_url| pointer twice.
  void Add(TemplateURL* template_url);

  // Like Add(), but overwrites the |template_url|'s values with the provided
  // ones.
  void AddAndSetProfile(TemplateURL* template_url, Profile* profile);
  void AddWithOverrides(TemplateURL* template_url,
                        const string16& short_name,
                        const string16& keyword,
                        const std::string& url);

  // Removes the keyword from the model. This deletes the supplied TemplateURL.
  // This fails if the supplied template_url is the default search provider.
  void Remove(TemplateURL* template_url);

  // Removes all auto-generated keywords that were created on or after the
  // date passed in.
  void RemoveAutoGeneratedSince(base::Time created_after);

  // Removes all auto-generated keywords that were created in the specified
  // range.
  void RemoveAutoGeneratedBetween(base::Time created_after,
                                  base::Time created_before);

  // Removes all auto-generated keywords that were created in the specified
  // range for a specified |origin|. If |origin| is empty, deletes all
  // auto-generated keywords in the range.
  void RemoveAutoGeneratedForOriginBetween(const GURL& origin,
                                           base::Time created_after,
                                           base::Time created_before);

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
  TemplateURL* GetTemplateURLForExtension(const Extension* extension);

  // Returns the set of URLs describing the keywords. The elements are owned
  // by TemplateURLService and should not be deleted.
  TemplateURLVector GetTemplateURLs();

  // Increment the usage count of a keyword.
  // Called when a URL is loaded that was generated from a keyword.
  void IncrementUsageCount(TemplateURL* url);

  // Resets the title, keyword and search url of the specified TemplateURL.
  // The TemplateURL is marked as not replaceable.
  void ResetTemplateURL(TemplateURL* url,
                        const string16& title,
                        const string16& keyword,
                        const std::string& search_url);

  // Return true if the given |url| can be made the default.
  bool CanMakeDefault(const TemplateURL* url);

  // Set the default search provider.  |url| may be null.
  // This will assert if the default search is managed; the UI should not be
  // invoking this method in that situation.
  void SetDefaultSearchProvider(TemplateURL* url);

  // Returns the default search provider. If the TemplateURLService hasn't been
  // loaded, the default search provider is pulled from preferences.
  //
  // NOTE: At least in unittest mode, this may return NULL.
  TemplateURL* GetDefaultSearchProvider();

  // Returns true if the default search is managed through group policy.
  bool is_default_search_managed() const { return is_default_search_managed_; }

  // Returns the default search specified in the prepopulated data, if it
  // exists.  If not, returns first URL in |template_urls_|, or NULL if that's
  // empty. The returned object is owned by TemplateURLService and can be
  // destroyed at any time so should be used right after the call.
  TemplateURL* FindNewDefaultSearchProvider();

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
      scoped_ptr<SyncChangeProcessor> sync_processor,
      scoped_ptr<SyncErrorFactory> sync_error_factory) OVERRIDE;
  virtual void StopSyncing(syncable::ModelType type) OVERRIDE;

  // Processes a local TemplateURL change for Sync. |turl| is the TemplateURL
  // that has been modified, and |type| is the Sync ChangeType that took place.
  // This may send a new SyncChange to the cloud. If our model has not yet been
  // associated with Sync, or if this is triggered by a Sync change, then this
  // does nothing.
  void ProcessTemplateURLChange(const TemplateURL* turl,
                                SyncChange::SyncChangeType type);

  Profile* profile() const { return profile_; }

  // Returns a SyncData with a sync representation of the search engine data
  // from |turl|.
  static SyncData CreateSyncDataFromTemplateURL(const TemplateURL& turl);

  // Creates a new heap-allocated TemplateURL* which is populated by overlaying
  // |sync_data| atop |existing_turl|.  |existing_turl| may be NULL; if not it
  // remains unmodified.  The caller owns the returned TemplateURL*.
  //
  // If the created TemplateURL is migrated in some way from out-of-date sync
  // data, an appropriate SyncChange is added to |change_list|.  If the sync
  // data is bad for some reason, an ACTION_DELETE change is added and the
  // function returns NULL.
  static TemplateURL* CreateTemplateURLFromTemplateURLAndSyncData(
      Profile* profile,
      TemplateURL* existing_turl,
      const SyncData& sync_data,
      SyncChangeList* change_list);

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

  typedef std::map<string16, TemplateURL*> KeywordToTemplateMap;
  typedef std::map<std::string, TemplateURL*> GUIDToTemplateMap;
  typedef std::list<std::string> PendingExtensionIDs;

  // Helper functor for FindMatchingKeywords(), for finding the range of
  // keywords which begin with a prefix.
  class LessWithPrefix;

  void Init(const Initializer* initializers, int num_initializers);

  void RemoveFromMaps(TemplateURL* template_url);

  // Removes the supplied template_url from the keyword maps. This searches
  // through all entries in the keyword map and does not generate the host or
  // keyword. This is used when the cached content of the TemplateURL changes.
  void RemoveFromKeywordMapByPointer(TemplateURL* template_url);

  void AddToMaps(TemplateURL* template_url);

  // Sets the keywords. This is used once the keywords have been loaded.
  // This does NOT notify the delegate or the database.
  void SetTemplateURLs(const TemplateURLVector& urls);

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
                                TemplateURL** to_replace);

  // Returns true if the TemplateURL is replaceable. This doesn't look at the
  // uniqueness of the keyword or host and is intended to be called after those
  // checks have been done. This returns true if the TemplateURL doesn't appear
  // in the default list and is marked as safe_for_autoreplace.
  bool CanReplace(const TemplateURL* t_url);

  // Like GetTemplateURLForKeyword(), but ignores extension-provided keywords.
  TemplateURL* FindNonExtensionTemplateURLForKeyword(const string16& keyword);

  // Updates the information in |existing_turl| using the information from
  // |new_values|, but the ID for |existing_turl| is retained.  Notifying
  // observers is the responsibility of the caller.  Returns whether
  // |existing_turl| was found in |template_urls_| and thus could be updated.
  //
  // NOTE: This should not be called with an extension keyword as there are no
  // updates needed in that case.
  bool UpdateNoNotify(TemplateURL* existing_turl,
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
  // Caller is responsible for notifying observers.  Returns whether |url| was
  // found in |template_urls_| and thus could be made default.
  bool SetDefaultSearchProviderNoNotify(TemplateURL* url);

  // Adds a new TemplateURL to this model. TemplateURLService will own the
  // reference, and delete it when the TemplateURL is removed.
  // If |newly_adding| is false, we assume that this TemplateURL was already
  // part of the model in the past, and therefore we don't need to do things
  // like assign it an ID or notify sync.
  // This function guarantees that on return the model will not have two
  // non-extension TemplateURLs with the same keyword.  If that means that it
  // cannot add the provided argument, it will delete it and return false.
  // Caller is responsible for notifying observers if this function returns
  // true.
  bool AddNoNotify(TemplateURL* template_url, bool newly_adding);

  // Removes the keyword from the model. This deletes the supplied TemplateURL.
  // This fails if the supplied template_url is the default search provider.
  // Caller is responsible for notifying observers.
  void RemoveNoNotify(TemplateURL* template_url);

  // Notify the observers that the model has changed.  This is done only if the
  // model is loaded.
  void NotifyObservers();

  // Removes from the vector any template URL that was created because of
  // policy.  These TemplateURLs are freed and removed from the database.
  // Sets default_search_provider to NULL if it was one of them, unless it is
  // the same as the current default from preferences and it is managed.
  void RemoveProvidersCreatedByPolicy(
      TemplateURLVector* template_urls,
      TemplateURL** default_search_provider,
      TemplateURL* default_from_prefs);

  // Resets the sync GUID of the specified TemplateURL and persists the change
  // to the database. This does not notify observers.
  void ResetTemplateURLGUID(TemplateURL* url, const std::string& guid);

  // Attempts to generate a unique keyword for |turl| based on its original
  // keyword. If its keyword is already unique, that is returned. Otherwise, it
  // tries to return the autogenerated keyword if that is unique to the Service,
  // and finally it repeatedly appends special characters to the keyword until
  // it is unique to the Service.
  string16 UniquifyKeyword(const TemplateURL& turl);

  // Given a TemplateURL from Sync (cloud) and a local, non-extension
  // TemplateURL with the same keyword, resolves the conflict by uniquifying
  // either the cloud keyword or the local keyword (whichever is older).  If the
  // cloud TURL is changed, then an appropriate SyncChange is appended to
  // |change_list|.  If a local TURL is changed, the service is updated with the
  // new keyword, and a SyncChange is also appended (though this may be deleted
  // before being sent to the server; see comments in the implementation).  In
  // the case of tied last_modified dates, |sync_turl| wins.
  //
  // Note that we never call this for conflicts with extension keywords because
  // other code (e.g. AddToMaps()) is responsible for correctly prioritizing
  // extension- vs. non-extension-based TemplateURLs with the same keyword.
  void ResolveSyncKeywordConflict(TemplateURL* sync_turl,
                                  TemplateURL* local_turl,
                                  SyncChangeList* change_list);

  // Returns a TemplateURL from the service that has the same keyword and search
  // URL as |sync_turl|, if it exists.
  TemplateURL* FindDuplicateOfSyncTemplateURL(const TemplateURL& sync_turl);

  // Given a TemplateURL from the cloud and a local matching duplicate found by
  // FindDuplicateOfSyncTemplateURL, merges the two. If |sync_turl| is newer,
  // this replaces |local_turl| with |sync_turl| using the service's Remove and
  // Add. If |local_turl| is newer, this replaces |sync_turl| with |local_turl|
  // through through adding appropriate SyncChanges to |change_list|. This
  // method takes ownership of |sync_turl|, and adds it to the model if it is
  // newer, so the caller must release it if need be.
  void MergeSyncAndLocalURLDuplicates(TemplateURL* sync_turl,
                                      TemplateURL* local_turl,
                                      SyncChangeList* change_list);

  // Checks a newly added TemplateURL from Sync by its sync_guid and sets it as
  // the default search provider if we were waiting for it.
  void SetDefaultSearchProviderIfNewlySynced(const std::string& guid);

  // Retrieve the pending default search provider according to Sync. Returns
  // NULL if there was no pending search provider from Sync.
  TemplateURL* GetPendingSyncedDefaultSearchProvider();

  // Goes through a vector of TemplateURLs and ensure that both the in-memory
  // and database copies have valid sync_guids. This is to fix crbug.com/102038,
  // where old entries were being pushed to Sync without a sync_guid.
  void PatchMissingSyncGUIDs(TemplateURLVector* template_urls);

  content::NotificationRegistrar notification_registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  // Mapping from keyword to the TemplateURL.
  KeywordToTemplateMap keyword_to_template_map_;

  // Mapping from Sync GUIDs to the TemplateURL.
  GUIDToTemplateMap guid_to_template_map_;

  TemplateURLVector template_urls_;

  ObserverList<TemplateURLServiceObserver> model_observers_;

  // Maps from host to set of TemplateURLs whose search url host is host.
  // NOTE: This is always non-NULL; we use a scoped_ptr<> to avoid circular
  // header dependencies.
  scoped_ptr<SearchHostToURLsMap> provider_map_;

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
  TemplateURL* default_search_provider_;

  // The initial search provider extracted from preferences. This is only valid
  // if we haven't been loaded or loading failed.
  scoped_ptr<TemplateURL> initial_default_search_provider_;

  // Whether the default search is managed via policy.
  bool is_default_search_managed_;

  // ID assigned to next TemplateURL added to this model. This is an ever
  // increasing integer that is initialized from the database.
  TemplateURLID next_id_;

  // List of extension IDs waiting for Load to have keywords registered.
  PendingExtensionIDs pending_extension_ids_;

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
  scoped_ptr<SyncChangeProcessor> sync_processor_;

  // Sync's error handler. We use it to create a sync error.
  scoped_ptr<SyncErrorFactory> sync_error_factory_;

  // Whether or not we are waiting on the default search provider to come in
  // from Sync. This is to facilitate the fact that changes to the value of
  // prefs::kSyncedDefaultSearchProviderGUID do not always come before the
  // TemplateURL entry it refers to, and to handle the case when we want to use
  // the Synced default when the default search provider becomes unmanaged.
  bool pending_synced_default_search_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLService);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_H_
