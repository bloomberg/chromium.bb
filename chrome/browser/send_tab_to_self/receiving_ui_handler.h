// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_H_
#define CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_H_

#include <string>
#include <vector>

namespace send_tab_to_self {

class SendTabToSelfEntry;

// Interface implemented by platforms to handle changes to the SendTabToSelf
// model. sImplementors of this interface should override all functions and
// update the UI accordingly. They should also register themselves with the
// ReceivingUIRegistry.
class ReceivingUiHandler {
 public:
  ReceivingUiHandler() {}
  virtual ~ReceivingUiHandler() {}

  // Display the new entry passed in as an argument. The entry is owned by the
  // model and should not be modified.
  virtual void DisplayNewEntry(const SendTabToSelfEntry* entry) = 0;
  // Dismiss any UI associated with this entry.
  // Entry object is owned by the the model and should not be
  // modified by any implementors of this class.
  // TODO(crbug.com/935719): Figure out whether we need to pass the entire entry
  // or we can pass a smaller subset of information.
  virtual void DismissEntries(const std::vector<std::string>& guids) = 0;
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_SEND_TAB_TO_SELF_RECEIVING_UI_HANDLER_H_
