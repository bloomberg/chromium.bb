// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_OBSERVER_DELEGATE_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_OBSERVER_DELEGATE_H_

#include "content/browser/device_orientation/device_data.h"
#include "content/browser/device_orientation/provider.h"

namespace IPC {
class Sender;
}

namespace content {

class ObserverDelegate
    : public base::RefCounted<ObserverDelegate>, public Provider::Observer {
 public:
  // Create ObserverDelegate that observes provider and forwards updates to
  // render_view_id.
  // Will stop observing provider when destructed.
  ObserverDelegate(DeviceData::Type device_data_type, Provider* provider,
                   int render_view_id, IPC::Sender* sender);

  // From Provider::Observer.
  virtual void OnDeviceDataUpdate(const DeviceData* device_data,
                                  DeviceData::Type device_data_type) OVERRIDE;

 private:
  static DeviceData* EmptyDeviceData(DeviceData::Type type);

  friend class base::RefCounted<ObserverDelegate>;
  virtual ~ObserverDelegate();

  scoped_refptr<Provider> provider_;
  int render_view_id_;
  IPC::Sender* sender_;  // Weak pointer.

  DISALLOW_COPY_AND_ASSIGN(ObserverDelegate);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_OBSERVER_DELEGATE_H_
