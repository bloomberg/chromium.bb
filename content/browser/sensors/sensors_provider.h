// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SENSORS_PROVIDER_H_
#define CONTENT_BROWSER_SENSORS_PROVIDER_H_
#pragma once

#include "base/basictypes.h"
#include "content/common/content_export.h"
#include "content/public/browser/sensors_listener.h"

// The sensors API will unify various types of sensor data into a set of
// channels, each of which provides change events and periodic updates.
//
// This version of the API is intended only to support the experimental screen
// rotation code and is not for general use. In particular, the final listener
// will declare generic |OnSensorChanged| and |OnSensorUpdated| methods, rather
// than the special-purpose |OnScreenOrientationChanged|.

namespace sensors {

// TODO(cwolfe): Finish defining the initial set of channels and replace this
// with the generic sensor provider.
//
class CONTENT_EXPORT Provider {
 public:
  static Provider* GetInstance();

  // Adds a sensor listener. The listener will receive callbacks to indicate
  // sensor changes and updates until it is removed.
  //
  // This method may be called in any thread. Callbacks on a listener will
  // always be executed in the thread which added that listener.
  virtual void AddListener(content::SensorsListener* listener) = 0;

  // Removes a sensor listener.
  //
  // This method must be called in the same thread which added the listener.
  // If the listener is not currently subscribed, this method may be called in
  // any thread.
  virtual void RemoveListener(content::SensorsListener* listener) = 0;

  // Broadcasts a change to the coarse screen orientation.
  //
  // This method may be called in any thread.
  virtual void ScreenOrientationChanged(
      content::ScreenOrientation change) = 0;

 protected:
  Provider();
  virtual ~Provider();

  DISALLOW_COPY_AND_ASSIGN(Provider);
};

}  // namespace sensors

#endif  // CONTENT_BROWSER_SENSORS_PROVIDER_H_
