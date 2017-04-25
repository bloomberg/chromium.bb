// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/tether/tether_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/chromeos/net/tether_notification_presenter.h"
#include "chrome/browser/chromeos/tether/tether_service_factory.h"
#include "chrome/browser/cryptauth/chrome_cryptauth_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/components/tether/initializer.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_state_handler.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ui/message_center/message_center.h"

// static
TetherService* TetherService::Get(Profile* profile) {
  return TetherServiceFactory::GetForBrowserContext(profile);
}

// static
void TetherService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kInstantTetheringAllowed, true);
  chromeos::tether::Initializer::RegisterProfilePrefs(registry);
}

TetherService::TetherService(Profile* profile)
    : profile_(profile), shut_down_(false) {
  registrar_.Init(profile_->GetPrefs());
  registrar_.Add(
      prefs::kInstantTetheringAllowed,
      base::Bind(&TetherService::OnPrefsChanged, base::Unretained(this)));
}

TetherService::~TetherService() {}

void TetherService::StartTether() {
  // TODO(hansberry): Switch to using an IsEnabled() method.
  if (!IsAllowed() || !IsEnabled()) {
    return;
  }

  auto notification_presenter =
      base::MakeUnique<chromeos::tether::TetherNotificationPresenter>(
          message_center::MessageCenter::Get(),
          chromeos::NetworkConnect::Get());
  chromeos::tether::Initializer::Init(
      ChromeCryptAuthServiceFactory::GetForBrowserContext(profile_),
      std::move(notification_presenter), profile_->GetPrefs(),
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_),
      chromeos::NetworkHandler::Get()->network_state_handler(),
      chromeos::NetworkHandler::Get()->managed_network_configuration_handler(),
      chromeos::NetworkConnect::Get(),
      chromeos::NetworkHandler::Get()->network_connection_handler());
}

bool TetherService::IsAllowed() const {
  if (shut_down_)
    return false;

  return profile_->GetPrefs()->GetBoolean(prefs::kInstantTetheringAllowed);
}

bool TetherService::IsEnabled() const {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      chromeos::switches::kEnableTether);
}

void TetherService::Shutdown() {
  if (shut_down_)
    return;

  shut_down_ = true;
  registrar_.RemoveAll();
  chromeos::tether::Initializer::Shutdown();
}

void TetherService::OnPrefsChanged() {
  if (IsAllowed() && IsEnabled()) {
    StartTether();
    return;
  }

  chromeos::tether::Initializer::Shutdown();
}
