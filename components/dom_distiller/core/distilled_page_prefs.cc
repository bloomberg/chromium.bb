// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/core/distilled_page_prefs.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/dom_distiller/core/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"

namespace dom_distiller {

DistilledPagePrefs::DistilledPagePrefs(PrefService* pref_service)
    : pref_service_(pref_service) {}

DistilledPagePrefs::~DistilledPagePrefs() = default;

// static
void DistilledPagePrefs::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      prefs::kTheme, static_cast<int32_t>(mojom::Theme::kLight),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(
      prefs::kFont, static_cast<int32_t>(mojom::FontFamily::kSansSerif),
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDoublePref(prefs::kFontScale, 1.0);
  registry->RegisterBooleanPref(
      prefs::kReaderForAccessibility, false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

void DistilledPagePrefs::SetFontFamily(mojom::FontFamily new_font_family) {
  pref_service_->SetInteger(prefs::kFont,
                            static_cast<int32_t>(new_font_family));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&DistilledPagePrefs::NotifyOnChangeFontFamily,
                     weak_ptr_factory_.GetWeakPtr(), new_font_family));
}

mojom::FontFamily DistilledPagePrefs::GetFontFamily() {
  auto font_family =
      static_cast<mojom::FontFamily>(pref_service_->GetInteger(prefs::kFont));
  if (mojom::IsKnownEnumValue(font_family))
    return font_family;

  // Persisted data was incorrect, trying to clean it up by storing the
  // default.
  SetFontFamily(mojom::FontFamily::kSansSerif);
  return mojom::FontFamily::kSansSerif;
}

void DistilledPagePrefs::SetTheme(mojom::Theme new_theme) {
  pref_service_->SetInteger(prefs::kTheme, static_cast<int32_t>(new_theme));
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DistilledPagePrefs::NotifyOnChangeTheme,
                                weak_ptr_factory_.GetWeakPtr(), new_theme));
}

mojom::Theme DistilledPagePrefs::GetTheme() {
  auto theme =
      static_cast<mojom::Theme>(pref_service_->GetInteger(prefs::kTheme));
  if (mojom::IsKnownEnumValue(theme))
    return theme;

  // Persisted data was incorrect, trying to clean it up by storing the
  // default.
  SetTheme(mojom::Theme::kLight);
  return mojom::Theme::kLight;
}

void DistilledPagePrefs::SetFontScaling(float scaling) {
  pref_service_->SetDouble(prefs::kFontScale, scaling);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&DistilledPagePrefs::NotifyOnChangeFontScaling,
                                weak_ptr_factory_.GetWeakPtr(), scaling));
}

float DistilledPagePrefs::GetFontScaling() {
  float scaling = pref_service_->GetDouble(prefs::kFontScale);
  if (scaling < 0.4 || scaling > 2.5) {
    // Persisted data was incorrect, trying to clean it up by storing the
    // default.
    SetFontScaling(1.0);
    return 1.0;
  }
  return scaling;
}

void DistilledPagePrefs::AddObserver(Observer* obs) {
  observers_.AddObserver(obs);
}

void DistilledPagePrefs::RemoveObserver(Observer* obs) {
  observers_.RemoveObserver(obs);
}

void DistilledPagePrefs::NotifyOnChangeFontFamily(
    mojom::FontFamily new_font_family) {
  for (Observer& observer : observers_)
    observer.OnChangeFontFamily(new_font_family);
}

void DistilledPagePrefs::NotifyOnChangeTheme(mojom::Theme new_theme) {
  for (Observer& observer : observers_)
    observer.OnChangeTheme(new_theme);
}

void DistilledPagePrefs::NotifyOnChangeFontScaling(float scaling) {
  for (Observer& observer : observers_)
    observer.OnChangeFontScaling(scaling);
}

}  // namespace dom_distiller
