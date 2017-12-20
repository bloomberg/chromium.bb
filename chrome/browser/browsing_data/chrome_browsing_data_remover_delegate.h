// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_
#define CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event_watcher.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/build_config.h"
#include "chrome/common/features.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/nacl/common/features.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/browsing_data_remover_delegate.h"
#include "extensions/features/features.h"
#include "media/media_features.h"
#include "ppapi/features/features.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "chrome/browser/pepper_flash_settings_manager.h"
#endif

class BrowsingDataFlashLSOHelper;
class Profile;
class WebappRegistry;

namespace content {
class BrowserContext;
class PluginDataRemover;
}

// A delegate used by BrowsingDataRemover to delete data specific to Chrome
// as the embedder.
class ChromeBrowsingDataRemoverDelegate
    : public content::BrowsingDataRemoverDelegate,
      public KeyedService
#if BUILDFLAG(ENABLE_PLUGINS)
      ,
      public PepperFlashSettingsManager::Client
#endif
{
 public:
  // This is an extension of content::BrowsingDataRemover::RemoveDataMask which
  // includes all datatypes therefrom and adds additional Chrome-specific ones.
  // TODO(crbug.com/668114): Extend this to uint64_t to ensure that we won't
  // run out of space anytime soon.
  enum DataType {
    // Embedder can start adding datatypes after the last platform datatype.
    DATA_TYPE_EMBEDDER_BEGIN =
        content::BrowsingDataRemover::DATA_TYPE_CONTENT_END << 1,

    // Chrome-specific datatypes.
    DATA_TYPE_HISTORY = DATA_TYPE_EMBEDDER_BEGIN,
    DATA_TYPE_FORM_DATA = DATA_TYPE_EMBEDDER_BEGIN << 1,
    DATA_TYPE_PASSWORDS = DATA_TYPE_EMBEDDER_BEGIN << 2,
    DATA_TYPE_PLUGIN_DATA = DATA_TYPE_EMBEDDER_BEGIN << 3,
#if defined(OS_ANDROID)
    DATA_TYPE_WEB_APP_DATA = DATA_TYPE_EMBEDDER_BEGIN << 4,
#endif
    DATA_TYPE_SITE_USAGE_DATA = DATA_TYPE_EMBEDDER_BEGIN << 5,
    DATA_TYPE_DURABLE_PERMISSION = DATA_TYPE_EMBEDDER_BEGIN << 6,
    DATA_TYPE_EXTERNAL_PROTOCOL_DATA = DATA_TYPE_EMBEDDER_BEGIN << 7,
    DATA_TYPE_HOSTED_APP_DATA_TEST_ONLY = DATA_TYPE_EMBEDDER_BEGIN << 8,
    DATA_TYPE_CONTENT_SETTINGS = DATA_TYPE_EMBEDDER_BEGIN << 9,

    // Group datatypes.

    // "Site data" includes storage backend accessible to websites and some
    // additional metadata kept by the browser (e.g. site usage data).
    DATA_TYPE_SITE_DATA = content::BrowsingDataRemover::DATA_TYPE_COOKIES |
                          content::BrowsingDataRemover::DATA_TYPE_CHANNEL_IDS |
                          content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE |
                          DATA_TYPE_PLUGIN_DATA |
#if defined(OS_ANDROID)
                          DATA_TYPE_WEB_APP_DATA |
#endif
                          DATA_TYPE_SITE_USAGE_DATA |
                          DATA_TYPE_DURABLE_PERMISSION |
                          DATA_TYPE_EXTERNAL_PROTOCOL_DATA,

    // Datatypes protected by Important Sites.
    IMPORTANT_SITES_DATA_TYPES =
        DATA_TYPE_SITE_DATA | content::BrowsingDataRemover::DATA_TYPE_CACHE,

    // Datatypes that can be deleted partially per URL / origin / domain,
    // whichever makes sense.
    FILTERABLE_DATA_TYPES = DATA_TYPE_SITE_DATA |
                            content::BrowsingDataRemover::DATA_TYPE_CACHE |
                            content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS,

    // Includes all the available remove options. Meant to be used by clients
    // that wish to wipe as much data as possible from a Profile, to make it
    // look like a new Profile.
    ALL_DATA_TYPES = DATA_TYPE_SITE_DATA |  //
                     content::BrowsingDataRemover::DATA_TYPE_CACHE |
                     content::BrowsingDataRemover::DATA_TYPE_DOWNLOADS |
                     DATA_TYPE_FORM_DATA |  //
                     DATA_TYPE_HISTORY |    //
                     DATA_TYPE_PASSWORDS |
                     content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES |
                     DATA_TYPE_CONTENT_SETTINGS,

    // Includes all available remove options. Meant to be used when the Profile
    // is scheduled to be deleted, and all possible data should be wiped from
    // disk as soon as possible.
    WIPE_PROFILE =
        ALL_DATA_TYPES | content::BrowsingDataRemover::DATA_TYPE_NO_CHECKS,
  };

  // This is an extension of content::BrowsingDataRemover::OriginType which
  // includes all origin types therefrom and adds additional Chrome-specific
  // ones.
  enum OriginType {
    // Embedder can start adding origin types after the last
    // platform origin type.
    ORIGIN_TYPE_EMBEDDER_BEGIN =
        content::BrowsingDataRemover::ORIGIN_TYPE_CONTENT_END << 1,

#if BUILDFLAG(ENABLE_EXTENSIONS)
    // Packaged apps and extensions (chrome-extension://*).
    ORIGIN_TYPE_EXTENSION = ORIGIN_TYPE_EMBEDDER_BEGIN,
#endif

    // All origin types.
    ALL_ORIGIN_TYPES =
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
#if BUILDFLAG(ENABLE_EXTENSIONS)
        ORIGIN_TYPE_EXTENSION |
#endif
        content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB,
  };

  // Important sites protect a small set of sites from the deletion of certain
  // datatypes. Therefore, those datatypes must be filterable by
  // url/origin/domain.
  static_assert((IMPORTANT_SITES_DATA_TYPES & ~FILTERABLE_DATA_TYPES) == 0,
                "All important sites datatypes must be filterable.");

  // Used to track the deletion of a single data storage backend.
  class SubTask {
   public:
    // Creates a SubTask that calls |forward_callback| when completed.
    // |forward_callback| is only kept as a reference and must outlive SubTask.
    explicit SubTask(const base::Closure& forward_callback);
    ~SubTask();

    // Indicate that the task is in progress and we're waiting.
    void Start();

    // Returns a callback that should be called to indicate that the task
    // has been finished.
    base::Closure GetCompletionCallback();

    // Whether the task is still in progress.
    bool is_pending() const { return is_pending_; }

   private:
    void CompletionCallback();

    bool is_pending_;
    const base::Closure& forward_callback_;
    base::WeakPtrFactory<SubTask> weak_ptr_factory_;
  };

  ChromeBrowsingDataRemoverDelegate(content::BrowserContext* browser_context);
  ~ChromeBrowsingDataRemoverDelegate() override;

  // KeyedService:
  void Shutdown() override;

  // BrowsingDataRemoverDelegate:
  content::BrowsingDataRemoverDelegate::EmbedderOriginTypeMatcher
  GetOriginTypeMatcher() const override;
  bool MayRemoveDownloadHistory() const override;
  void RemoveEmbedderData(
      const base::Time& delete_begin,
      const base::Time& delete_end,
      int remove_mask,
      const content::BrowsingDataFilterBuilder& filter_builder,
      int origin_type_mask,
      base::OnceClosure callback) override;

#if defined(OS_ANDROID)
  void OverrideWebappRegistryForTesting(
      std::unique_ptr<WebappRegistry> webapp_registry);
#endif

#if BUILDFLAG(ENABLE_PLUGINS)
  // Used for testing.
  void OverrideFlashLSOHelperForTesting(
      scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper);
#endif

 private:
  // If AllDone(), calls the callback provided in RemoveEmbedderData().
  void NotifyIfDone();

  // Whether there are no running deletion tasks.
  bool AllDone();

  // Callback for when TemplateURLService has finished loading. Clears the data,
  // clears the respective waiting flag, and invokes NotifyIfDone.
  void OnKeywordsLoaded(base::Callback<bool(const GURL&)> url_filter);

#if defined (OS_CHROMEOS)
  void OnClearPlatformKeys(base::Optional<bool> result);
#endif

  // Callback for when cookies have been deleted. Invokes NotifyIfDone.
  void OnClearedCookies();

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

  // The profile for which the data will be deleted.
  Profile* profile_;

  // Start time to delete from.
  base::Time delete_begin_;

  // End time to delete to.
  base::Time delete_end_;

  // Completion callback to call when all data are deleted.
  base::OnceClosure callback_;

  // A callback to NotifyIfDone() used by SubTasks instances.
  const base::Closure sub_task_forward_callback_;

  // Keeping track of various subtasks to be completed.
  // Non-zero if waiting for SafeBrowsing cookies to be cleared.
  int clear_cookies_count_ = 0;
  SubTask synchronous_clear_operations_;
  SubTask clear_autofill_origin_urls_;
  SubTask clear_flash_content_licenses_;
  SubTask clear_media_drm_licenses_;
  SubTask clear_domain_reliability_monitor_;
  SubTask clear_form_;
  SubTask clear_history_;
  SubTask clear_keyword_data_;
#if BUILDFLAG(ENABLE_NACL)
  SubTask clear_nacl_cache_;
  SubTask clear_pnacl_cache_;
#endif
  SubTask clear_hostname_resolution_cache_;
  SubTask clear_network_predictor_;
  SubTask clear_passwords_;
  SubTask clear_passwords_stats_;
  SubTask clear_http_auth_cache_;
  SubTask clear_platform_keys_;
#if defined(OS_ANDROID)
  SubTask clear_precache_history_;
  SubTask clear_offline_page_data_;
#endif
#if BUILDFLAG(ENABLE_WEBRTC)
  SubTask clear_webrtc_logs_;
#endif
  SubTask clear_auto_sign_in_;
  SubTask clear_reporting_cache_;
  SubTask clear_video_perf_history_;
  // Counts the number of plugin data tasks. Should be the number of LSO cookies
  // to be deleted, or 1 while we're fetching LSO cookies or deleting in bulk.
  int clear_plugin_data_count_ = 0;

#if BUILDFLAG(ENABLE_PLUGINS)
  // Used to delete plugin data.
  std::unique_ptr<content::PluginDataRemover> plugin_data_remover_;
  base::WaitableEventWatcher watcher_;

  // Used for per-site plugin data deletion.
  scoped_refptr<BrowsingDataFlashLSOHelper> flash_lso_helper_;

  uint32_t deauthorize_flash_content_licenses_request_id_ = 0;

  // Used to deauthorize content licenses for Pepper Flash.
  std::unique_ptr<PepperFlashSettingsManager> pepper_flash_settings_manager_;
#endif

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  std::unique_ptr<TemplateURLService::Subscription> template_url_sub_;

#if defined(OS_ANDROID)
  // WebappRegistry makes calls across the JNI. In unit tests, the Java side is
  // not initialised, so the registry must be mocked out.
  std::unique_ptr<WebappRegistry> webapp_registry_;
#endif

  base::WeakPtrFactory<ChromeBrowsingDataRemoverDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowsingDataRemoverDelegate);
};

#endif  // CHROME_BROWSER_BROWSING_DATA_CHROME_BROWSING_DATA_REMOVER_DELEGATE_H_
