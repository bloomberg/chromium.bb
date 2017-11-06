// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

#include <utility>

#include "ash/new_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/audio/arc_audio_bridge.h"
#include "components/url_formatter/url_fixer.h"
#include "ui/base/layout.h"
#include "url/gurl.h"

namespace arc {
namespace {

constexpr char kChromeUIScheme[] = "chrome";

class OpenUrlDelegateImpl : public ArcIntentHelperBridge::OpenUrlDelegate {
 public:
  ~OpenUrlDelegateImpl() override = default;

  // ArcIntentHelperBridge::OpenUrlDelegate:
  void OpenUrl(const GURL& url) override {
    // TODO(mash): Support this functionality without ash::Shell access in
    // Chrome.
    if (ash::Shell::HasInstance())
      ash::Shell::Get()->shell_delegate()->OpenUrlFromArc(url);
  }
};

// Singleton factory for ArcIntentHelperBridge.
class ArcIntentHelperBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcIntentHelperBridge,
          ArcIntentHelperBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcIntentHelperBridgeFactory";

  static ArcIntentHelperBridgeFactory* GetInstance() {
    return base::Singleton<ArcIntentHelperBridgeFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ArcIntentHelperBridgeFactory>;

  ArcIntentHelperBridgeFactory() = default;
  ~ArcIntentHelperBridgeFactory() override = default;
};

// Base URL for the Chrome settings pages.
constexpr char kSettingsPageBaseUrl[] = "chrome://settings";

}  // namespace

// static
const char ArcIntentHelperBridge::kArcIntentHelperPackageName[] =
    "org.chromium.arc.intent_helper";

// static
ArcIntentHelperBridge* ArcIntentHelperBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcIntentHelperBridgeFactory::GetForBrowserContext(context);
}

// static
KeyedServiceBaseFactory* ArcIntentHelperBridge::GetFactory() {
  return ArcIntentHelperBridgeFactory::GetInstance();
}

ArcIntentHelperBridge::ArcIntentHelperBridge(content::BrowserContext* context,
                                             ArcBridgeService* bridge_service)
    : context_(context),
      arc_bridge_service_(bridge_service),
      binding_(this),
      open_url_delegate_(std::make_unique<OpenUrlDelegateImpl>()) {
  arc_bridge_service_->intent_helper()->AddObserver(this);
}

ArcIntentHelperBridge::~ArcIntentHelperBridge() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  arc_bridge_service_->intent_helper()->RemoveObserver(this);
}

void ArcIntentHelperBridge::OnInstanceReady() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* instance =
      ARC_GET_INSTANCE_FOR_METHOD(arc_bridge_service_->intent_helper(), Init);
  DCHECK(instance);
  mojom::IntentHelperHostPtr host_proxy;
  binding_.Bind(mojo::MakeRequest(&host_proxy));
  instance->Init(std::move(host_proxy));
}

void ArcIntentHelperBridge::OnInstanceClosed() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ArcIntentHelperBridge::OnIconInvalidated(const std::string& package_name) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  icon_loader_.InvalidateIcons(package_name);
}

void ArcIntentHelperBridge::OnOpenDownloads() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(607411): If the FileManager is not yet open this will open to
  // downloads by default, which is what we want.  However if it is open it will
  // simply be brought to the forgeground without forcibly being navigated to
  // downloads, which is probably not ideal.
  // TODO(mash): Support this functionality without ash::Shell access in Chrome.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->new_window_controller()->OpenFileManager();
}

void ArcIntentHelperBridge::OnOpenUrl(const std::string& url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Converts |url| to a fixed-up one. This converts about: URIs to chrome://,
  // for example.
  const GURL gurl(url_formatter::FixupURL(url, std::string()));
  // Disallow opening chrome:// URLs.
  if (!gurl.is_valid() || gurl.SchemeIs(kChromeUIScheme))
    return;
  open_url_delegate_->OpenUrl(gurl);
}

void ArcIntentHelperBridge::OnOpenChromeSettings(mojom::SettingsPage page) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Mapping from the mojom enum values to the URL components.
  const char* sub_url = nullptr;
  switch (page) {
    case mojom::SettingsPage::MULTIDEVICE:
      sub_url = "multidevice";
      break;
    case mojom::SettingsPage::MAIN:
      sub_url = "";
      break;
    case mojom::SettingsPage::POWER:
      sub_url = "power";
      break;
    case mojom::SettingsPage::BLUETOOTH:
      sub_url = "bluetoothDevices";
      break;
    case mojom::SettingsPage::DATETIME:
      sub_url = "dateTime";
      break;
    case mojom::SettingsPage::DISPLAY:
      sub_url = "display";
      break;
    case mojom::SettingsPage::WIFI:
      sub_url = "networks/?type=WiFi";
      break;
    case mojom::SettingsPage::LANGUAGE:
      sub_url = "languages";
      break;
    case mojom::SettingsPage::PRIVACY:
      sub_url = "privacy";
      break;
    case mojom::SettingsPage::HELP:
      sub_url = "help";
      break;
  }

  if (!sub_url) {
    LOG(ERROR) << "Invalid settings page: " << page;
    return;
  }
  open_url_delegate_->OpenUrl(GURL(kSettingsPageBaseUrl).Resolve(sub_url));
}

void ArcIntentHelperBridge::OpenWallpaperPicker() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO(mash): Support this functionality without ash::Shell access in Chrome.
  if (ash::Shell::HasInstance())
    ash::Shell::Get()->wallpaper_controller()->OpenSetWallpaperPage();
}

void ArcIntentHelperBridge::SetWallpaperDeprecated(
    const std::vector<uint8_t>& jpeg_data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  LOG(ERROR) << "IntentHelper.SetWallpaper is deprecated";
}

void ArcIntentHelperBridge::OpenVolumeControl() {
  auto* audio = ArcAudioBridge::GetForBrowserContext(context_);
  DCHECK(audio);
  audio->ShowVolumeControls();
}

ArcIntentHelperBridge::GetResult ArcIntentHelperBridge::GetActivityIcons(
    const std::vector<ActivityName>& activities,
    const OnIconsReadyCallback& callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return icon_loader_.GetActivityIcons(activities, callback);
}

bool ArcIntentHelperBridge::ShouldChromeHandleUrl(const GURL& url) {
  if (!url.SchemeIsHTTPOrHTTPS()) {
    // Chrome will handle everything that is not http and https.
    return true;
  }

  for (const IntentFilter& filter : intent_filters_) {
    if (filter.Match(url))
      return false;
  }

  // Didn't find any matches for Android so let Chrome handle it.
  return true;
}

void ArcIntentHelperBridge::AddObserver(ArcIntentHelperObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ArcIntentHelperBridge::RemoveObserver(ArcIntentHelperObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ArcIntentHelperBridge::HasObserver(
    ArcIntentHelperObserver* observer) const {
  return observer_list_.HasObserver(observer);
}

// static
bool ArcIntentHelperBridge::IsIntentHelperPackage(
    const std::string& package_name) {
  return package_name == kArcIntentHelperPackageName;
}

// static
std::vector<mojom::IntentHandlerInfoPtr>
ArcIntentHelperBridge::FilterOutIntentHelper(
    std::vector<mojom::IntentHandlerInfoPtr> handlers) {
  std::vector<mojom::IntentHandlerInfoPtr> handlers_filtered;
  for (auto& handler : handlers) {
    if (IsIntentHelperPackage(handler->package_name))
      continue;
    handlers_filtered.push_back(std::move(handler));
  }
  return handlers_filtered;
}

void ArcIntentHelperBridge::OnIntentFiltersUpdated(
    std::vector<IntentFilter> filters) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  intent_filters_ = std::move(filters);

  for (auto& observer : observer_list_)
    observer.OnIntentFiltersUpdated();
}

void ArcIntentHelperBridge::SetOpenUrlDelegateForTesting(
    std::unique_ptr<OpenUrlDelegate> open_url_delegate) {
  open_url_delegate_ = std::move(open_url_delegate);
}

}  // namespace arc
