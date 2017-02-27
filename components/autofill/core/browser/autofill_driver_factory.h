// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_FACTORY_H_
#define AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_FACTORY_H_

#include <memory>
#include <unordered_map>

#include "base/callback_forward.h"
#include "base/macros.h"

namespace autofill {

class AutofillClient;
class AutofillDriver;

// Manages the lifetime of AutofillDrivers for a particular AutofillClient by
// creating, notifying, retrieving and deleting on demand.
class AutofillDriverFactory {
 public:
  explicit AutofillDriverFactory(AutofillClient* client);

  ~AutofillDriverFactory();

  // A convenience function to retrieve an AutofillDriver for the given key or
  // null if there is none.
  AutofillDriver* DriverForKey(void* key);

  // Handles finished navigation in any of the frames.
  void NavigationFinished();

  // Handles hiding of the corresponding tab.
  void TabHidden();

  // Call this to notify the factory that one of the frames saw a user gesture.
  // The factory will distribute this information to all drivers when it comes
  // for the first time since the last main frame navigation to a different
  // page. It will also notify drivers added later (see AddForKey).
  void OnFirstUserGestureObserved();

  AutofillClient* client() { return client_; };

 protected:
  // The API manipulating the drivers map is protected to guarantee subclasses
  // that nothing else can interfere with the map of drivers.

  // Adds a driver, constructed by calling |factory_method|, for |key|. If there
  // already is a driver for |key|, |factory_method| is not called. This might
  // end up notifying the driver that a user gesture has been observed.
  void AddForKey(
      void* key,
      base::Callback<std::unique_ptr<AutofillDriver>()> factory_method);

  // Deletes the AutofillDriver for |key|.
  void DeleteForKey(void* key);

 private:
  AutofillClient* const client_;

  std::unordered_map<void*, std::unique_ptr<AutofillDriver>> driver_map_;

  bool user_gesture_seen_ = false;  // The state for OnFirstUserGestureObserved.

  DISALLOW_COPY_AND_ASSIGN(AutofillDriverFactory);
};

}  // namespace autofill

#endif  // AUTOFILL_CORE_BROWSER_AUTOFILL_DRIVER_FACTORY_H_
