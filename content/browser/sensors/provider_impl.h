// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SENSORS_PROVIDER_IMPL_H_
#define CONTENT_BROWSER_SENSORS_PROVIDER_IMPL_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/task.h"
#include "content/browser/sensors/provider.h"

template <typename T>
struct DefaultSingletonTraits;

namespace sensors {

class ProviderImpl : public Provider {
 public:
  static ProviderImpl* GetInstance();

  // Provider implementation
  virtual void AddListener(Listener* listener);
  virtual void RemoveListener(Listener* listener);
  virtual void ScreenOrientationChanged(const ScreenOrientation& change);

 private:
  friend struct DefaultSingletonTraits<ProviderImpl>;

  ProviderImpl();
  virtual ~ProviderImpl();

  typedef ObserverListThreadSafe<Listener> ListenerList;
  scoped_refptr<ListenerList> listeners_;

  DISALLOW_COPY_AND_ASSIGN(ProviderImpl);
};

}  // namespace sensors

DISABLE_RUNNABLE_METHOD_REFCOUNT(sensors::ProviderImpl);

#endif  // CONTENT_BROWSER_SENSORS_PROVIDER_IMPL_H_
