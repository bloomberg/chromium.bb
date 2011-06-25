// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GEOLOCATION_WIN7_LOCATION_API_WIN_H_
#define CONTENT_BROWSER_GEOLOCATION_WIN7_LOCATION_API_WIN_H_

#include <atlbase.h>
#include <atlcom.h>
#include <locationapi.h>
#include <sensors.h>
#include <Windows.h>

#include "app/win/scoped_com_initializer.h"
#include "base/time.h"

struct Geoposition;

// PropVariantToDouble
typedef HRESULT (WINAPI* PropVariantToDoubleFunction)
    (REFPROPVARIANT propvarIn, DOUBLE *pdblRet);

class Win7LocationApi {
 public:
  virtual ~Win7LocationApi();
  // Attempts to load propsys.dll, initialise |location_| and requests the user
  // for access to location information. Creates and returns ownership of an
  // instance of Win7LocationApi if all succeed.
  static Win7LocationApi* Create();
  static Win7LocationApi* CreateForTesting(
            PropVariantToDoubleFunction PropVariantToDouble_function,
            ILocation* locator);
  // Gives the best available position.
  // Returns false if no valid position is available.
  virtual void GetPosition(Geoposition* position);
  // Changes the "accuracy" needed. Affects power levels of devices.
  virtual bool SetHighAccuracy(bool acc);

 protected:
  Win7LocationApi();

 private:
  void Init(HINSTANCE prop_library,
            PropVariantToDoubleFunction PropVariantToDouble_function,
            ILocation* locator);

  // Provides the best position fix if one is available.
  // Does this by requesting a location report and querying it to obtain
  // location information.
  virtual bool GetPositionIfFixed(Geoposition* position);

  // Ensure that COM has been initialized for this thread.
  app::win::ScopedCOMInitializer com_initializer_;
  // ILocation object that lets us communicate with the Location and
  // Sensors platform.
  CComPtr<ILocation> locator_;
  // Holds the opened propsys.dll library that is passed on construction.
  // This class is responsible for closing it.
  HINSTANCE prop_lib_;
  PropVariantToDoubleFunction PropVariantToDouble_function_;

  DISALLOW_COPY_AND_ASSIGN(Win7LocationApi);
};

#endif  // CONTENT_BROWSER_GEOLOCATION_WIN7_LOCATION_API_WIN_H_
