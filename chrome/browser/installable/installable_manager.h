// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_
#define CHROME_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_

#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/installable/installable_data.h"
#include "chrome/browser/installable/installable_logging.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/installable/installable_params.h"
#include "chrome/browser/installable/installable_task_queue.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"


// This class is responsible for fetching the resources required to check and
// install a site.
class InstallableManager
    : public content::ServiceWorkerContextObserver,
      public content::WebContentsObserver,
      public content::WebContentsUserData<InstallableManager> {
 public:
  explicit InstallableManager(content::WebContents* web_contents);
  ~InstallableManager() override;

  // Returns true if the overall security state of |web_contents| is sufficient
  // to be considered installable.
  static bool IsContentSecure(content::WebContents* web_contents);

  // Returns the minimum icon size in pixels for a site to be installable.
  // TODO(dominickn): consolidate this concept with minimum_icon_size_in_px
  // across all platforms.
  static int GetMinimumIconSizeInPx();

  // Get the installable data, fetching the resources specified in |params|.
  // |callback| is invoked synchronously (i.e. no via PostTask on the UI thread
  // when the data is ready; the synchronous execution ensures that the
  // references |callback| receives in its InstallableData argument are valid.
  //
  // Clients must be prepared for |callback| to not ever be invoked. For
  // instance, if installability checking is requested, this method will wait
  // until the site registers a service worker (and hence not invoke |callback|
  // at all if a service worker is never registered).
  //
  // Calls requesting data that is already fetched will return the cached data.
  virtual void GetData(const InstallableParams& params,
                       const InstallableCallback& callback);

  // Called via AppBannerManagerAndroid to record metrics on how often the
  // installable check is completed when the menu or add to homescreen menu item
  // is opened on Android.
  void RecordMenuOpenHistogram();
  void RecordMenuItemAddToHomescreenHistogram();
  void RecordQueuedMetricsOnTaskCompletion(const InstallableParams& params,
                                           bool check_passed);

 protected:
  // For mocking in tests.
  virtual void OnWaitingForServiceWorker() {}

 private:
  friend class AddToHomescreenDataFetcherTest;
  friend class InstallableManagerBrowserTest;
  friend class InstallableManagerUnitTest;
  friend class TestInstallableManager;
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           ManagerBeginsInEmptyState);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest, CheckWebapp);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerPassesWhenWaiting);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerNoFetchHandlerFails);

  using IconParams = std::tuple<int, int, content::Manifest::Icon::IconPurpose>;

  struct ManifestProperty {
    InstallableStatusCode error = NO_ERROR_DETECTED;
    GURL url;
    content::Manifest manifest;
    bool fetched = false;
  };

  struct ValidManifestProperty {
    InstallableStatusCode error = NO_ERROR_DETECTED;
    bool is_valid = false;
    bool fetched = false;
  };

  struct ServiceWorkerProperty {
    InstallableStatusCode error = NO_ERROR_DETECTED;
    bool has_worker = false;
    bool is_waiting = false;
    bool fetched = false;
  };

  struct IconProperty {
    IconProperty();
    IconProperty(IconProperty&& other);
    ~IconProperty();
    IconProperty& operator=(IconProperty&& other);

    InstallableStatusCode error;
    GURL url;
    std::unique_ptr<SkBitmap> icon;
    bool fetched;

   private:
    // This class contains a std::unique_ptr and therefore must be move-only.
    DISALLOW_COPY_AND_ASSIGN(IconProperty);
  };

  // Returns an IconParams object that queries for a primary icon conforming to
  // the primary icon size parameters in |params|.
  IconParams ParamsForPrimaryIcon(const InstallableParams& params) const;
  // Returns an IconParams object that queries for a badge icon conforming to
  // the badge icon size parameters in |params|.
  IconParams ParamsForBadgeIcon(const InstallableParams& params) const;

  // Returns true if |params| matches any fetched icon, or false if no icon has
  // been requested yet or there is no match.
  bool IsIconFetched(const IconParams& params) const;

  // Sets the icon matching |params| as fetched.
  void SetIconFetched(const IconParams& params);

  // Returns the error code associated with the resources requested in |params|,
  // or NO_ERROR_DETECTED if there is no error.
  InstallableStatusCode GetErrorCode(const InstallableParams& params);

  // Gets/sets parts of particular properties. Exposed for testing.
  InstallableStatusCode manifest_error() const;
  InstallableStatusCode valid_manifest_error() const;
  void set_valid_manifest_error(InstallableStatusCode error_code);
  InstallableStatusCode worker_error() const;
  InstallableStatusCode icon_error(const IconParams& icon_params);
  GURL& icon_url(const IconParams& icon_params);
  const SkBitmap* icon(const IconParams& icon);

  // Returns the WebContents to which this object is attached, or nullptr if the
  // WebContents doesn't exist or is currently being destroyed.
  content::WebContents* GetWebContents();

  // Returns true if |params| requires no more work to be done.
  bool IsComplete(const InstallableParams& params) const;

  // Resets members to empty and removes all queued tasks.
  // Called when navigating to a new page or if the WebContents is destroyed
  // whilst waiting for a callback.
  void Reset();

  // Sets the fetched bit on the installable and icon subtasks.
  // Called if no manifest (or an empty manifest) was fetched from the site.
  void SetManifestDependentTasksComplete();

  // Methods coordinating and dispatching work for the current task.
  void RunCallback(const InstallableTask& task, InstallableStatusCode error);
  void WorkOnTask();

  // Data retrieval methods.
  void FetchManifest();
  void OnDidGetManifest(const GURL& manifest_url,
                        const content::Manifest& manifest);

  void CheckInstallable();
  bool IsManifestValidForWebApp(const content::Manifest& manifest);
  void CheckServiceWorker();
  void OnDidCheckHasServiceWorker(content::ServiceWorkerCapability capability);

  void CheckAndFetchBestIcon(const IconParams& params);
  void OnIconFetched(const GURL icon_url,
                     const IconParams& params,
                     const SkBitmap& bitmap);

  // content::ServiceWorkerContextObserver overrides
  void OnRegistrationStored(const GURL& pattern) override;

  // content::WebContentsObserver overrides
  void DidFinishNavigation(content::NavigationHandle* handle) override;
  void WebContentsDestroyed() override;

  const GURL& manifest_url() const;
  const content::Manifest& manifest() const;
  bool is_installable() const;

  InstallableTaskQueue task_queue_;

  // Installable properties cached on this object.
  std::unique_ptr<ManifestProperty> manifest_;
  std::unique_ptr<ValidManifestProperty> valid_manifest_;
  std::unique_ptr<ServiceWorkerProperty> worker_;
  std::map<IconParams, IconProperty> icons_;

  // Owned by the storage partition attached to the content::WebContents which
  // this object is scoped to.
  content::ServiceWorkerContext* service_worker_context_;

  // Whether or not the current page is a PWA. This is reset per navigation and
  // is independent of the caching mechanism, i.e. if a PWA check is run
  // multiple times for one page, this will be set on the first check.
  InstallabilityCheckStatus page_status_;

  // Counts for the number of queued requests of the menu and add to homescreen
  // menu item there have been whilst the installable check is awaiting
  // completion. Used for metrics recording.
  int menu_open_count_;
  int menu_item_add_to_homescreen_count_;

  bool is_pwa_check_complete_;

  base::WeakPtrFactory<InstallableManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InstallableManager);
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_
