// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/message_filter.h"

#include "content/browser/device_orientation/observer_delegate.h"
#include "content/browser/device_orientation/provider.h"
#include "content/public/browser/browser_thread.h"

namespace content {

DeviceOrientationMessageFilter::DeviceOrientationMessageFilter(
    DeviceData::Type device_data_type)
    : provider_(NULL),
      device_data_type_(device_data_type) {
}

DeviceOrientationMessageFilter::~DeviceOrientationMessageFilter() {
}

void DeviceOrientationMessageFilter::OnStartUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!provider_)
    provider_ = Provider::GetInstance();

  observers_map_[render_view_id] = new ObserverDelegate(device_data_type_,
      provider_, render_view_id, this);
}

void DeviceOrientationMessageFilter::OnStopUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  observers_map_.erase(render_view_id);
}

}  // namespace content
