// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares a CoreLocation data provider class that allows the
// CoreLocation framework to run on the UI thread, since the Geolocation API's
// providers all live on the IO thread

#ifndef CONTENT_BROWSER_GEOLOCATION_CORE_LOCATION_DATA_PROVIDER_H_
#define CONTENT_BROWSER_GEOLOCATION_CORE_LOCATION_DATA_PROVIDER_H_
#pragma once

#include "base/ref_counted.h"
#include "base/scoped_nsobject.h"
#include "chrome/common/geoposition.h"
#include "content/browser/browser_thread.h"

#import <Foundation/Foundation.h>

class CoreLocationProviderMac;
@class CoreLocationWrapperMac;

// Data provider class that allows CoreLocation to run in Chrome's UI thread
// while existing on any of Chrome's threads (in this case the IO thread)
class CoreLocationDataProviderMac
    : public base::RefCountedThreadSafe<CoreLocationDataProviderMac> {
 public:
  CoreLocationDataProviderMac();

  bool StartUpdating(CoreLocationProviderMac* provider);
  void StopUpdating();

  void UpdatePosition(Geoposition *position);

 protected:
  friend class base::RefCountedThreadSafe<CoreLocationDataProviderMac>;
  ~CoreLocationDataProviderMac();

 private:
  // These must execute in BrowserThread::UI
  void StartUpdatingTask();
  void StopUpdatingTask();
  // This must execute in the origin thread (IO thread)
  void PositionUpdated(Geoposition position);

  // The wrapper class that supplies this class with position data
  scoped_nsobject<CoreLocationWrapperMac> wrapper_;
  // The LocationProviderBase class that should receive position data
  CoreLocationProviderMac* provider_;
};

#endif  // CONTENT_BROWSER_GEOLOCATION_CORE_LOCATION_DATA_PROVIDER_H_
