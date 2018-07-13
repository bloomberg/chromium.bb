// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TRANSIENT_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TRANSIENT_ELEMENT_H_

#include "base/callback.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/vr_ui_export.h"

namespace vr {

// Base class for a transient element that automatically hides itself after some
// point in time. The exacly transience behavior depends on the subclass.
class VR_UI_EXPORT TransientElement : public UiElement {
 public:
  ~TransientElement() override;

  // Sets the elements visibility to the given value. If the visibility is
  // changing to true, it stays visible for the set timeout.
  void SetVisible(bool visible) override;
  void SetVisibleImmediately(bool visible) override;

  // Resets the time this element stays visible for if the element is currently
  // visible.
  void RefreshVisible();

 protected:
  explicit TransientElement(const base::TimeDelta& timeout);
  virtual void Reset();

  base::TimeDelta timeout_;
  base::TimeTicks set_visible_time_;

 private:
  typedef UiElement super;

  DISALLOW_COPY_AND_ASSIGN(TransientElement);
};

// An element that hides itself after after a set timeout.
class VR_UI_EXPORT SimpleTransientElement : public TransientElement {
 public:
  explicit SimpleTransientElement(const base::TimeDelta& timeout);
  ~SimpleTransientElement() override;

 private:
  bool OnBeginFrame(const gfx::Transform& head_pose) override;

  typedef TransientElement super;

  DISALLOW_COPY_AND_ASSIGN(SimpleTransientElement);
};

// The reason why a transient element hid itself. Note that this is only used by
// ShowUntilSignalTransientElement below.
enum class TransientElementHideReason : int {
  kTimeout,
  kSignal,
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TRANSIENT_ELEMENT_H_
