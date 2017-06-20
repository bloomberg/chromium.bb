// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_manager.h"

#include "base/bind.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/manifest_icon_selector.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/url_util.h"
#include "third_party/WebKit/public/platform/WebDisplayMode.h"

using IconPurpose = content::Manifest::Icon::IconPurpose;

namespace {

const char kPngExtension[] = ".png";

// This constant is the icon size on Android (48dp) multiplied by the scale
// factor of a Nexus 5 device (3x). For mobile and desktop platforms, a 144px
// icon is an approximate, appropriate lower bound. It is the currently
// advertised minimum icon size for triggering banners.
// TODO(dominickn): consolidate with minimum_icon_size_in_px across platforms.
const int kIconMinimumSizeInPx = 144;

// Returns true if |manifest| specifies a PNG icon >= 144x144px (or size "any"),
// and of icon purpose IconPurpose::ANY.
bool DoesManifestContainRequiredIcon(const content::Manifest& manifest) {
  for (const auto& icon : manifest.icons) {
    // The type field is optional. If it isn't present, fall back on checking
    // the src extension, and allow the icon if the extension ends with png.
    if (!base::EqualsASCII(icon.type, "image/png") &&
        !(icon.type.empty() && base::EndsWith(
            icon.src.ExtractFileName(), kPngExtension,
            base::CompareCase::INSENSITIVE_ASCII)))
      continue;

    if (!base::ContainsValue(icon.purpose, IconPurpose::ANY))
      continue;

    for (const auto& size : icon.sizes) {
      if (size.IsEmpty())  // "any"
        return true;
      if (size.width() >= kIconMinimumSizeInPx &&
          size.height() >= kIconMinimumSizeInPx) {
        return true;
      }
    }
  }

  return false;
}

// Returns true if |params| specifies a full PWA check.
bool IsParamsForPwaCheck(const InstallableParams& params) {
  return params.check_installable && params.fetch_valid_primary_icon;
}

}  // namespace

DEFINE_WEB_CONTENTS_USER_DATA_KEY(InstallableManager);

struct InstallableManager::ManifestProperty {
  InstallableStatusCode error = NO_ERROR_DETECTED;
  GURL url;
  content::Manifest manifest;
  bool fetched = false;
};

struct InstallableManager::ValidManifestProperty {
  InstallableStatusCode error = NO_ERROR_DETECTED;
  bool is_valid = false;
  bool fetched = false;
};

struct InstallableManager::ServiceWorkerProperty {
  InstallableStatusCode error = NO_ERROR_DETECTED;
  bool has_worker = false;
  bool is_waiting = false;
  bool fetched = false;
};

struct InstallableManager::IconProperty {
  IconProperty() :
    error(NO_ERROR_DETECTED), url(), icon(), fetched(false) { }
  IconProperty(IconProperty&& other) = default;
  IconProperty& operator=(IconProperty&& other) = default;

  InstallableStatusCode error = NO_ERROR_DETECTED;
  GURL url;
  std::unique_ptr<SkBitmap> icon;
  bool fetched;

 private:
  // This class contains a std::unique_ptr and therefore must be move-only.
  DISALLOW_COPY_AND_ASSIGN(IconProperty);
};

InstallableManager::InstallableManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      manifest_(base::MakeUnique<ManifestProperty>()),
      valid_manifest_(base::MakeUnique<ValidManifestProperty>()),
      worker_(base::MakeUnique<ServiceWorkerProperty>()),
      service_worker_context_(nullptr),
      page_status_(InstallabilityCheckStatus::NOT_STARTED),
      menu_open_count_(0),
      menu_item_add_to_homescreen_count_(0),
      is_active_(false),
      is_pwa_check_complete_(false),
      weak_factory_(this) {
  // This is null in unit tests.
  if (web_contents) {
    content::StoragePartition* storage_partition =
        content::BrowserContext::GetStoragePartition(
            Profile::FromBrowserContext(web_contents->GetBrowserContext()),
            web_contents->GetSiteInstance());
    DCHECK(storage_partition);

    service_worker_context_ = storage_partition->GetServiceWorkerContext();
    service_worker_context_->AddObserver(this);
  }
}

InstallableManager::~InstallableManager() {
  // Null in unit tests.
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

// static
bool InstallableManager::IsContentSecure(content::WebContents* web_contents) {
  if (!web_contents)
    return false;

  // Whitelist localhost. Check the VisibleURL to match what the
  // SecurityStateTabHelper looks at.
  if (net::IsLocalhost(web_contents->GetVisibleURL().HostNoBrackets()))
    return true;

  security_state::SecurityInfo security_info;
  SecurityStateTabHelper::FromWebContents(web_contents)
      ->GetSecurityInfo(&security_info);
  return security_info.security_level == security_state::SECURE ||
         security_info.security_level == security_state::EV_SECURE;
}

// static
int InstallableManager::GetMinimumIconSizeInPx() {
  return kIconMinimumSizeInPx;
}

void InstallableManager::GetData(const InstallableParams& params,
                                 const InstallableCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Return immediately if we're already working on a task. The new task will be
  // looked at once the current task is finished.
  tasks_.push_back({params, callback});
  if (is_active_)
    return;

  is_active_ = true;
  if (page_status_ == InstallabilityCheckStatus::NOT_STARTED)
    page_status_ = InstallabilityCheckStatus::NOT_COMPLETED;
  StartNextTask();
}

void InstallableManager::RecordMenuOpenHistogram() {
  if (is_pwa_check_complete_)
    InstallableMetrics::RecordMenuOpenHistogram(page_status_);
  else
    ++menu_open_count_;
}

void InstallableManager::RecordMenuItemAddToHomescreenHistogram() {
  if (is_pwa_check_complete_)
    InstallableMetrics::RecordMenuItemAddToHomescreenHistogram(page_status_);
  else
    ++menu_item_add_to_homescreen_count_;
}

void InstallableManager::RecordQueuedMetricsOnTaskCompletion(
    const InstallableParams& params,
    bool check_passed) {
  // Don't do anything if we've:
  //  - already finished the PWA check, or
  //  - we passed the check AND it was not for the full PWA params.
  // In the latter case (i.e. the check passed but we weren't checking
  // everything), we don't yet know if the site is installable. However, if the
  // check didn't pass, we know for sure the site isn't installable, regardless
  // of how much we checked.
  //
  // Once a full check is completed, metrics will be directly recorded in
  // Record*Histogram since |is_pwa_check_complete_| will be true.
  if (is_pwa_check_complete_ || (check_passed && !IsParamsForPwaCheck(params)))
    return;

  is_pwa_check_complete_ = true;
  page_status_ =
      check_passed
          ? InstallabilityCheckStatus::COMPLETE_PROGRESSIVE_WEB_APP
          : InstallabilityCheckStatus::COMPLETE_NON_PROGRESSIVE_WEB_APP;

  // Compute what the status would have been for any queued calls to
  // Record*Histogram, and record appropriately.
  InstallabilityCheckStatus prev_status =
      check_passed
          ? InstallabilityCheckStatus::IN_PROGRESS_PROGRESSIVE_WEB_APP
          : InstallabilityCheckStatus::IN_PROGRESS_NON_PROGRESSIVE_WEB_APP;
  for (; menu_open_count_ > 0; --menu_open_count_)
    InstallableMetrics::RecordMenuOpenHistogram(prev_status);

  for (; menu_item_add_to_homescreen_count_ > 0;
       --menu_item_add_to_homescreen_count_) {
    InstallableMetrics::RecordMenuItemAddToHomescreenHistogram(prev_status);
  }
}

InstallableManager::IconParams InstallableManager::ParamsForPrimaryIcon(
    const InstallableParams& params) const {
  return std::make_tuple(params.ideal_primary_icon_size_in_px,
                         params.minimum_primary_icon_size_in_px,
                         IconPurpose::ANY);
}

InstallableManager::IconParams InstallableManager::ParamsForBadgeIcon(
    const InstallableParams& params) const {
  return std::make_tuple(params.ideal_badge_icon_size_in_px,
                         params.minimum_badge_icon_size_in_px,
                         IconPurpose::BADGE);
}

bool InstallableManager::IsIconFetched(const IconParams& params) const {
  const auto it = icons_.find(params);
  return it != icons_.end() && it->second.fetched;
}

void InstallableManager::SetIconFetched(const IconParams& params) {
  icons_[params].fetched = true;
}

InstallableStatusCode InstallableManager::GetErrorCode(
    const InstallableParams& params) {
  if (manifest_->error != NO_ERROR_DETECTED)
    return manifest_->error;

  if (params.check_installable) {
    if (valid_manifest_->error != NO_ERROR_DETECTED)
      return valid_manifest_->error;
    if (worker_->error != NO_ERROR_DETECTED)
      return worker_->error;
  }

  if (params.fetch_valid_primary_icon) {
    IconProperty& icon = icons_[ParamsForPrimaryIcon(params)];
    if (icon.error != NO_ERROR_DETECTED)
      return icon.error;
  }

  if (params.fetch_valid_badge_icon) {
    IconProperty& icon = icons_[ParamsForBadgeIcon(params)];

    // If the error is NO_ACCEPTABLE_ICON, there is no icon suitable as a badge
    // in the manifest. Ignore this case since we only want to fail the check if
    // there was a suitable badge icon specified and we couldn't fetch it.
    if (icon.error != NO_ERROR_DETECTED && icon.error != NO_ACCEPTABLE_ICON)
      return icon.error;
  }

  return NO_ERROR_DETECTED;
}

InstallableStatusCode InstallableManager::manifest_error() const {
  return manifest_->error;
}

InstallableStatusCode InstallableManager::valid_manifest_error() const {
  return valid_manifest_->error;
}

void InstallableManager::set_valid_manifest_error(
    InstallableStatusCode error_code) {
  valid_manifest_->error = error_code;
}

InstallableStatusCode InstallableManager::worker_error() const {
  return worker_->error;
}

bool InstallableManager::worker_waiting() const {
  return worker_->is_waiting;
}

InstallableStatusCode InstallableManager::icon_error(
    const IconParams& icon_params) {
  return icons_[icon_params].error;
}

GURL& InstallableManager::icon_url(const IconParams& icon_params) {
  return icons_[icon_params].url;
}

const SkBitmap* InstallableManager::icon(const IconParams& icon_params) {
  return icons_[icon_params].icon.get();
}

content::WebContents* InstallableManager::GetWebContents() {
  content::WebContents* contents = web_contents();
  if (!contents || contents->IsBeingDestroyed())
    return nullptr;
  return contents;
}

bool InstallableManager::IsComplete(const InstallableParams& params) const {
  // Returns true if for all resources:
  //  a. the params did not request it, OR
  //  b. the resource has been fetched/checked.
  return manifest_->fetched &&
         (!params.check_installable ||
          (valid_manifest_->fetched && worker_->fetched)) &&
         (!params.fetch_valid_primary_icon ||
          IsIconFetched(ParamsForPrimaryIcon(params))) &&
         (!params.fetch_valid_badge_icon ||
          IsIconFetched(ParamsForBadgeIcon(params)));
}

void InstallableManager::Reset() {
  // Prevent any outstanding callbacks to or from this object from being called.
  weak_factory_.InvalidateWeakPtrs();
  tasks_.clear();
  icons_.clear();

  // We may have reset prior to completion, in which case |menu_open_count_| or
  // |menu_item_add_to_homescreen_count_| might be nonzero and |page_status_| is
  // one of NOT_STARTED or NOT_COMPLETED. If we completed, then these values
  // cannot be anything except 0.
  is_pwa_check_complete_ = false;

  for (; menu_open_count_ > 0; --menu_open_count_)
    InstallableMetrics::RecordMenuOpenHistogram(page_status_);

  for (; menu_item_add_to_homescreen_count_ > 0;
       --menu_item_add_to_homescreen_count_) {
    InstallableMetrics::RecordMenuItemAddToHomescreenHistogram(page_status_);
  }

  page_status_ = InstallabilityCheckStatus::NOT_STARTED;
  manifest_ = base::MakeUnique<ManifestProperty>();
  valid_manifest_ = base::MakeUnique<ValidManifestProperty>();
  worker_ = base::MakeUnique<ServiceWorkerProperty>();

  is_active_ = false;
}

void InstallableManager::SetManifestDependentTasksComplete() {
  DCHECK(!tasks_.empty());
  const InstallableParams& params = tasks_[0].first;

  valid_manifest_->fetched = true;
  worker_->fetched = true;
  SetIconFetched(ParamsForPrimaryIcon(params));
  SetIconFetched(ParamsForBadgeIcon(params));
}

void InstallableManager::RunCallback(const Task& task,
                                     InstallableStatusCode code) {
  const InstallableParams& params = task.first;
  IconProperty null_icon;
  IconProperty* primary_icon = &null_icon;
  IconProperty* badge_icon = &null_icon;

  if (params.fetch_valid_primary_icon &&
      base::ContainsKey(icons_, ParamsForPrimaryIcon(params)))
    primary_icon = &icons_[ParamsForPrimaryIcon(params)];
  if (params.fetch_valid_badge_icon &&
      base::ContainsKey(icons_, ParamsForBadgeIcon(params)))
    badge_icon = &icons_[ParamsForBadgeIcon(params)];

  InstallableData data = {
      code,
      manifest_url(),
      manifest(),
      primary_icon->url,
      primary_icon->icon.get(),
      badge_icon->url,
      badge_icon->icon.get(),
      params.check_installable ? is_installable() : false};

  task.second.Run(data);
}

void InstallableManager::StartNextTask() {
  // If there's nothing to do, exit. Resources remain cached so any future calls
  // won't re-fetch anything that has already been retrieved.
  if (tasks_.empty()) {
    is_active_ = false;
    return;
  }

  DCHECK(is_active_);
  WorkOnTask();
}

void InstallableManager::WorkOnTask() {
  DCHECK(!tasks_.empty());
  const Task& task = tasks_[0];
  const InstallableParams& params = task.first;

  InstallableStatusCode code = GetErrorCode(params);
  bool check_passed = (code == NO_ERROR_DETECTED);
  if (!check_passed || IsComplete(params)) {
    RecordQueuedMetricsOnTaskCompletion(params, check_passed);
    RunCallback(task, code);

    // Sites can always register a service worker after we finish checking, so
    // don't cache a missing service worker error to ensure we always check
    // again.
    if (worker_error() == NO_MATCHING_SERVICE_WORKER)
      worker_ = base::MakeUnique<ServiceWorkerProperty>();
    tasks_.erase(tasks_.begin());
    StartNextTask();
    return;
  }

  if (!manifest_->fetched) {
    FetchManifest();
  } else if (params.fetch_valid_primary_icon &&
             !IsIconFetched(ParamsForPrimaryIcon(params))) {
    CheckAndFetchBestIcon(ParamsForPrimaryIcon(params));
  } else if (params.check_installable && !valid_manifest_->fetched) {
    CheckInstallable();
  } else if (params.check_installable && !worker_->fetched) {
    CheckServiceWorker();
  } else if (params.fetch_valid_badge_icon &&
             !IsIconFetched(ParamsForBadgeIcon(params))) {
    CheckAndFetchBestIcon(ParamsForBadgeIcon(params));
  } else {
    NOTREACHED();
  }
}

void InstallableManager::FetchManifest() {
  DCHECK(!manifest_->fetched);

  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  web_contents->GetManifest(base::Bind(&InstallableManager::OnDidGetManifest,
                                       weak_factory_.GetWeakPtr()));
}

void InstallableManager::OnDidGetManifest(const GURL& manifest_url,
                                          const content::Manifest& manifest) {
  if (!GetWebContents())
    return;

  if (manifest_url.is_empty()) {
    manifest_->error = NO_MANIFEST;
    SetManifestDependentTasksComplete();
  } else if (manifest.IsEmpty()) {
    manifest_->error = MANIFEST_EMPTY;
    SetManifestDependentTasksComplete();
  }

  manifest_->url = manifest_url;
  manifest_->manifest = manifest;
  manifest_->fetched = true;
  WorkOnTask();
}

void InstallableManager::CheckInstallable() {
  DCHECK(!valid_manifest_->fetched);
  DCHECK(!manifest().IsEmpty());

  valid_manifest_->is_valid = IsManifestValidForWebApp(manifest());
  valid_manifest_->fetched = true;
  WorkOnTask();
}

bool InstallableManager::IsManifestValidForWebApp(
    const content::Manifest& manifest) {
  if (manifest.IsEmpty()) {
    valid_manifest_->error = MANIFEST_EMPTY;
    return false;
  }

  if (!manifest.start_url.is_valid()) {
    valid_manifest_->error = START_URL_NOT_VALID;
    return false;
  }

  if ((manifest.name.is_null() || manifest.name.string().empty()) &&
      (manifest.short_name.is_null() || manifest.short_name.string().empty())) {
    valid_manifest_->error = MANIFEST_MISSING_NAME_OR_SHORT_NAME;
    return false;
  }

  // TODO(dominickn,mlamouri): when Chrome supports "minimal-ui", it should be
  // accepted. If we accept it today, it would fallback to "browser" and make
  // this check moot. See https://crbug.com/604390.
  if (manifest.display != blink::kWebDisplayModeStandalone &&
      manifest.display != blink::kWebDisplayModeFullscreen) {
    valid_manifest_->error = MANIFEST_DISPLAY_NOT_SUPPORTED;
    return false;
  }

  if (!DoesManifestContainRequiredIcon(manifest)) {
    valid_manifest_->error = MANIFEST_MISSING_SUITABLE_ICON;
    return false;
  }

  return true;
}

void InstallableManager::CheckServiceWorker() {
  DCHECK(!worker_->fetched);
  DCHECK(!manifest().IsEmpty());
  DCHECK(manifest().start_url.is_valid());

  // Check to see if there is a single service worker controlling this page
  // and the manifest's start url.
  service_worker_context_->CheckHasServiceWorker(
      GetWebContents()->GetLastCommittedURL(), manifest().start_url,
      base::Bind(&InstallableManager::OnDidCheckHasServiceWorker,
                 weak_factory_.GetWeakPtr()));
}

void InstallableManager::OnDidCheckHasServiceWorker(
    content::ServiceWorkerCapability capability) {
  if (!GetWebContents())
    return;

  switch (capability) {
    case content::ServiceWorkerCapability::SERVICE_WORKER_WITH_FETCH_HANDLER:
      worker_->has_worker = true;
      break;
    case content::ServiceWorkerCapability::SERVICE_WORKER_NO_FETCH_HANDLER:
      worker_->has_worker = false;
      worker_->error = NOT_OFFLINE_CAPABLE;
      break;
    case content::ServiceWorkerCapability::NO_SERVICE_WORKER:
      InstallableParams& params = tasks_[0].first;
      if (params.wait_for_worker) {
        // Wait for ServiceWorkerContextObserver::OnRegistrationStored. Set the
        // param |wait_for_worker| to false so we only wait once per task.
        params.wait_for_worker = false;
        worker_->is_waiting = true;
        OnWaitingForServiceWorker();
        return;
      }
      worker_->has_worker = false;
      worker_->error = NO_MATCHING_SERVICE_WORKER;
      break;
  }

  worker_->fetched = true;
  WorkOnTask();
}

void InstallableManager::CheckAndFetchBestIcon(const IconParams& params) {
  DCHECK(!manifest().IsEmpty());

  int ideal_icon_size_in_px = std::get<0>(params);
  int minimum_icon_size_in_px = std::get<1>(params);
  IconPurpose icon_purpose = std::get<2>(params);

  IconProperty& icon = icons_[params];
  icon.fetched = true;

  GURL icon_url = content::ManifestIconSelector::FindBestMatchingIcon(
      manifest().icons, ideal_icon_size_in_px, minimum_icon_size_in_px,
      icon_purpose);

  if (icon_url.is_empty()) {
    icon.error = NO_ACCEPTABLE_ICON;
  } else {
    bool can_download_icon = content::ManifestIconDownloader::Download(
        GetWebContents(), icon_url, ideal_icon_size_in_px,
        minimum_icon_size_in_px,
        base::Bind(&InstallableManager::OnIconFetched,
                   weak_factory_.GetWeakPtr(), icon_url, params));
    if (can_download_icon)
      return;
    icon.error = CANNOT_DOWNLOAD_ICON;
  }

  WorkOnTask();
}

void InstallableManager::OnIconFetched(
    const GURL icon_url, const IconParams& params, const SkBitmap& bitmap) {
  IconProperty& icon = icons_[params];

  if (!GetWebContents())
    return;

  if (bitmap.drawsNothing()) {
    icon.error = NO_ICON_AVAILABLE;
  } else {
    icon.url = icon_url;
    icon.icon.reset(new SkBitmap(bitmap));
  }

  WorkOnTask();
}

void InstallableManager::OnRegistrationStored(const GURL& pattern) {
  // If we aren't currently waiting, either:
  //   a) we've already failed the check, or
  //   b) we haven't yet called CheckHasServiceWorker.
  // Otherwise if the scope doesn't match we keep waiting.
  if (!worker_->is_waiting || !content::ServiceWorkerContext::ScopeMatches(
                                  pattern, manifest().start_url)) {
    return;
  }

  // This will call CheckHasServiceWorker to check whether the service worker
  // controls the start_url and if it has a fetch handler.
  worker_->is_waiting = false;
  WorkOnTask();
}

void InstallableManager::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (handle->IsInMainFrame() && handle->HasCommitted() &&
      !handle->IsSameDocument()) {
    Reset();
  }
}

void InstallableManager::WebContentsDestroyed() {
  Reset();
  Observe(nullptr);
}

const GURL& InstallableManager::manifest_url() const {
  return manifest_->url;
}

const content::Manifest& InstallableManager::manifest() const {
  return manifest_->manifest;
}

bool InstallableManager::is_installable() const {
  return valid_manifest_->is_valid && worker_->has_worker;
}
