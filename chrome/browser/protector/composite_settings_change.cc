// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/protector/composite_settings_change.h"

#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace protector {


CompositeSettingsChange::CompositeSettingsChange() {
}

CompositeSettingsChange::~CompositeSettingsChange() {
}

CompositeSettingsChange* CompositeSettingsChange::MergeWith(
    BaseSettingChange* other) {
  DCHECK(profile());
  DCHECK(other);
  changes_.push_back(other);
  apply_names_.push(other->GetApplyDisplayName());
  return this;
}

bool CompositeSettingsChange::Contains(const BaseSettingChange* other) const {
  return this == other ||
      std::find(changes_.begin(), changes_.end(), other) != changes_.end();
}

// Apart from PrefsBackupInvalidChange, which should never appear with other
// Preferences-related changes together, the Apply/Discard logic of change
// classes does not overlap, so it's safe to call them in any order.

void CompositeSettingsChange::Apply(Browser* browser) {
  DVLOG(1) << "Apply all changes";
  for (size_t i = 0; i < changes_.size(); ++i)
    changes_[i]->Apply(browser);
}

void CompositeSettingsChange::Discard(Browser* browser) {
  DVLOG(1) << "Discard all changes";
  for (size_t i = 0; i < changes_.size(); ++i)
    changes_[i]->Discard(browser);
}

void CompositeSettingsChange::Timeout() {
  DVLOG(1) << "Timeout all changes";
  for (size_t i = 0; i < changes_.size(); ++i)
    changes_[i]->Timeout();
}

int CompositeSettingsChange::GetBadgeIconID() const {
  DCHECK(changes_.size());
  // Use icons from the first change.
  // TODO(ivankr): need something better, maybe a special icon.
  return changes_[0]->GetBadgeIconID();
}

int CompositeSettingsChange::GetMenuItemIconID() const {
  DCHECK(changes_.size());
  return changes_[0]->GetMenuItemIconID();
}

int CompositeSettingsChange::GetBubbleIconID() const {
  DCHECK(changes_.size());
  return changes_[0]->GetBubbleIconID();
}

string16 CompositeSettingsChange::GetBubbleTitle() const {
  return l10n_util::GetStringUTF16(IDS_SETTING_CHANGE_TITLE);
}

string16 CompositeSettingsChange::GetBubbleMessage() const {
  // TODO(ivankr): indicate what kind of changes happened (i.e., "something
  // tried to change your search engine, startup pages, etc.").
  return l10n_util::GetStringUTF16(IDS_SETTING_CHANGE_BUBBLE_MESSAGE);
}

string16 CompositeSettingsChange::GetApplyButtonText() const {
  DCHECK(apply_names_.size());
  string16 apply_text = apply_names_.top().second;
  return apply_text.empty() ?
      l10n_util::GetStringUTF16(IDS_CHANGE_SETTING_NO_NAME) :
      l10n_util::GetStringFUTF16(IDS_CHANGE_SETTING, apply_text);
}

string16 CompositeSettingsChange::GetDiscardButtonText() const {
  return l10n_util::GetStringUTF16(IDS_KEEP_SETTING);
}

BaseSettingChange::DisplayName
CompositeSettingsChange::GetApplyDisplayName() const {
  // CompositeSettingsChange should never be put inside another one.
  NOTREACHED();
  return BaseSettingChange::GetApplyDisplayName();
}

GURL CompositeSettingsChange::GetNewSettingURL() const {
  DCHECK(changes_.size());
  return changes_[0]->GetNewSettingURL();
}

bool CompositeSettingsChange::CanBeMerged() const {
  // We did that already, why not do it again.
  return true;
}

}  // namespace protector
