// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_ELEMENTS_TRANSIENT_ELEMENT_H_
#define CHROME_BROWSER_VR_ELEMENTS_TRANSIENT_ELEMENT_H_

#include "chrome/browser/vr/elements/textured_element.h"

namespace vr {

class TransientElement : public UiElement {
 public:
  explicit TransientElement(const base::TimeDelta& timeout);
  ~TransientElement() override;

  void OnBeginFrame(const base::TimeTicks& time) override;

  // Sets the elements visibility to the given value. If the visibility is
  // changing to true, it stays visible for the set timeout.
  void SetVisible(bool visible) override;
  // Resets the time this element stays visible for if the element is currently
  // visible.
  void RefreshVisible();

 private:
  typedef UiElement super;

  base::TimeDelta timeout_;
  base::TimeTicks set_visible_time_;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_ELEMENTS_TRANSIENT_ELEMENT_H_
