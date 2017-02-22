// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefs/preferences_connection_manager.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/preferences_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"

namespace {

// Required singleton subclass to control dependencies between
// PreferenceConnectionManager and the KeyedServiceShutdownNotifier.
class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance() {
    return base::Singleton<ShutdownNotifierFactory>::get();
  }

 private:
  friend struct base::DefaultSingletonTraits<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "PreferencesConnectionManager") {
    DependsOn(ProfileOAuth2TokenServiceFactory::GetInstance());
  }
  ~ShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(ShutdownNotifierFactory);
};

}  // namespace

PreferencesConnectionManager::PreferencesConnectionManager() {}

PreferencesConnectionManager::~PreferencesConnectionManager() {}

void PreferencesConnectionManager::OnProfileDestroyed() {
  // Shutdown any PreferenceService that is still alive.
  manager_bindings_.CloseAllBindings();
  profile_shutdown_notification_.reset();
}

void PreferencesConnectionManager::Create(
    prefs::mojom::PreferencesServiceClientPtr client,
    prefs::mojom::PreferencesServiceRequest service) {
  // Certain tests have no profiles to connect to, and static initializers
  // which block the creation of test profiles.
  if (!g_browser_process->profile_manager()->GetNumberOfProfiles())
    return;

  manager_bindings_.AddBinding(
      base::MakeUnique<PreferencesService>(
          std::move(client), ProfileManager::GetActiveUserProfile()),
      std::move(service));
}

void PreferencesConnectionManager::Create(
    const service_manager::Identity& remote_identity,
    prefs::mojom::PreferencesServiceFactoryRequest request) {
  factory_bindings_.AddBinding(this, std::move(request));

  if (!profile_shutdown_notification_) {
    profile_shutdown_notification_ =
        ShutdownNotifierFactory::GetInstance()
            ->Get(ProfileManager::GetActiveUserProfile())
            ->Subscribe(
                base::Bind(&PreferencesConnectionManager::OnProfileDestroyed,
                           base::Unretained(this)));
  }
}

bool PreferencesConnectionManager::OnConnect(
    const service_manager::ServiceInfo& remote_info,
    service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<prefs::mojom::PreferencesServiceFactory>(this);
  return true;
}
