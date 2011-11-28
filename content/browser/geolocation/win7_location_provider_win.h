// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIN7_LOCATION_PROVIDER_WIN_H_
#define CONTENT_BROWSER_GEOLOCATION_WIN7_LOCATION_PROVIDER_WIN_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task.h"
#include "content/browser/geolocation/location_provider.h"
#include "content/browser/geolocation/win7_location_api_win.h"
#include "content/common/content_export.h"
#include "content/common/geoposition.h"

// Location provider for Windows 7 that uses the Location and Sensors platform
// to obtain position fixes.
// TODO(allanwoj): Remove polling and let the api signal when a new location.
// TODO(allanwoj): Possibly derive this class and the linux gps provider class
// from a single SystemLocationProvider class as their implementation is very
// similar.
class CONTENT_EXPORT Win7LocationProvider : public LocationProviderBase {
 public:
  Win7LocationProvider(Win7LocationApi* api);
  virtual ~Win7LocationProvider();

  // LocationProvider.
  virtual bool StartProvider(bool high_accuracy) OVERRIDE;
  virtual void StopProvider() OVERRIDE;
  virtual void GetPosition(Geoposition* position) OVERRIDE;
  virtual void UpdatePosition() OVERRIDE;
  virtual void OnPermissionGranted(const GURL& requesting_frame) OVERRIDE;

 private:
  // Task which runs in the child thread.
  void DoPollTask();
  // Will schedule a poll; i.e. enqueue DoPollTask deferred task.
  void ScheduleNextPoll(int interval);

  scoped_ptr<Win7LocationApi> api_;
  Geoposition position_;
  // Holder for the tasks which run on the thread; takes care of cleanup.
  base::WeakPtrFactory<Win7LocationProvider> weak_factory_;
};

#endif  // CONTENT_BROWSER_GEOLOCATION_WIN7_LOCATION_PROVIDER_WIN_H_
