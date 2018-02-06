// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/magnifier/docked_magnifier_controller.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller.h"
#include "ash/shell.h"
#include "base/numerics/ranges.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

constexpr float kDefaultMagnifierScale = 2.0f;
constexpr float kMinMagnifierScale = 1.0f;
constexpr float kMaxMagnifierScale = 10.0f;

}  // namespace

DockedMagnifierController::DockedMagnifierController() : binding_(this) {
  Shell::Get()->session_controller()->AddObserver(this);
}

DockedMagnifierController::~DockedMagnifierController() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void DockedMagnifierController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kDockedMagnifierEnabled, false,
                                PrefRegistry::PUBLIC);
  registry->RegisterDoublePref(prefs::kDockedMagnifierScale,
                               kDefaultMagnifierScale, PrefRegistry::PUBLIC);
}

void DockedMagnifierController::BindRequest(
    mojom::DockedMagnifierControllerRequest request) {
  binding_.Bind(std::move(request));
}

bool DockedMagnifierController::GetEnabled() const {
  return active_user_pref_service_ &&
         active_user_pref_service_->GetBoolean(prefs::kDockedMagnifierEnabled);
}

float DockedMagnifierController::GetScale() const {
  if (active_user_pref_service_)
    return active_user_pref_service_->GetDouble(prefs::kDockedMagnifierScale);

  return kDefaultMagnifierScale;
}

void DockedMagnifierController::SetEnabled(bool enabled) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetBoolean(prefs::kDockedMagnifierEnabled,
                                          enabled);
  }
}

void DockedMagnifierController::SetScale(float scale) {
  if (active_user_pref_service_) {
    active_user_pref_service_->SetDouble(
        prefs::kDockedMagnifierScale,
        base::ClampToRange(scale, kMinMagnifierScale, kMaxMagnifierScale));
  }
}

void DockedMagnifierController::SetClient(
    mojom::DockedMagnifierClientPtr client) {
  client_ = std::move(client);
  NotifyClientWithStatusChanged();
}

void DockedMagnifierController::CenterOnPoint(
    const gfx::Point& point_in_screen) {
  // TODO(afakhry): Implement logic here.
}

void DockedMagnifierController::OnActiveUserPrefServiceChanged(
    PrefService* pref_service) {
  active_user_pref_service_ = pref_service;
  InitFromUserPrefs();
}

void DockedMagnifierController::FlushClientPtrForTesting() {
  client_.FlushForTesting();
}

void DockedMagnifierController::InitFromUserPrefs() {
  DCHECK(active_user_pref_service_);

  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(active_user_pref_service_);
  pref_change_registrar_->Add(
      prefs::kDockedMagnifierEnabled,
      base::BindRepeating(&DockedMagnifierController::OnEnabledPrefChanged,
                          base::Unretained(this)));
  pref_change_registrar_->Add(
      prefs::kDockedMagnifierScale,
      base::BindRepeating(&DockedMagnifierController::OnScalePrefChanged,
                          base::Unretained(this)));

  Refresh();
  NotifyClientWithStatusChanged();
}

void DockedMagnifierController::OnEnabledPrefChanged() {
  Refresh();
  NotifyClientWithStatusChanged();
}

void DockedMagnifierController::OnScalePrefChanged() {
  Refresh();
}

void DockedMagnifierController::Refresh() {
  // TODO(afakhry): Implement logic here.
}

void DockedMagnifierController::NotifyClientWithStatusChanged() {
  if (client_)
    client_->OnEnabledStatusChanged(GetEnabled());
}

}  // namespace ash
