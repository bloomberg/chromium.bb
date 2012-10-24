// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_MESSAGE_FILTER_H_

#include <map>

#include "content/browser/device_orientation/device_data.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

// Helper class that observes a Provider and forwards updates to a RenderView.
class ObserverDelegate;

class Provider;

class DeviceOrientationMessageFilter : public BrowserMessageFilter {
 public:
  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE = 0;

 protected:
  DeviceOrientationMessageFilter(DeviceData::Type device_data_type);
  virtual ~DeviceOrientationMessageFilter();

  void OnStartUpdating(int render_view_id);
  void OnStopUpdating(int render_view_id);

 private:

  // map from render_view_id to ObserverDelegate.
  std::map<int, scoped_refptr<ObserverDelegate> > observers_map_;

  scoped_refptr<Provider> provider_;
  DeviceData::Type device_data_type_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_MESSAGE_FILTER_H_
