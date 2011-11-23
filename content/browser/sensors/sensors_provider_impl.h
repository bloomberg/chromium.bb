// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SENSORS_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_SENSORS_PROVIDER_IMPL_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "content/browser/sensors/sensors_provider.h"

template <typename T> struct DefaultSingletonTraits;

namespace sensors {

class ProviderImpl : public Provider {
 public:
  static ProviderImpl* GetInstance();

  // Provider implementation
  virtual void AddListener(content::SensorsListener* listener) OVERRIDE;
  virtual void RemoveListener(content::SensorsListener* listener) OVERRIDE;
  virtual void ScreenOrientationChanged(
      content::ScreenOrientation change) OVERRIDE;

 private:
  friend struct DefaultSingletonTraits<ProviderImpl>;

  ProviderImpl();
  virtual ~ProviderImpl();

  typedef ObserverListThreadSafe<content::SensorsListener> ListenerList;
  scoped_refptr<ListenerList> listeners_;

  DISALLOW_COPY_AND_ASSIGN(ProviderImpl);
};

}  // namespace sensors

#endif  // CONTENT_BROWSER_SENSORS_PROVIDER_IMPL_H_
