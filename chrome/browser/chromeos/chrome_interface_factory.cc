// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/chrome_interface_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/app_list/app_list_presenter_service.h"
#include "chrome/browser/ui/ash/keyboard_ui_service.h"
#include "chrome/browser/ui/browser_commands.h"
#include "services/shell/public/cpp/connection.h"

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

  void CreateNewTab() { NOTIMPLEMENTED(); }

  // mash::mojom::Launchable:
  void Launch(uint32_t what, mash::mojom::LaunchMode how) override {
    if (how != mash::mojom::LaunchMode::MAKE_NEW) {
      LOG(ERROR) << "Unable to handle Launch request with how = " << how;
      return;
    }
    switch (what) {
      case mash::mojom::kDocument:
        CreateNewTab();
        break;
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

namespace chromeos {

ChromeInterfaceFactory::ChromeInterfaceFactory() {}
ChromeInterfaceFactory::~ChromeInterfaceFactory() {}

bool ChromeInterfaceFactory::AcceptConnection(shell::Connection* connection) {
  connection->AddInterface<keyboard::mojom::Keyboard>(this);
  connection->AddInterface<mash::mojom::Launchable>(this);
  connection->AddInterface<app_list::mojom::AppListPresenter>(this);
  return true;
}

void ChromeInterfaceFactory::Create(
    shell::Connection* connection,
    mojo::InterfaceRequest<keyboard::mojom::Keyboard> request) {
  if (!keyboard_ui_service_)
    keyboard_ui_service_.reset(new KeyboardUIService);
  keyboard_bindings_.AddBinding(keyboard_ui_service_.get(), std::move(request));
}

void ChromeInterfaceFactory::Create(shell::Connection* connection,
                                    mash::mojom::LaunchableRequest request) {
  if (!launchable_)
    launchable_.reset(new ChromeLaunchable);
  launchable_->ProcessRequest(std::move(request));
}

void ChromeInterfaceFactory::Create(
    shell::Connection* connection,
    mojo::InterfaceRequest<app_list::mojom::AppListPresenter> request) {
  if (!app_list_presenter_service_)
    app_list_presenter_service_.reset(new AppListPresenterService);
  app_list_presenter_bindings_.AddBinding(app_list_presenter_service_.get(),
                                          std::move(request));
}

}  // namespace chromeos
