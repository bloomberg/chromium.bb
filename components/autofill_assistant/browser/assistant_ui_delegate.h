// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_UI_DELEGATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_UI_DELEGATE_H_

namespace autofill_assistant {
// UI delegate called by assistant UI.
class AssistantUiDelegate {
 public:
  virtual ~AssistantUiDelegate() = default;

  // Called when the overlay has been clicked by user.
  virtual void OnClickOverlay() = 0;

  // Called when the Autofill Assistant should be destroyed, e.g. the tab
  // detached from the associated activity.
  virtual void OnDestroy() = 0;

 protected:
  AssistantUiDelegate() = default;
};
}  // namespace autofill_assistant.
#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_ASSISTANT_UI_DELEGATE_H_