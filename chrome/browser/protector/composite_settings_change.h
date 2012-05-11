// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROTECTOR_COMPOSITE_SETTINGS_CHANGE_H_
#define CHROME_BROWSER_PROTECTOR_COMPOSITE_SETTINGS_CHANGE_H_
#pragma once

#include <queue>

#include "base/memory/scoped_vector.h"
#include "chrome/browser/protector/base_setting_change.h"

namespace protector {

// Class for tracking multiple settings changes as a single change.
class CompositeSettingsChange : public BaseSettingChange {
 public:
  CompositeSettingsChange();
  virtual ~CompositeSettingsChange();

  // BaseSettingChange overrides:
  virtual CompositeSettingsChange* MergeWith(BaseSettingChange* other) OVERRIDE;
  virtual bool Contains(const BaseSettingChange* other) const OVERRIDE;
  virtual void Apply(Browser* browser) OVERRIDE;
  virtual void Discard(Browser* browser) OVERRIDE;
  virtual void Timeout() OVERRIDE;
  virtual int GetBadgeIconID() const OVERRIDE;
  virtual int GetMenuItemIconID() const OVERRIDE;
  virtual int GetBubbleIconID() const OVERRIDE;
  virtual string16 GetBubbleTitle() const OVERRIDE;
  virtual string16 GetBubbleMessage() const OVERRIDE;
  virtual string16 GetApplyButtonText() const OVERRIDE;
  virtual string16 GetDiscardButtonText() const OVERRIDE;
  virtual DisplayName GetApplyDisplayName() const OVERRIDE;
  virtual GURL GetNewSettingURL() const OVERRIDE;
  virtual bool CanBeMerged() const OVERRIDE;

 private:
  // Vector with all merged changes.
  ScopedVector<BaseSettingChange> changes_;

  // Display names of merged changes, sorted by their priority. The name with
  // the highest priority is picked.
  std::priority_queue<DisplayName> apply_names_;

  DISALLOW_COPY_AND_ASSIGN(CompositeSettingsChange);
};

}  // namespace protector

#endif  // CHROME_BROWSER_PROTECTOR_COMPOSITE_SETTINGS_CHANGE_H_
