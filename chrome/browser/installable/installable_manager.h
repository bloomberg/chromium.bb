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

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/installable/installable_logging.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/manifest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

// This struct specifies the work to be done by the InstallableManager.
// Data is cached and fetched in the order specified in this struct. A web app
// manifest will always be fetched first.
struct InstallableParams {
  // The ideal primary icon size to fetch. Used only if
  // |fetch_valid_primary_icon| is true.
  int ideal_primary_icon_size_in_px = -1;

  // The minimum primary icon size to fetch. Used only if
  // |fetch_valid_primary_icon| is true.
  int minimum_primary_icon_size_in_px = -1;

  // The ideal badge icon size to fetch. Used only if
  // |fetch_valid_badge_icon| is true.
  int ideal_badge_icon_size_in_px = -1;

  // The minimum badge icon size to fetch. Used only if
  // |fetch_valid_badge_icon| is true.
  int minimum_badge_icon_size_in_px = -1;

  // Check whether there is a fetchable, non-empty icon in the manifest
  // conforming to the primary icon size parameters.
  bool fetch_valid_primary_icon = false;

  // Check whether there is a fetchable, non-empty icon in the manifest
  // conforming to the badge icon size parameters.
  bool fetch_valid_badge_icon = false;

  // Check whether the site is installable. That is, it has a manifest valid for
  // a web app and a service worker controlling the manifest start URL and the
  // current URL.
  bool check_installable = false;

  // Whether or not to wait indefinitely for a service worker. If this is set to
  // false, the worker status will not be cached and will be re-checked if
  // GetData() is called again for the current page.
  bool wait_for_worker = true;
};

// This struct is passed to an InstallableCallback when the InstallableManager
// has finished working. Each reference is owned by InstallableManager, and
// callers should copy any objects which they wish to use later. Non-requested
// fields will be set to null, empty, or false.
struct InstallableData {
  // NO_ERROR_DETECTED if there were no issues.
  const InstallableStatusCode error_code;

  // Empty if the site has no <link rel="manifest"> tag.
  const GURL& manifest_url;

  // Empty if the site has an unparseable manifest.
  const content::Manifest& manifest;

  // Empty if no primary_icon was requested.
  const GURL& primary_icon_url;

  // nullptr if the most appropriate primary icon couldn't be determined or
  // downloaded. The underlying primary icon is owned by the InstallableManager;
  // clients must copy the bitmap if they want to to use it. If
  // fetch_valid_primary_icon was true and a primary icon could not be
  // retrieved, the reason will be in error_code.
  const SkBitmap* primary_icon;

  // Empty if no badge_icon was requested.
  const GURL& badge_icon_url;

  // nullptr if the most appropriate badge icon couldn't be determined or
  // downloaded. The underlying badge icon is owned by the InstallableManager;
  // clients must copy the bitmap if they want to to use it. Since the badge
  // icon is optional, no error code is set if it cannot be fetched, and clients
  // specifying fetch_valid_badge_icon must check that the bitmap exists before
  // using it.
  const SkBitmap* badge_icon;

  // true if the site has a service worker with a fetch handler and a viable web
  // app manifest. If check_installable was true and the site isn't installable,
  // the reason will be in error_code.
  const bool is_installable;
};

using InstallableCallback = base::Callback<void(const InstallableData&)>;

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
  void GetData(const InstallableParams& params,
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
  friend class InstallableManagerBrowserTest;
  friend class InstallableManagerUnitTest;
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           ManagerBeginsInEmptyState);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest, CheckWebapp);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerPassesWhenWaiting);
  FRIEND_TEST_ALL_PREFIXES(InstallableManagerBrowserTest,
                           CheckLazyServiceWorkerNoFetchHandlerFails);

  using Task = std::pair<InstallableParams, InstallableCallback>;
  using IconParams = std::tuple<int, int, content::Manifest::Icon::IconPurpose>;

  struct ManifestProperty;
  struct ValidManifestProperty;
  struct ServiceWorkerProperty;
  struct IconProperty;

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
  bool worker_waiting() const;
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
  void RunCallback(const Task& task, InstallableStatusCode error);
  void StartNextTask();
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

  // The list of <params, callback> pairs that have come from a call to GetData.
  std::vector<Task> tasks_;

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

  bool is_active_;
  bool is_pwa_check_complete_;

  base::WeakPtrFactory<InstallableManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InstallableManager);
};

#endif  // CHROME_BROWSER_INSTALLABLE_INSTALLABLE_MANAGER_H_
