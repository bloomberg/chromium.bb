// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_BASE_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_BASE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit.h"
#include "content/public/browser/visibility.h"

namespace resource_coordinator {

// Base class for a LifecycleUnit.
class LifecycleUnitBase : public LifecycleUnit {
 public:
  LifecycleUnitBase();
  ~LifecycleUnitBase() override;

  // LifecycleUnit:
  int32_t GetID() const override;
  State GetState() const override;
  base::TimeTicks GetLastVisibilityChangeTime() const override;
  void AddObserver(LifecycleUnitObserver* observer) override;
  void RemoveObserver(LifecycleUnitObserver* observer) override;

 protected:
  // Sets the state of this LifecycleUnit to |state| and notifies observers.
  void SetState(State state);

  // Notifies observers that the visibility of the LifecycleUnit has changed.
  void OnLifecycleUnitVisibilityChanged(content::Visibility visibility);

  // Notifies observers that the LifecycleUnit is being destroyed. This is
  // invoked by derived classes rather than by the base class to avoid notifying
  // observers when the LifecycleUnit has been partially destroyed.
  void OnLifecycleUnitDestroyed();

 private:
  static int32_t next_id_;

  // A unique id representing this LifecycleUnit.
  const int32_t id_ = ++next_id_;

  // Current state of this LifecycleUnit.
  State state_ = State::LOADED;

  base::TimeTicks last_visibility_change_time_;

  base::ObserverList<LifecycleUnitObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(LifecycleUnitBase);
};

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_BASE_H_
