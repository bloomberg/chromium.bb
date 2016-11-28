// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_interface_factory.h"

#include <memory>
#include <utility>

#include "ash/common/mojo_interface_factory.h"
#include "ash/public/interfaces/new_window.mojom.h"
#include "ash/public/interfaces/shutdown.mojom.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/login/users/wallpaper/wallpaper_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/app_list/app_list_presenter_service.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/ash/chrome_new_window_client.h"
#include "chrome/browser/ui/ash/keyboard_ui_service.h"
#include "chrome/browser/ui/ash/volume_controller_chromeos.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/common/service_manager_connection.h"
#include "mash/public/interfaces/launchable.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#include "ui/app_list/presenter/app_list_presenter.mojom.h"
#include "ui/keyboard/keyboard.mojom.h"

namespace chromeos {

namespace {

class ChromeLaunchable : public mash::mojom::Launchable {
 public:
  ChromeLaunchable() {}
  ~ChromeLaunchable() override {}

  void ProcessRequest(mash::mojom::LaunchableRequest request) {
    bindings_.AddBinding(this, std::move(request));
  }

 private:
  void CreateNewWindowImpl(bool is_incognito) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    chrome::NewEmptyWindow(is_incognito ? profile->GetOffTheRecordProfile()
                                        : profile);
  }

  void CreateNewTab() { chrome::NewTab(chrome::FindLastActive()); }

  // mash::mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override {
    if (how != mash::mojom::LaunchMode::MAKE_NEW) {
      LOG(ERROR) << "Unable to handle Launch request with how = " << how;
      return;
    }
    switch (what) {
      case mash::mojom::kWindow:
        CreateNewWindowImpl(false /* is_incognito */);
        break;
      case mash::mojom::kIncognitoWindow:
        CreateNewWindowImpl(true /* is_incognito */);
        break;
      default:
        NOTREACHED();
    }
  }

  mojo::BindingSet<mash::mojom::Launchable> bindings_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLaunchable);
};

class FactoryImpl {
 public:
  FactoryImpl() {}
  ~FactoryImpl() {}

  template <typename Interface>
  static void AddFactory(
      service_manager::InterfaceRegistry* registry,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
    registry->AddInterface<Interface>(
        base::Bind(&FactoryImpl::CallMainThreadFactory<Interface>),
        task_runner);
  }

 private:
  static FactoryImpl* Get() {
    if (!factory_.Get())
      factory_.Get() = base::MakeUnique<FactoryImpl>();
    return factory_.Get().get();
  }

  template <typename Interface>
  static void CallMainThreadFactory(mojo::InterfaceRequest<Interface> request) {
    Get()->BindRequest(std::move(request));
  }

  void BindRequest(keyboard::mojom::KeyboardRequest request) {
    if (!keyboard_ui_service_)
      keyboard_ui_service_ = base::MakeUnique<KeyboardUIService>();
    keyboard_bindings_.AddBinding(keyboard_ui_service_.get(),
                                  std::move(request));
  }

  void BindRequest(mash::mojom::LaunchableRequest request) {
    if (!launchable_)
      launchable_ = base::MakeUnique<ChromeLaunchable>();
    launchable_->ProcessRequest(std::move(request));
  }

  void BindRequest(ash::mojom::NewWindowClientRequest request) {
    if (!new_window_client_)
      new_window_client_.reset(new ChromeNewWindowClient);
    new_window_client_bindings_.AddBinding(new_window_client_.get(),
                                           std::move(request));
  }

  void BindRequest(ash::mojom::WallpaperManagerRequest request) {
    WallpaperManager::Get()->BindRequest(std::move(request));
  }

  void BindRequest(ash::mojom::VolumeControllerRequest request) {
    if (!volume_controller_)
      volume_controller_ = base::MakeUnique<VolumeController>();
    volume_controller_->BindRequest(std::move(request));
  }

  void BindRequest(app_list::mojom::AppListPresenterRequest request) {
    if (!app_list_presenter_service_)
      app_list_presenter_service_ = base::MakeUnique<AppListPresenterService>();
    app_list_presenter_bindings_.AddBinding(app_list_presenter_service_.get(),
                                            std::move(request));
  }

  static base::LazyInstance<std::unique_ptr<FactoryImpl>>::Leaky factory_;

  std::unique_ptr<KeyboardUIService> keyboard_ui_service_;
  mojo::BindingSet<keyboard::mojom::Keyboard> keyboard_bindings_;
  std::unique_ptr<ChromeLaunchable> launchable_;
  std::unique_ptr<ChromeNewWindowClient> new_window_client_;
  mojo::BindingSet<ash::mojom::NewWindowClient> new_window_client_bindings_;
  std::unique_ptr<VolumeController> volume_controller_;
  std::unique_ptr<AppListPresenterService> app_list_presenter_service_;
  mojo::BindingSet<app_list::mojom::AppListPresenter>
      app_list_presenter_bindings_;

  DISALLOW_COPY_AND_ASSIGN(FactoryImpl);
};

base::LazyInstance<std::unique_ptr<FactoryImpl>>::Leaky FactoryImpl::factory_ =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

ChromeInterfaceFactory::ChromeInterfaceFactory()
    : main_thread_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

ChromeInterfaceFactory::~ChromeInterfaceFactory() {}

bool ChromeInterfaceFactory::OnConnect(
    const service_manager::Identity& remote_identity,
    service_manager::InterfaceRegistry* registry,
    service_manager::Connector* connector) {
  // TODO(jamescook): Only register the interfaces needed for a particular
  // |remote_identity|. For example, a connection from service:ash needs these,
  // but a connection from service:content_gpu does not.
  FactoryImpl::AddFactory<keyboard::mojom::Keyboard>(registry,
                                                     main_thread_task_runner_);
  FactoryImpl::AddFactory<mash::mojom::Launchable>(registry,
                                                   main_thread_task_runner_);
  FactoryImpl::AddFactory<ash::mojom::NewWindowClient>(
      registry, main_thread_task_runner_);
  FactoryImpl::AddFactory<ash::mojom::VolumeController>(
      registry, main_thread_task_runner_);
  FactoryImpl::AddFactory<ash::mojom::WallpaperManager>(
      registry, main_thread_task_runner_);
  FactoryImpl::AddFactory<app_list::mojom::AppListPresenter>(
      registry, main_thread_task_runner_);

  // In classic ash, the browser process provides ash services to itself.
  if (!chrome::IsRunningInMash()) {
    ash::mojo_interface_factory::RegisterInterfaces(registry,
                                                    main_thread_task_runner_);
  }

  return true;
}

}  // namespace chromeos
