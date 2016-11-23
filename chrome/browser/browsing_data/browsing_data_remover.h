// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
#define CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_

#include <stdint.h>

#include <queue>
#include <set>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/common/features.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/prefs/pref_member.h"
#include "components/search_engines/template_url_service.h"
#include "media/media_features.h"
#include "ppapi/features/features.h"
#include "storage/common/quota/quota_types.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/pepper_flash_settings_manager.h"
#endif

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_method_call_status.h"
#endif

class BrowsingDataFilterBuilder;
class BrowsingDataFlashLSOHelper;
class BrowsingDataRemoverFactory;
class HostContentSettingsMap;
class IOThread;
class Profile;
class WebappRegistry;

namespace chrome_browser_net {
class Predictor;
}

namespace content {
class BrowserContext;
class PluginDataRemover;
class StoragePartition;
}

namespace net {
class URLRequestContextGetter;
}

////////////////////////////////////////////////////////////////////////////////
// BrowsingDataRemover is responsible for removing data related to browsing:
// visits in url database, downloads, cookies ...
//
//  USAGE:
//
//  0. Instantiation.
//
//       BrowsingDataRemover remover =
//           BrowsingDataRemoverFactory::GetForBrowserContext(profile);
//
//  1. No observer.
//
//       remover->Remove(Unbounded(), REMOVE_COOKIES, ALL);
//
//  2. Using an observer to report when one's own removal task is finished.
//
//       class CookiesDeleter : public BrowsingDataRemover::Observer {
//         CookiesDeleter() { remover->AddObserver(this); }
//         ~CookiesDeleter() { remover->RemoveObserver(this); }
//
//         void DeleteCookies() {
//           remover->RemoveAndReply(Unbounded(), REMOVE_COOKIES, ALL, this);
//         }
//
//         void OnBrowsingDataRemoverDone() {
//           LOG(INFO) << "Cookies were deleted.";
//         }
//       }
//
////////////////////////////////////////////////////////////////////////////////

class BrowsingDataRemover : public KeyedService
#if BUILDFLAG(ENABLE_PLUGINS)
                            , public PepperFlashSettingsManager::Client
#endif
{
 public:
  // Mask used for Remove.
  enum RemoveDataMask {
    REMOVE_APPCACHE = 1 << 0,
    REMOVE_CACHE = 1 << 1,
    REMOVE_COOKIES = 1 << 2,
    REMOVE_DOWNLOADS = 1 << 3,
    REMOVE_FILE_SYSTEMS = 1 << 4,
    REMOVE_FORM_DATA = 1 << 5,
    // In addition to visits, REMOVE_HISTORY removes keywords, last session and
    // passwords UI statistics.
    REMOVE_HISTORY = 1 << 6,
    REMOVE_INDEXEDDB = 1 << 7,
    REMOVE_LOCAL_STORAGE = 1 << 8,
    REMOVE_PLUGIN_DATA = 1 << 9,
    REMOVE_PASSWORDS = 1 << 10,
    REMOVE_WEBSQL = 1 << 11,
    REMOVE_CHANNEL_IDS = 1 << 12,
    REMOVE_MEDIA_LICENSES = 1 << 13,
    REMOVE_SERVICE_WORKERS = 1 << 14,
    REMOVE_SITE_USAGE_DATA = 1 << 15,
    // REMOVE_NOCHECKS intentionally does not check if the Profile's prohibited
    // from deleting history or downloads.
    REMOVE_NOCHECKS = 1 << 16,
    REMOVE_CACHE_STORAGE = 1 << 17,
#if BUILDFLAG(ANDROID_JAVA_UI)
    REMOVE_WEBAPP_DATA = 1 << 18,
#endif
    REMOVE_DURABLE_PERMISSION = 1 << 19,

    // The following flag is used only in tests. In normal usage, hosted app
    // data is controlled by the REMOVE_COOKIES flag, applied to the
    // protected-web origin.
    REMOVE_HOSTED_APP_DATA_TESTONLY = 1 << 31,

    // "Site data" includes cookies, appcache, file systems, indexedDBs, local
    // storage, webSQL, service workers, cache storage, plugin data, web app
    // data (on Android) and statistics about passwords.
    REMOVE_SITE_DATA = REMOVE_APPCACHE | REMOVE_COOKIES | REMOVE_FILE_SYSTEMS |
                       REMOVE_INDEXEDDB |
                       REMOVE_LOCAL_STORAGE |
                       REMOVE_PLUGIN_DATA |
                       REMOVE_SERVICE_WORKERS |
                       REMOVE_CACHE_STORAGE |
                       REMOVE_WEBSQL |
                       REMOVE_CHANNEL_IDS |
#if BUILDFLAG(ANDROID_JAVA_UI)
                       REMOVE_WEBAPP_DATA |
#endif
                       REMOVE_SITE_USAGE_DATA |
                       REMOVE_DURABLE_PERMISSION,

    // Datatypes protected by Important Sites.
    IMPORTANT_SITES_DATATYPES = REMOVE_SITE_DATA |
                                REMOVE_CACHE,

    // Datatypes that can be deleted partially per URL / origin / domain,
    // whichever makes sense.
    FILTERABLE_DATATYPES = REMOVE_SITE_DATA |
                           REMOVE_CACHE |
                           REMOVE_DOWNLOADS,

    // Includes all the available remove options. Meant to be used by clients
    // that wish to wipe as much data as possible from a Profile, to make it
    // look like a new Profile.
    REMOVE_ALL = REMOVE_SITE_DATA | REMOVE_CACHE | REMOVE_DOWNLOADS |
                 REMOVE_FORM_DATA |
                 REMOVE_HISTORY |
                 REMOVE_PASSWORDS |
                 REMOVE_MEDIA_LICENSES,

    // Includes all available remove options. Meant to be used when the Profile
    // is scheduled to be deleted, and all possible data should be wiped from
    // disk as soon as possible.
    REMOVE_WIPE_PROFILE = REMOVE_ALL | REMOVE_NOCHECKS,
  };

  // Important sites protect a small set of sites from the deletion of certain
  // datatypes. Therefore, those datatypes must be filterable by
  // url/origin/domain.
  static_assert(0 == (IMPORTANT_SITES_DATATYPES & ~FILTERABLE_DATATYPES),
                "All important sites datatypes must be filterable.");

  // A helper enum to report the deletion of cookies and/or cache. Do not
  // reorder the entries, as this enum is passed to UMA.
  enum CookieOrCacheDeletionChoice {
    NEITHER_COOKIES_NOR_CACHE,
    ONLY_COOKIES,
    ONLY_CACHE,
    BOTH_COOKIES_AND_CACHE,
    MAX_CHOICE_VALUE
  };

  struct TimeRange {
    TimeRange(base::Time begin, base::Time end) : begin(begin), end(end) {}
    bool operator==(const TimeRange& other) const;

    base::Time begin;
    base::Time end;
  };

  // Observer is notified when its own removal task is done.
  class Observer {
   public:
    // Called when a removal task is finished. Note that every removal task can
    // only have one observer attached to it, and only that one is called.
    virtual void OnBrowsingDataRemoverDone() = 0;

   protected:
    virtual ~Observer() {}
  };

  // The completion inhibitor can artificially delay completion of the browsing
  // data removal process. It is used during testing to simulate scenarios in
  // which the deletion stalls or takes a very long time.
  class CompletionInhibitor {
   public:
    // Invoked when a |remover| is just about to complete clearing browser data,
    // and will be prevented from completing until after the callback
    // |continue_to_completion| is run.
    virtual void OnBrowsingDataRemoverWouldComplete(
        BrowsingDataRemover* remover,
        const base::Closure& continue_to_completion) = 0;

   protected:
    virtual ~CompletionInhibitor() {}
  };

  static TimeRange Unbounded();

  static TimeRange Period(browsing_data::TimePeriod period);

  // Is the BrowsingDataRemover currently in the process of removing data?
  bool is_removing() { return is_removing_; }

  // Sets a CompletionInhibitor, which will be notified each time an instance is
  // about to complete a browsing data removal process, and will be able to
  // artificially delay the completion.
  // TODO(crbug.com/483528): Make this non-static.
  static void set_completion_inhibitor_for_testing(
      CompletionInhibitor* inhibitor) {
    completion_inhibitor_ = inhibitor;
  }

  // Removes browsing data within the given |time_range|, with datatypes being
  // specified by |remove_mask| and origin types by |origin_type_mask|.
  void Remove(const TimeRange& time_range,
              int remove_mask,
              int origin_type_mask);

  // A version of the above that in addition informs the |observer| when the
  // removal task is finished.
  void RemoveAndReply(const TimeRange& time_range,
                      int remove_mask,
                      int origin_type_mask,
                      Observer* observer);

  // Like Remove(), but in case of URL-keyed only removes data whose URL match
  // |filter_builder| (e.g. are on certain origin or domain).
  // RemoveWithFilter() currently only works with FILTERABLE_DATATYPES.
  void RemoveWithFilter(
      const TimeRange& time_range,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder);

  // A version of the above that in addition informs the |observer| when the
  // removal task is finished.
  void RemoveWithFilterAndReply(
      const TimeRange& time_range,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
      Observer* observer);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Used for testing.
  void OverrideStoragePartitionForTesting(
      content::StoragePartition* storage_partition);

#if BUILDFLAG(ANDROID_JAVA_UI)
  void OverrideWebappRegistryForTesting(
      std::unique_ptr<WebappRegistry> webapp_registry);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  void OverrideFlashLSOHelperForTesting(
      scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper);
#endif

  // Parameters of the last call are exposed to be used by tests. Removal and
  // origin type masks equal to -1 mean that no removal has ever been executed.
  // TODO(msramek): If other consumers than tests are interested in this,
  // consider returning them in OnBrowsingDataRemoverDone() callback.
  const base::Time& GetLastUsedBeginTime();
  const base::Time& GetLastUsedEndTime();
  int GetLastUsedRemovalMask();
  int GetLastUsedOriginTypeMask();

 protected:
  // Use BrowsingDataRemoverFactory::GetForBrowserContext to get an instance of
  // this class. The constructor is protected so that the class is mockable.
  BrowsingDataRemover(content::BrowserContext* browser_context);
  ~BrowsingDataRemover() override;

  // A common reduction of all public Remove[WithFilter][AndReply] methods.
  virtual void RemoveInternal(
      const TimeRange& time_range,
      int remove_mask,
      int origin_type_mask,
      std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
      Observer* observer);

 private:
  // The clear API needs to be able to toggle removing_ in order to test that
  // only one BrowsingDataRemover instance can be called at a time.
  FRIEND_TEST_ALL_PREFIXES(ExtensionBrowsingDataTest, OneAtATime);
  // Testing the private RemovalTask.
  FRIEND_TEST_ALL_PREFIXES(BrowsingDataRemoverTest, MultipleTasks);

  // The BrowsingDataRemover tests need to be able to access the implementation
  // of Remove(), as it exposes details that aren't yet available in the public
  // API. As soon as those details are exposed via new methods, this should be
  // removed.
  //
  // TODO(mkwst): See http://crbug.com/113621
  friend class BrowsingDataRemoverTest;

  friend class BrowsingDataRemoverFactory;

  // Represents a single removal task. Contains all parameters needed to execute
  // it and a pointer to the observer that added it.
  struct RemovalTask {
    RemovalTask(const TimeRange& time_range,
                int remove_mask,
                int origin_type_mask,
                std::unique_ptr<BrowsingDataFilterBuilder> filter_builder,
                Observer* observer);
    ~RemovalTask();

    TimeRange time_range;
    int remove_mask;
    int origin_type_mask;
    std::unique_ptr<BrowsingDataFilterBuilder> filter_builder;
    Observer* observer;
  };

  void Shutdown() override;

  // Setter for |is_removing_|; DCHECKs that we can only start removing if we're
  // not already removing, and vice-versa.
  void SetRemoving(bool is_removing);

  // Callback for when TemplateURLService has finished loading. Clears the data,
  // clears the respective waiting flag, and invokes NotifyIfDone.
  void OnKeywordsLoaded(base::Callback<bool(const GURL&)> url_filter);

#if BUILDFLAG(ENABLE_PLUGINS)
  // Called when plugin data has been cleared. Invokes NotifyIfDone.
  void OnWaitableEventSignaled(base::WaitableEvent* waitable_event);

  // Called when the list of |sites| storing Flash LSO cookies is fetched.
  void OnSitesWithFlashDataFetched(
      base::Callback<bool(const std::string&)> plugin_filter,
      const std::vector<std::string>& sites);

  // Indicates that LSO cookies for one website have been deleted.
  void OnFlashDataDeleted();

  // PepperFlashSettingsManager::Client implementation.
  void OnDeauthorizeFlashContentLicensesCompleted(uint32_t request_id,
                                                  bool success) override;
#endif

#if defined (OS_CHROMEOS)
  void OnClearPlatformKeys(chromeos::DBusMethodCallStatus call_status,
                           bool result);
#endif

  // Executes the next removal task. Called after the previous task was finished
  // or directly from Remove() if the task queue was empty.
  void RunNextTask();

  // Removes the specified items related to browsing for a specific host. If the
  // provided |remove_url| is empty, data is removed for all origins; otherwise,
  // it is restricted by the origin filter origin (where implemented yet). The
  // |origin_type_mask| parameter defines the set of origins from which data
  // should be removed (protected, unprotected, or both).
  // TODO(ttr314): Remove "(where implemented yet)" constraint above once
  // crbug.com/113621 is done.
  // TODO(crbug.com/589586): Support all backends w/ origin filter.
  void RemoveImpl(const TimeRange& time_range,
                  int remove_mask,
                  const BrowsingDataFilterBuilder& filter_builder,
                  int origin_type_mask);

  // Notifies observers and transitions to the idle state.
  void Notify();

  // Checks if we are all done, and if so, calls Notify().
  void NotifyIfDone();

  // Called when history deletion is done.
  void OnHistoryDeletionDone();

  // Callback for when the hostname resolution cache has been cleared.
  // Clears the respective waiting flag and invokes NotifyIfDone.
  void OnClearedHostnameResolutionCache();

  // Callback for when HTTP auth cache has been cleared.
  // Clears the respective waiting flag and invokes NotifyIfDone.
  void OnClearedHttpAuthCache();

  // Callback for when speculative data in the network Predictor has been
  // cleared. Clears the respective waiting flag and invokes
  // NotifyIfDone.
  void OnClearedNetworkPredictor();

  // Callback for when network related data in ProfileIOData has been cleared.
  // Clears the respective waiting flag and invokes NotifyIfDone.
  void OnClearedNetworkingHistory();

  // Callback for when the cache has been deleted. Invokes
  // NotifyIfDone.
  void ClearedCache();
#if !defined(DISABLE_NACL)
  // Callback for when the NaCl cache has been deleted. Invokes
  // NotifyIfDone.
  void ClearedNaClCache();

  // Callback for when the PNaCl translation cache has been deleted. Invokes
  // NotifyIfDone.
  void ClearedPnaclCache();

#endif

  // Callback for when passwords for the requested time range have been cleared.
  void OnClearedPasswords();

  // Callback for when passwords stats for the requested time range have been
  // cleared.
  void OnClearedPasswordsStats();

  // Callback for when the autosignin state of passwords has been revoked.
  void OnClearedAutoSignIn();

  // Callback for when cookies have been deleted. Invokes NotifyIfDone.
  void OnClearedCookies();

  // Callback for when channel IDs have been deleted. Invokes
  // NotifyIfDone.
  void OnClearedChannelIDs();

  // Callback from the above method.
  void OnClearedFormData();

  // Callback for when the Autofill profile and credit card origin URLs have
  // been deleted.
  void OnClearedAutofillOriginURLs();

  // Callback on UI thread when the storage partition related data are cleared.
  void OnClearedStoragePartitionData();

#if BUILDFLAG(ENABLE_WEBRTC)
  // Callback on UI thread when the WebRTC logs have been deleted.
  void OnClearedWebRtcLogs();
#endif

#if BUILDFLAG(ANDROID_JAVA_UI)
  // Callback on UI thread when the precache history has been cleared.
  void OnClearedPrecacheHistory();

  // Callback on UI thread when the offline page data has been cleared.
  void OnClearedOfflinePageData(
      offline_pages::OfflinePageModel::DeletePageResult result);
#endif

  void OnClearedDomainReliabilityMonitor();

  // Returns true if we're all done.
  bool AllDone();

  // Profile we're to remove from.
  Profile* profile_;

  // Start time to delete from.
  base::Time delete_begin_;

  // End time to delete to.
  base::Time delete_end_;

  // The removal mask for the current removal operation.
  int remove_mask_ = 0;

  // From which types of origins should we remove data?
  int origin_type_mask_ = 0;

  // True if Remove has been invoked.
  bool is_removing_;

  // Removal tasks to be processed.
  std::queue<RemovalTask> task_queue_;

  // If non-NULL, the |completion_inhibitor_| is notified each time an instance
  // is about to complete a browsing data removal process, and has the ability
  // to artificially delay completion. Used for testing.
  static CompletionInhibitor* completion_inhibitor_;

#if BUILDFLAG(ENABLE_PLUGINS)
  // Used to delete plugin data.
  std::unique_ptr<content::PluginDataRemover> plugin_data_remover_;
  base::WaitableEventWatcher watcher_;

  // Used for per-site plugin data deletion.
  scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper_;

  // Used to deauthorize content licenses for Pepper Flash.
  std::unique_ptr<PepperFlashSettingsManager> pepper_flash_settings_manager_;
#endif

  uint32_t deauthorize_flash_content_licenses_request_id_ = 0;
  // True if we're waiting for various data to be deleted.
  // These may only be accessed from UI thread in order to avoid races!
  bool waiting_for_synchronous_clear_operations_ = false;
  bool waiting_for_clear_autofill_origin_urls_ = false;
  bool waiting_for_clear_cache_ = false;
  bool waiting_for_clear_channel_ids_ = false;
  bool waiting_for_clear_flash_content_licenses_ = false;
  // Non-zero if waiting for cookies to be cleared.
  int waiting_for_clear_cookies_count_ = 0;
  // Counts the number of plugin data tasks. Should be the number of LSO cookies
  // to be deleted, or 1 while we're fetching LSO cookies or deleting in bulk.
  int waiting_for_clear_plugin_data_count_ = 0;
  bool waiting_for_clear_domain_reliability_monitor_ = false;
  bool waiting_for_clear_form_ = false;
  bool waiting_for_clear_history_ = false;
  bool waiting_for_clear_hostname_resolution_cache_ = false;
  bool waiting_for_clear_http_auth_cache_ = false;
  bool waiting_for_clear_keyword_data_ = false;
  bool waiting_for_clear_nacl_cache_ = false;
  bool waiting_for_clear_network_predictor_ = false;
  bool waiting_for_clear_networking_history_ = false;
  bool waiting_for_clear_passwords_ = false;
  bool waiting_for_clear_passwords_stats_ = false;
  bool waiting_for_clear_platform_keys_ = false;
  bool waiting_for_clear_pnacl_cache_ = false;
#if BUILDFLAG(ANDROID_JAVA_UI)
  bool waiting_for_clear_precache_history_ = false;
  bool waiting_for_clear_offline_page_data_ = false;
#endif
  bool waiting_for_clear_storage_partition_data_ = false;
#if BUILDFLAG(ENABLE_WEBRTC)
  bool waiting_for_clear_webrtc_logs_ = false;
#endif
  bool waiting_for_clear_auto_sign_in_ = false;

  // Observers of the global state and individual tasks.
  base::ObserverList<Observer, true> observer_list_;

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_sub_;

  // We do not own this.
  content::StoragePartition* storage_partition_for_testing_ = nullptr;

#if BUILDFLAG(ANDROID_JAVA_UI)
  // WebappRegistry makes calls across the JNI. In unit tests, the Java side is
  // not initialised, so the registry must be mocked out.
  std::unique_ptr<WebappRegistry> webapp_registry_;
#endif

  base::WeakPtrFactory<BrowsingDataRemover> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowsingDataRemover);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_BROWSING_DATA_REMOVER_H_
