// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_ACTION_ADAPTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_ACTION_ADAPTER_H_

namespace extensions {

// Adapts an object to receive actions from the Automation extension API.
class AutomationActionAdapter {
 public:
  // Performs a default action (e.g. click, check) on the target node.
  virtual void DoDefault(int32 id) = 0;

  // Performs a focus action on the target node.
  virtual void Focus(int32 id) = 0;

  // Makes the node visible by scrolling; does not change nodes from hidden to
  // shown.
  virtual void MakeVisible(int32 id) = 0;

  // Sets selection for a start and end index (usually only relevant on text
  // fields).
  virtual void SetSelection(int32 id, int32 start, int32 end) = 0;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_ACTION_ADAPTER_H_
