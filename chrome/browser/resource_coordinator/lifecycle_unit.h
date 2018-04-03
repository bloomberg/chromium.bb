// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/process/process_handle.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/resource_coordinator/discard_reason.h"

namespace resource_coordinator {

class LifecycleUnitObserver;
class TabLifecycleUnitExternal;

// A LifecycleUnit represents a unit that can switch between the "loaded" and
// "discarded" states. When it is loaded, the unit uses system resources and
// provides functionality to the user. When it is discarded, the unit doesn't
// use any system resource.
class LifecycleUnit {
 public:
  enum class State {
    // The LifecycleUnit is using system resources.
    LOADED,
    // The LifecycleUnit is not using system resources.
    DISCARDED,
  };

  // Used to sort LifecycleUnit by importance. The most important LifecycleUnit
  // has the greatest SortKey.
  struct SortKey {
    SortKey();
    explicit SortKey(base::TimeTicks last_focused_time);

    bool operator<(const SortKey& other) const;
    bool operator>(const SortKey& other) const;

    // Last time at which the LifecycleUnit was focused. base::TimeTicks::Max()
    // if the LifecycleUnit is currently focused.
    base::TimeTicks last_focused_time;
  };

  virtual ~LifecycleUnit();

  // Returns the TabLifecycleUnitExternal associated with this LifecycleUnit, if
  // any.
  virtual TabLifecycleUnitExternal* AsTabLifecycleUnitExternal() = 0;

  // Returns a unique id representing this LifecycleUnit.
  virtual int32_t GetID() const = 0;

  // Returns a title describing this LifecycleUnit, or an empty string if no
  // title is available.
  virtual base::string16 GetTitle() const = 0;

  // Returns the URL of an icon for this LifecycleUnit, or an empty string if no
  // icon is available.
  virtual std::string GetIconURL() const = 0;

  // Returns the process hosting this LifecycleUnit. Used to distribute OOM
  // scores.
  //
  // TODO(fdoray): Change this to take into account the fact that a
  // LifecycleUnit can be hosted in multiple processes. https://crbug.com/775644
  virtual base::ProcessHandle GetProcessHandle() const = 0;

  // Returns a key that can be used to evaluate the relative importance of this
  // LifecycleUnit.
  //
  // TODO(fdoray): Figure out if GetSortKey() and CanDiscard() should be
  // replaced with a method that returns a numeric value representing the
  // expected user pain caused by a discard. A values above a given threshold
  // would be equivalent to CanDiscard() returning false for a given
  // DiscardReason. https://crbug.com/775644
  virtual SortKey GetSortKey() const = 0;

  // Returns the current state of this LifecycleUnit.
  virtual State GetState() const = 0;

  // Returns the last time that the visibility of the LifecycleUnit changed.
  virtual base::TimeTicks GetLastVisibilityChangeTime() const = 0;

  // Freezes this LifecycleUnit, i.e. prevents it from using the CPU. Returns
  // true on success.
  virtual bool Freeze() = 0;

  // Returns the estimated number of kilobytes that would be freed if this
  // LifecycleUnit was discarded.
  //
  // TODO(fdoray): Consider exposing this only on a new class that represents a
  // group of LifecycleUnits. It is easier to compute memory consumption
  // accurately for a group of LifecycleUnits that live in the same process(es)
  // than for individual LifecycleUnits. https://crbug.com/775644
  virtual int GetEstimatedMemoryFreedOnDiscardKB() const = 0;

  // Whether memory can be purged in the process hosting this LifecycleUnit.
  //
  // TODO(fdoray): This method should be on a class that represents a process,
  // not on a LifecycleUnit. https://crbug.com/775644
  virtual bool CanPurge() const = 0;

  // Returns true if this LifecycleUnit can be discared.
  virtual bool CanDiscard(DiscardReason reason) const = 0;

  // Discards this LifecycleUnit.
  //
  // TODO(fdoray): Consider handling urgent discard with groups of
  // LifecycleUnits. On urgent discard, we want to minimize memory accesses. It
  // is easier to achieve that if we discard a group of LifecycleUnits that live
  // in the same process(es) than if we discard individual LifecycleUnits.
  // https://crbug.com/775644
  virtual bool Discard(DiscardReason discard_reason) = 0;

  // Adds/removes an observer to this LifecycleUnit.
  virtual void AddObserver(LifecycleUnitObserver* observer) = 0;
  virtual void RemoveObserver(LifecycleUnitObserver* observer) = 0;
};

using LifecycleUnitSet = base::flat_set<LifecycleUnit*>;
using LifecycleUnitVector = std::vector<LifecycleUnit*>;

}  // namespace resource_coordinator

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_LIFECYCLE_UNIT_H_
