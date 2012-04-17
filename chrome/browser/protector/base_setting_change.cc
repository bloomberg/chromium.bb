// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/base_setting_change.h"
#include "chrome/browser/protector/composite_settings_change.h"

#include "base/logging.h"

namespace protector {

const size_t kDefaultSearchProviderChangeNamePriority = 100U;
const size_t kSessionStartupChangeNamePriority = 50U;
const size_t kHomepageChangeNamePriority = 10U;

// static
const size_t BaseSettingChange::kDefaultNamePriority = 0U;

BaseSettingChange::BaseSettingChange()
    : profile_(NULL) {
}

BaseSettingChange::~BaseSettingChange() {
}

CompositeSettingsChange* BaseSettingChange::MergeWith(
    BaseSettingChange* other) {
  CompositeSettingsChange* composite_change = new CompositeSettingsChange();
  CHECK(composite_change->Init(profile_));
  composite_change->MergeWith(this);
  composite_change->MergeWith(other);
  return composite_change;
}

bool BaseSettingChange::Contains(const BaseSettingChange* other) const {
  // BaseSettingChange can only contain itself.
  return this == other;
}

bool BaseSettingChange::Init(Profile* profile) {
  DCHECK(profile && !profile_);
  profile_ = profile;
  return true;
}

void BaseSettingChange::InitWhenDisabled(Profile* profile) {
  DCHECK(profile && !profile_);
}

void BaseSettingChange::Apply(Browser* browser) {
}

void BaseSettingChange::Discard(Browser* browser) {
}

void BaseSettingChange::Timeout() {
}

BaseSettingChange::DisplayName BaseSettingChange::GetApplyDisplayName() const {
  return DisplayName(kDefaultNamePriority, string16());
}

GURL BaseSettingChange::GetNewSettingURL() const {
  return GURL();
}

bool BaseSettingChange::CanBeMerged() const {
  // By default change can be collapsed if it has a non-empty keyword.
  return !GetNewSettingURL().is_empty();
}

bool BaseSettingChange::IsUserVisible() const {
  return true;
}

}  // namespace protector
