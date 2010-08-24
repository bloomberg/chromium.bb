// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_ORIENTATION_DISPATCHER_HOST_H_
#define CHROME_BROWSER_DEVICE_ORIENTATION_DISPATCHER_HOST_H_

#include <map>

#include "base/ref_counted.h"
#include "chrome/browser/device_orientation/provider.h"

namespace IPC { class Message; }

namespace device_orientation {

class Orientation;

class DispatcherHost : public base::RefCounted<DispatcherHost> {
 public:
  explicit DispatcherHost(int process_id);
  bool OnMessageReceived(const IPC::Message& msg, bool* msg_was_ok);

 private:
  virtual ~DispatcherHost();
  friend class base::RefCounted<DispatcherHost>;

  void OnStartUpdating(int render_view_id);
  void OnStopUpdating(int render_view_id);

  // Helper class that observes a Provider and forwards updates to a RenderView.
  class ObserverDelegate;

  int process_id_;

  // map from render_view_id to ObserverDelegate.
  std::map<int, scoped_refptr<ObserverDelegate> > observers_map_;

  scoped_refptr<Provider> provider_;

  DISALLOW_COPY_AND_ASSIGN(DispatcherHost);
};

}  // namespace device_orientation

#endif  // CHROME_BROWSER_DEVICE_ORIENTATION_DISPATCHER_HOST_H_
