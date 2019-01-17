// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_
#define COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_

namespace send_tab_to_self {

// Observer for the Send Tab To Self model. In the observer methods care should
// be taken to not modify the model.
class SendTabToSelfModelObserver {
 public:
  SendTabToSelfModelObserver() {}
  virtual ~SendTabToSelfModelObserver() {}

  // Invoked when the model has finished loading. Until this method is called it
  // is unsafe to use the model.
  virtual void SendTabToSelfModelLoaded() = 0;

  // Invoked when elements of the model are added, removed, or updated.
  virtual void SendTabToSelfModelChanged() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfModelObserver);
};

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_MODEL_OBSERVER_H_
