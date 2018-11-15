// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_

#include <string>

namespace autofill_assistant {
// UI delegate called for script executions.
class UiDelegate {
 public:
  virtual ~UiDelegate() = default;

  // Called when autofill assistant can start executing scripts.
  virtual void Start(const GURL& initialUrl) = 0;

  // Called when the overlay has been clicked.
  virtual void OnClickOverlay() = 0;

  // Called when the Autofill Assistant should be destroyed, e.g. the tab
  // detached from the associated activity.
  virtual void OnDestroy() = 0;

  // Scrolls the browser view.
  //
  // Distance is expressed as a float between -1.0 and 1.0, relative to the
  // width or height of the visible viewport.
  virtual void ScrollBy(float distanceXRatio, float distanceYRatio) = 0;

  // Asks for updated coordinates for the touchable area. This is called to
  // speed up update of the touchable areas when there are good reasons to think
  // that the current coordinates are out of date, such as while scrolling.
  virtual void UpdateTouchableArea() = 0;

  // Called when a script was selected for execution.
  virtual void OnScriptSelected(const std::string& script_path) = 0;

  // Returns a string describing the current execution context. This is useful
  // when analyzing feedback forms and for debugging in general.
  virtual std::string GetDebugContext() = 0;

 protected:
  UiDelegate() = default;
};
}  // namespace autofill_assistant
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_UI_DELEGATE_H_
