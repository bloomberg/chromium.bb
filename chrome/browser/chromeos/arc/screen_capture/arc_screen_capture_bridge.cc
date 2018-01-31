// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/screen_capture/arc_screen_capture_bridge.h"

#include "ash/shell.h"
#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/arc/screen_capture/arc_screen_capture_session.h"
#include "chrome/browser/media/webrtc/desktop_media_list_ash.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "content/public/browser/browser_thread.h"

namespace arc {
namespace {

// Singleton factory for ArcScreenCaptureBridge
class ArcScreenCaptureBridgeFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcScreenCaptureBridge,
          ArcScreenCaptureBridgeFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase.
  static constexpr const char* kName = "ArcScreenCaptureBridgeFactory";

  static ArcScreenCaptureBridgeFactory* GetInstance() {
    return base::Singleton<ArcScreenCaptureBridgeFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcScreenCaptureBridgeFactory>;
  ArcScreenCaptureBridgeFactory() = default;
  ~ArcScreenCaptureBridgeFactory() override = default;
};

}  // namespace

// static
ArcScreenCaptureBridge* ArcScreenCaptureBridge::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcScreenCaptureBridgeFactory::GetForBrowserContext(context);
}

ArcScreenCaptureBridge::ArcScreenCaptureBridge(content::BrowserContext* context,
                                               ArcBridgeService* bridge_service)
    : arc_bridge_service_(bridge_service), weak_factory_(this) {
  arc_bridge_service_->screen_capture()->SetHost(this);
}

ArcScreenCaptureBridge::~ArcScreenCaptureBridge() {
  arc_bridge_service_->screen_capture()->SetHost(nullptr);
}

void ArcScreenCaptureBridge::RequestPermission(
    const std::string& display_name,
    const std::string& package_name,
    RequestPermissionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  std::unique_ptr<DesktopMediaPicker> picker = DesktopMediaPicker::Create();
  std::vector<std::unique_ptr<DesktopMediaList>> source_lists;
  source_lists.emplace_back(std::make_unique<DesktopMediaListAsh>(
      content::DesktopMediaID::TYPE_SCREEN));
  const base::string16 display_name16 = base::UTF8ToUTF16(display_name);
  DesktopMediaPicker::Params picker_params;
  picker_params.context = ash::Shell::GetRootWindowForDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  picker_params.modality = ui::ModalType::MODAL_TYPE_SYSTEM;
  picker_params.app_name = display_name16;
  picker_params.target_name = display_name16;
  picker->Show(
      picker_params, std::move(source_lists),
      base::BindRepeating(&ArcScreenCaptureBridge::PermissionPromptCallback,
                          base::Unretained(this), base::Passed(&picker),
                          display_name, package_name, base::Passed(&callback)));
}

void ArcScreenCaptureBridge::PermissionPromptCallback(
    std::unique_ptr<DesktopMediaPicker> picker,
    const std::string& display_name,
    const std::string& package_name,
    RequestPermissionCallback callback,
    content::DesktopMediaID desktop_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (desktop_id.is_null()) {
    std::move(callback).Run(false);
    return;
  }
  // This may overwrite an existing entry which is OK since these persist
  // forever and this may be requested again with a different desktop.
  permissions_map_[package_name] =
      std::make_unique<GrantedCaptureParams>(display_name, desktop_id);
  std::move(callback).Run(true);
}

void ArcScreenCaptureBridge::OpenSession(
    mojom::ScreenCaptureSessionNotifierPtr notifier,
    const std::string& package_name,
    const gfx::Size& size,
    OpenSessionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto found = permissions_map_.find(package_name);
  if (found == permissions_map_.end()) {
    LOG(ERROR) << "Attempt to open screen capture session without granted "
                  "permissions for package "
               << package_name;
    std::move(callback).Run(nullptr);
    return;
  }
  DCHECK(found->second) << package_name;
  std::move(callback).Run(ArcScreenCaptureSession::Create(
      std::move(notifier), found->second->display_name,
      found->second->desktop_id, size));
}

}  // namespace arc
