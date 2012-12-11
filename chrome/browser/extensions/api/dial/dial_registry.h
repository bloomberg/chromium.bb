// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_REGISTRY_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/api/dial/dial_device_data.h"

namespace extensions {

// Keeps track of devices that have responded to discovery requests and notifies
// the observer with the set of active devices has changed.  The registry's
// observer (i.e., the Dial API) owns the registry instance.
class DialRegistry {
 public:
  typedef std::vector<DialDeviceData> DeviceList;

  class Observer {
   public:
    // Invoked on the IO thread by the DialRegistry during a device event (new
    // device discovered or an update triggered by dial.discoverNow).
    virtual void OnDialDeviceEvent(const DeviceList& devices) = 0;

   protected:
    virtual ~Observer() {}
  };

  // Create the DIAL registry and pass a reference to allow it to notify on
  // DIAL device events.
  explicit DialRegistry(Observer *dial_api);
  virtual ~DialRegistry();

  // Called by the DIAL API when event listeners are added or removed. The dial
  // service is started after the first listener is added and stopped after the
  // last listener is removed.
  void OnListenerAdded();
  void OnListenerRemoved();

  // Called by the DIAL API to try to kickoff a discovery if there is not one
  // already active.
  bool DiscoverNow();

 private:
  // The current number of event listeners attached to this registry.
  int num_listeners_;

  // Whether discovering is currently active.
  bool discovering_;

  // Interface from which the DIAL API is notified of DIAL device events. the
  // DIAL API owns this DIAL registry.
  Observer* dial_api_;

  // Thread checker.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DialRegistry);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DIAL_DIAL_REGISTRY_H_
