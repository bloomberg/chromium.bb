// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SENSORS_SENSORS_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_SENSORS_SENSORS_PROVIDER_IMPL_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "content/public/browser/sensors_provider.h"

template <typename T> struct DefaultSingletonTraits;

namespace content {

class SensorsProviderImpl : public SensorsProvider {
 public:
  static SensorsProviderImpl* GetInstance();

  // SensorsProvider implementation:
  virtual void AddListener(SensorsListener* listener) OVERRIDE;
  virtual void RemoveListener(SensorsListener* listener) OVERRIDE;
  virtual void ScreenOrientationChanged(ScreenOrientation change) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<SensorsProviderImpl>;

  SensorsProviderImpl();
  virtual ~SensorsProviderImpl();

  typedef ObserverListThreadSafe<SensorsListener> ListenerList;
  scoped_refptr<ListenerList> listeners_;

  DISALLOW_COPY_AND_ASSIGN(SensorsProviderImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SENSORS_SENSORS_PROVIDER_IMPL_H_
