// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/installable/installable_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"
#include "url/origin.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/shortcut_helper.h"
#endif

namespace {

// This constant is the icon size on Android (48dp) multiplied by the scale
// factor of a Nexus 5 device (3x). It is the currently advertised minimum icon
// size for triggering banners.
const int kMinimumPrimaryIconSizeInPx = 144;

// This constant is the smallest possible adaptive launcher icon size for any
// device density.
// The ideal icon size is 83dp (see documentation for
// R.dimen.webapk_adaptive_icon_size for discussion of maskable icon size). For
// a manifest to be valid, we do NOT need an maskable icon to be 83dp for the
// device's screen density. Instead, we only need the maskable icon be larger
// than (or equal to) 83dp in the smallest screen density (that is the mdpi
// screen density). For mdpi devices, 1dp is 1px. Therefore, we have 83px here.
// Requiring the minimum icon size (in pixel) independent of the device's screen
// density is because we use mipmap-anydpi-v26 to specify adaptive launcher
// icon, and it will make the icon adaptive as long as there is one usable
// maskable icon (if that icon is of wrong size, it'll be automatically
// resized).
const int kMinimumPrimaryAdaptiveLauncherIconSizeInPx = 83;

int GetIdealPrimaryIconSizeInPx() {
#if defined(OS_ANDROID)
  return ShortcutHelper::GetIdealHomescreenIconSizeInPx();
#else
  return kMinimumPrimaryIconSizeInPx;
#endif
}

int GetMinimumPrimaryIconSizeInPx() {
#if defined(OS_ANDROID)
  return ShortcutHelper::GetMinimumHomescreenIconSizeInPx();
#else
  return kMinimumPrimaryIconSizeInPx;
#endif
}

int GetIdealPrimaryAdaptiveLauncherIconSizeInPx() {
#if defined(OS_ANDROID)
  return ShortcutHelper::GetIdealAdaptiveLauncherIconSizeInPx();
#else
  return kMinimumPrimaryAdaptiveLauncherIconSizeInPx;
#endif
}

int GetIdealSplashIconSizeInPx() {
#if defined(OS_ANDROID)
  return ShortcutHelper::GetIdealSplashImageSizeInPx();
#else
  return kMinimumPrimaryIconSizeInPx;
#endif
}

int GetMinimumSplashIconSizeInPx() {
#if defined(OS_ANDROID)
  return ShortcutHelper::GetMinimumSplashImageSizeInPx();
#else
  return kMinimumPrimaryIconSizeInPx;
#endif
}

using IconPurpose = blink::Manifest::ImageResource::Purpose;

struct ImageTypeDetails {
  const char* extension;
  const char* mimetype;
};

constexpr ImageTypeDetails kSupportedImageTypes[] = {
    {".png", "image/png"},
// TODO(https://crbug.com/578122): Add SVG support for Android.
// TODO(https://crbug.com/466958): Add WebP support for Android.
#if !defined(OS_ANDROID)
    {".svg", "image/svg+xml"},
    {".webp", "image/webp"},
#endif
};

bool IsIconTypeSupported(const blink::Manifest::ImageResource& icon) {
  // The type field is optional. If it isn't present, fall back on checking
  // the src extension.
  if (icon.type.empty()) {
    std::string filename = icon.src.ExtractFileName();
    for (const ImageTypeDetails& details : kSupportedImageTypes) {
      if (base::EndsWith(filename, details.extension,
                         base::CompareCase::INSENSITIVE_ASCII)) {
        return true;
      }
    }
    return false;
  }

  for (const ImageTypeDetails& details : kSupportedImageTypes) {
    if (base::EqualsASCII(icon.type, details.mimetype))
      return true;
  }
  return false;
}

// Returns true if |manifest| specifies an SVG or PNG icon that either
// 1. has IconPurpose::ANY, with height and width >= kMinimumPrimaryIconSizeInPx
// (or size "any")
// 2. if maskable icon is preferred, has IconPurpose::MASKABLE with height and
// width >= kMinimumPrimaryAdaptiveLauncherIconSizeInPx (or size "any")

bool DoesManifestContainRequiredIcon(const blink::Manifest& manifest,
                                     bool prefer_maskable_icon) {
  for (const auto& icon : manifest.icons) {
    if (!IsIconTypeSupported(icon))
      continue;

    if (!(base::Contains(icon.purpose,
                         blink::Manifest::ImageResource::Purpose::ANY) ||
          (prefer_maskable_icon &&
           base::Contains(
               icon.purpose,
               blink::Manifest::ImageResource::Purpose::MASKABLE)))) {
      continue;
    }

    for (const auto& size : icon.sizes) {
      if (size.IsEmpty())  // "any"
        return true;
      if (base::Contains(icon.purpose,
                         blink::Manifest::ImageResource::Purpose::ANY) &&
          size.width() >= kMinimumPrimaryIconSizeInPx &&
          size.height() >= kMinimumPrimaryIconSizeInPx) {
        return true;
      }
      if (prefer_maskable_icon &&
          base::Contains(icon.purpose,
                         blink::Manifest::ImageResource::Purpose::MASKABLE) &&
          size.height() >= kMinimumPrimaryAdaptiveLauncherIconSizeInPx &&
          size.width() >= kMinimumPrimaryAdaptiveLauncherIconSizeInPx) {
        return true;
      }
    }
  }

  return false;
}

// Returns true if |params| specifies a full PWA check.
bool IsParamsForPwaCheck(const InstallableParams& params) {
  return params.valid_manifest && params.has_worker &&
         params.valid_primary_icon;
}

void OnDidCompleteGetAllErrors(
    base::OnceCallback<void(std::vector<content::InstallabilityError>
                                installability_errors)> callback,
    const InstallableData& data) {
  std::vector<content::InstallabilityError> installability_errors;
  for (auto error : data.errors) {
    content::InstallabilityError installability_error =
        GetInstallabilityError(error);
    if (!installability_error.error_id.empty())
      installability_errors.push_back(installability_error);
  }

  std::move(callback).Run(std::move(installability_errors));
}

void OnDidCompleteGetPrimaryIcon(
    base::OnceCallback<void(const SkBitmap*)> callback,
    const InstallableData& data) {
  std::move(callback).Run(data.primary_icon);
}

}  // namespace

InstallableManager::EligiblityProperty::EligiblityProperty() = default;

InstallableManager::EligiblityProperty::~EligiblityProperty() = default;

InstallableManager::ValidManifestProperty::ValidManifestProperty() = default;

InstallableManager::ValidManifestProperty::~ValidManifestProperty() = default;

InstallableManager::IconProperty::IconProperty()
    : error(NO_ERROR_DETECTED),
      purpose(IconPurpose::ANY),
      icon(),
      fetched(false) {}

InstallableManager::IconProperty::IconProperty(IconProperty&& other) = default;

InstallableManager::IconProperty::~IconProperty() {}

InstallableManager::IconProperty& InstallableManager::IconProperty::operator=(
    InstallableManager::IconProperty&& other) = default;

InstallableManager::InstallableManager(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      eligibility_(std::make_unique<EligiblityProperty>()),
      manifest_(std::make_unique<ManifestProperty>()),
      valid_manifest_(std::make_unique<ValidManifestProperty>()),
      worker_(std::make_unique<ServiceWorkerProperty>()),
      service_worker_context_(nullptr),
      has_pwa_check_(false) {
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
  if (service_worker_context_)
    service_worker_context_->RemoveObserver(this);
}

// static
int InstallableManager::GetMinimumIconSizeInPx() {
  return kMinimumPrimaryIconSizeInPx;
}

// static
bool InstallableManager::IsContentSecure(content::WebContents* web_contents) {
  if (!web_contents)
    return false;

  // chrome:// URLs are considered secure.
  const GURL& url = web_contents->GetLastCommittedURL();
  if (url.scheme() == content::kChromeUIScheme)
    return true;

  // chrome-untrusted:// URLs are shipped with Chrome, so they are considered
  // secure in this context.
  if (url.scheme() == content::kChromeUIUntrustedScheme)
    return true;

  if (IsOriginConsideredSecure(url))
    return true;

  return security_state::IsSslCertificateValid(
      SecurityStateTabHelper::FromWebContents(web_contents)
          ->GetSecurityLevel());
}

// static
bool InstallableManager::IsOriginConsideredSecure(const GURL& url) {
  return net::IsLocalhost(url) ||
         network::SecureOriginAllowlist::GetInstance().IsOriginAllowlisted(
             url::Origin::Create(url));
}

void InstallableManager::GetData(const InstallableParams& params,
                                 InstallableCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (IsParamsForPwaCheck(params))
    has_pwa_check_ = true;

  // Return immediately if we're already working on a task. The new task will be
  // looked at once the current task is finished.
  bool was_active = task_queue_.HasCurrent();
  task_queue_.Add({params, std::move(callback)});
  if (was_active)
    return;

  WorkOnTask();
}

void InstallableManager::GetAllErrors(
    base::OnceCallback<void(std::vector<content::InstallabilityError>
                                installability_errors)> callback) {
  InstallableParams params;
  params.check_eligibility = true;
  params.valid_manifest = true;
  params.check_webapp_manifest_display = true;
  params.has_worker = true;
  params.valid_primary_icon = true;
  params.wait_for_worker = false;
  params.is_debug_mode = true;
  GetData(params,
          base::BindOnce(OnDidCompleteGetAllErrors, std::move(callback)));
}

void InstallableManager::GetPrimaryIcon(
    base::OnceCallback<void(const SkBitmap*)> callback) {
  InstallableParams params;
  params.valid_primary_icon = true;
  GetData(params,
          base::BindOnce(OnDidCompleteGetPrimaryIcon, std::move(callback)));
}

bool InstallableManager::IsIconFetchComplete(const IconUsage usage) const {
  const auto it = icons_.find(usage);
  if (it == icons_.end() || !it->second.fetched)
    return false;

  // If we fetched maskable icon, but fetching was not success, do not consider
  // it's completed since we want to fallback to fetch ANY icon.
  if (it->second.purpose == IconPurpose::MASKABLE &&
      it->second.error != NO_ERROR_DETECTED) {
    return false;
  }

  return true;
}

bool InstallableManager::IsMaskableIconFetched(const IconUsage usage) const {
  const auto it = icons_.find(usage);
  if (it == icons_.end() || !it->second.fetched)
    return false;
  // if we fetched MASKABLE icon, or fetched ANY icon for fallback, consider
  // maskable icon is fetched.
  return it->second.purpose == IconPurpose::MASKABLE ||
         it->second.purpose == IconPurpose::ANY;
}

void InstallableManager::SetIconFetched(const IconUsage usage) {
  icons_[usage].fetched = true;
}

std::vector<InstallableStatusCode> InstallableManager::GetErrors(
    const InstallableParams& params) {
  std::vector<InstallableStatusCode> errors;

  if (params.check_eligibility && !eligibility_->errors.empty()) {
    errors.insert(errors.end(), eligibility_->errors.begin(),
                  eligibility_->errors.end());
  }

  if (manifest_->error != NO_ERROR_DETECTED)
    errors.push_back(manifest_->error);

  if (params.valid_manifest && !valid_manifest_->errors.empty()) {
    errors.insert(errors.end(), valid_manifest_->errors.begin(),
                  valid_manifest_->errors.end());
  }

  if (params.has_worker && worker_->error != NO_ERROR_DETECTED)
    errors.push_back(worker_->error);

  if (params.valid_primary_icon) {
    IconProperty& icon = icons_[IconUsage::kPrimary];
    // If the icon is MASKABLE, ignore any error since we want to fallback to
    // fetch IconPurpose::ANY.
    if (icon.error != NO_ERROR_DETECTED &&
        icon.purpose != IconPurpose::MASKABLE)
      errors.push_back(icon.error);
  }

  if (params.valid_splash_icon) {
    IconProperty& icon = icons_[IconUsage::kSplash];

    // If the error is NO_ACCEPTABLE_ICON, there is no icon suitable as a splash
    // icon in the manifest. Ignore this case since we only want to fail the
    // check if there was a suitable splash icon specified and we couldn't fetch
    // it.
    if (icon.error != NO_ERROR_DETECTED && icon.error != NO_ACCEPTABLE_ICON)
      errors.push_back(icon.error);
  }

  return errors;
}

InstallableStatusCode InstallableManager::eligibility_error() const {
  return eligibility_->errors.empty() ? NO_ERROR_DETECTED
                                      : eligibility_->errors[0];
}

InstallableStatusCode InstallableManager::manifest_error() const {
  return manifest_->error;
}

InstallableStatusCode InstallableManager::valid_manifest_error() const {
  return valid_manifest_->errors.empty() ? NO_ERROR_DETECTED
                                         : valid_manifest_->errors[0];
}

void InstallableManager::set_valid_manifest_error(
    InstallableStatusCode error_code) {
  valid_manifest_->errors.clear();
  if (error_code != NO_ERROR_DETECTED)
    valid_manifest_->errors.push_back(error_code);
}

InstallableStatusCode InstallableManager::worker_error() const {
  return worker_->error;
}

InstallableStatusCode InstallableManager::icon_error(const IconUsage usage) {
  return icons_[usage].error;
}

GURL& InstallableManager::icon_url(const IconUsage usage) {
  return icons_[usage].url;
}

const SkBitmap* InstallableManager::icon(const IconUsage usage) {
  return icons_[usage].icon.get();
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
  return (!params.check_eligibility || eligibility_->fetched) &&
         manifest_->fetched &&
         (!params.valid_manifest || valid_manifest_->fetched) &&
         (!params.has_worker || worker_->fetched) &&
         (!params.valid_primary_icon ||
          IsIconFetchComplete(IconUsage::kPrimary)) &&
         (!params.valid_splash_icon || IsIconFetchComplete(IconUsage::kSplash));
}

void InstallableManager::Reset() {
  // Prevent any outstanding callbacks to or from this object from being called.
  weak_factory_.InvalidateWeakPtrs();
  icons_.clear();

  // If we have paused tasks, we are waiting for a service worker.
  task_queue_.Reset();
  has_pwa_check_ = false;

  eligibility_ = std::make_unique<EligiblityProperty>();
  manifest_ = std::make_unique<ManifestProperty>();
  valid_manifest_ = std::make_unique<ValidManifestProperty>();
  worker_ = std::make_unique<ServiceWorkerProperty>();

  OnResetData();
}

void InstallableManager::SetManifestDependentTasksComplete() {
  valid_manifest_->fetched = true;
  worker_->fetched = true;
  SetIconFetched(IconUsage::kPrimary);
  SetIconFetched(IconUsage::kSplash);
}

void InstallableManager::CleanupAndStartNextTask() {
  // Sites can always register a service worker after we finish checking, so
  // don't cache a missing service worker error to ensure we always check
  // again.
  if (worker_error() == NO_MATCHING_SERVICE_WORKER)
    worker_ = std::make_unique<ServiceWorkerProperty>();

  task_queue_.Next();
  WorkOnTask();
}

void InstallableManager::RunCallback(
    InstallableTask task,
    std::vector<InstallableStatusCode> errors) {
  const InstallableParams& params = task.params;
  IconProperty null_icon;
  IconProperty* primary_icon = &null_icon;
  bool has_maskable_primary_icon = false;
  IconProperty* splash_icon = &null_icon;

  if (params.valid_primary_icon && IsIconFetchComplete(IconUsage::kPrimary)) {
    primary_icon = &icons_[IconUsage::kPrimary];
    has_maskable_primary_icon =
        (primary_icon->purpose == IconPurpose::MASKABLE);
  }
  if (params.valid_splash_icon && IsIconFetchComplete(IconUsage::kSplash))
    splash_icon = &icons_[IconUsage::kSplash];

  InstallableData data = {
      std::move(errors),   manifest_url(),           &manifest(),
      primary_icon->url,   primary_icon->icon.get(), has_maskable_primary_icon,
      splash_icon->url,    splash_icon->icon.get(),  valid_manifest_->is_valid,
      worker_->has_worker,
  };

  std::move(task.callback).Run(data);
}

void InstallableManager::WorkOnTask() {
  if (!task_queue_.HasCurrent())
    return;

  const InstallableParams& params = task_queue_.Current().params;

  auto errors = GetErrors(params);
  bool check_passed = errors.empty();
  if ((!check_passed && !params.is_debug_mode) || IsComplete(params)) {
    // Yield the UI thread before processing the next task. If this object is
    // deleted in the meantime, the next task naturally won't run.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&InstallableManager::CleanupAndStartNextTask,
                                  weak_factory_.GetWeakPtr()));

    auto task = std::move(task_queue_.Current());
    RunCallback(std::move(task), std::move(errors));
    return;
  }

  if (params.check_eligibility && !eligibility_->fetched) {
    CheckEligiblity();
  } else if (!manifest_->fetched) {
    FetchManifest();
  } else if (params.valid_primary_icon && params.prefer_maskable_icon &&
             !IsMaskableIconFetched(IconUsage::kPrimary)) {
    CheckAndFetchBestIcon(GetIdealPrimaryAdaptiveLauncherIconSizeInPx(),
                          kMinimumPrimaryAdaptiveLauncherIconSizeInPx,
                          IconPurpose::MASKABLE, IconUsage::kPrimary);
  } else if (params.valid_primary_icon &&
             !IsIconFetchComplete(IconUsage::kPrimary)) {
    CheckAndFetchBestIcon(GetIdealPrimaryIconSizeInPx(),
                          GetMinimumPrimaryIconSizeInPx(), IconPurpose::ANY,
                          IconUsage::kPrimary);
  } else if (params.valid_manifest && !valid_manifest_->fetched) {
    CheckManifestValid(params.check_webapp_manifest_display,
                       params.prefer_maskable_icon);
  } else if (params.has_worker && !worker_->fetched) {
    CheckServiceWorker();
  } else if (params.valid_splash_icon &&
             !IsIconFetchComplete(IconUsage::kSplash)) {
    CheckAndFetchBestIcon(GetIdealSplashIconSizeInPx(),
                          GetMinimumSplashIconSizeInPx(), IconPurpose::ANY,
                          IconUsage::kSplash);
  } else {
    NOTREACHED();
  }
}

void InstallableManager::CheckEligiblity() {
  // Fail if this is an incognito window, non-main frame, or insecure context.
  content::WebContents* web_contents = GetWebContents();
  if (Profile::FromBrowserContext(web_contents->GetBrowserContext())
          ->IsOffTheRecord()) {
    eligibility_->errors.push_back(IN_INCOGNITO);
  }
  if (web_contents->GetMainFrame()->GetParent()) {
    eligibility_->errors.push_back(NOT_IN_MAIN_FRAME);
  }
  if (!IsContentSecure(web_contents)) {
    eligibility_->errors.push_back(NOT_FROM_SECURE_ORIGIN);
  }

  eligibility_->fetched = true;
  WorkOnTask();
}

void InstallableManager::FetchManifest() {
  DCHECK(!manifest_->fetched);

  content::WebContents* web_contents = GetWebContents();
  DCHECK(web_contents);

  web_contents->GetManifest(base::BindOnce(
      &InstallableManager::OnDidGetManifest, weak_factory_.GetWeakPtr()));
}

void InstallableManager::OnDidGetManifest(const GURL& manifest_url,
                                          const blink::Manifest& manifest) {
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

void InstallableManager::CheckManifestValid(bool check_webapp_manifest_display,
                                            bool prefer_maskable_icon) {
  DCHECK(!valid_manifest_->fetched);
  DCHECK(!manifest().IsEmpty());

  valid_manifest_->is_valid = IsManifestValidForWebApp(
      manifest(), check_webapp_manifest_display, prefer_maskable_icon);
  valid_manifest_->fetched = true;
  WorkOnTask();
}

bool InstallableManager::IsManifestValidForWebApp(
    const blink::Manifest& manifest,
    bool check_webapp_manifest_display,
    bool prefer_maskable_icon) {
  bool is_valid = true;
  if (manifest.IsEmpty()) {
    valid_manifest_->errors.push_back(MANIFEST_EMPTY);
    return false;
  }

  if (!manifest.start_url.is_valid()) {
    valid_manifest_->errors.push_back(START_URL_NOT_VALID);
    is_valid = false;
  }

  if ((manifest.name.is_null() || manifest.name.string().empty()) &&
      (manifest.short_name.is_null() || manifest.short_name.string().empty())) {
    valid_manifest_->errors.push_back(MANIFEST_MISSING_NAME_OR_SHORT_NAME);
    is_valid = false;
  }

  if (check_webapp_manifest_display &&
      manifest.display != blink::mojom::DisplayMode::kStandalone &&
      manifest.display != blink::mojom::DisplayMode::kFullscreen &&
      manifest.display != blink::mojom::DisplayMode::kMinimalUi) {
    valid_manifest_->errors.push_back(MANIFEST_DISPLAY_NOT_SUPPORTED);
    is_valid = false;
  }

  if (!DoesManifestContainRequiredIcon(manifest, prefer_maskable_icon)) {
    valid_manifest_->errors.push_back(MANIFEST_MISSING_SUITABLE_ICON);
    is_valid = false;
  }

  return is_valid;
}

void InstallableManager::CheckServiceWorker() {
  DCHECK(!worker_->fetched);
  DCHECK(!manifest().IsEmpty());

  if (!service_worker_context_)
    return;

  // Check to see if there is a service worker for the manifest's scope.
  service_worker_context_->CheckHasServiceWorker(
      manifest().scope,
      base::BindOnce(&InstallableManager::OnDidCheckHasServiceWorker,
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
      InstallableTask& task = task_queue_.Current();
      if (task.params.wait_for_worker) {
        // Wait for ServiceWorkerContextObserver::OnRegistrationCompleted. Set
        // the param |wait_for_worker| to false so we only wait once per task.
        task.params.wait_for_worker = false;
        OnWaitingForServiceWorker();
        task_queue_.PauseCurrent();
        WorkOnTask();
        return;
      }
      worker_->has_worker = false;
      worker_->error = NO_MATCHING_SERVICE_WORKER;
      break;
  }

  worker_->fetched = true;
  WorkOnTask();
}

void InstallableManager::CheckAndFetchBestIcon(int ideal_icon_size_in_px,
                                               int minimum_icon_size_in_px,
                                               const IconPurpose purpose,
                                               const IconUsage usage) {
  DCHECK(!manifest().IsEmpty());

  IconProperty& icon = icons_[usage];
  icon.fetched = true;
  icon.purpose = purpose;
  icon.error = NO_ERROR_DETECTED;

  GURL icon_url = blink::ManifestIconSelector::FindBestMatchingSquareIcon(
      manifest().icons, ideal_icon_size_in_px, minimum_icon_size_in_px,
      purpose);

  if (icon_url.is_empty()) {
    icon.error = NO_ACCEPTABLE_ICON;
  } else {
    bool can_download_icon = content::ManifestIconDownloader::Download(
        GetWebContents(), icon_url, ideal_icon_size_in_px,
        minimum_icon_size_in_px,
        base::BindOnce(&InstallableManager::OnIconFetched,
                       weak_factory_.GetWeakPtr(), icon_url, usage));
    if (can_download_icon)
      return;
    icon.error = CANNOT_DOWNLOAD_ICON;
  }

  WorkOnTask();
}

void InstallableManager::OnIconFetched(const GURL icon_url,
                                       const IconUsage usage,
                                       const SkBitmap& bitmap) {
  if (!GetWebContents())
    return;

  IconProperty& icon = icons_[usage];
  if (bitmap.drawsNothing()) {
    icon.error = NO_ICON_AVAILABLE;
  } else {
    icon.url = icon_url;
    icon.icon.reset(new SkBitmap(bitmap));
  }

  WorkOnTask();
}

void InstallableManager::OnRegistrationCompleted(const GURL& pattern) {
  // If the scope doesn't match we keep waiting.
  if (!content::ServiceWorkerContext::ScopeMatches(pattern, manifest().scope))
    return;

  bool was_active = task_queue_.HasCurrent();

  // The existence of paused tasks implies that we are waiting for a service
  // worker. We move any paused tasks back into the main queue so that the
  // pipeline will call CheckHasServiceWorker again, in order to find out if
  // the SW has a fetch handler.
  // NOTE: If there are no paused tasks, that means:
  //   a) we've already failed the check, or
  //   b) we haven't yet called CheckHasServiceWorker.
  task_queue_.UnpauseAll();
  if (was_active)
    return;  // If the pipeline was already running, we don't restart it.

  WorkOnTask();
}

void InstallableManager::OnDestruct(content::ServiceWorkerContext* context) {
  service_worker_context_->RemoveObserver(this);
  service_worker_context_ = nullptr;
}

void InstallableManager::DidFinishNavigation(
    content::NavigationHandle* handle) {
  if (handle->IsInMainFrame() && handle->HasCommitted() &&
      !handle->IsSameDocument()) {
    Reset();
  }
}

void InstallableManager::DidUpdateWebManifestURL(
    const base::Optional<GURL>& manifest_url) {
  // A change in the manifest URL invalidates our entire internal state.
  Reset();
}

void InstallableManager::WebContentsDestroyed() {
  Reset();
  Observe(nullptr);
}

const GURL& InstallableManager::manifest_url() const {
  return manifest_->url;
}

const blink::Manifest& InstallableManager::manifest() const {
  return manifest_->manifest;
}

bool InstallableManager::valid_manifest() {
  return valid_manifest_->is_valid;
}

bool InstallableManager::has_worker() {
  return worker_->has_worker;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(InstallableManager)
