// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_ORIENTATION_DISPATCHER_HOST_H_
#define CHROME_BROWSER_DEVICE_ORIENTATION_DISPATCHER_HOST_H_

#include <set>

#include "base/ref_counted.h"
#include "chrome/browser/device_orientation/provider.h"

namespace IPC { class Message; }

namespace device_orientation {

class Orientation;

class DispatcherHost : public base::RefCountedThreadSafe<DispatcherHost>,
                       public Provider::Observer {
 public:
  explicit DispatcherHost(int process_id);
  bool OnMessageReceived(const IPC::Message& msg, bool* msg_was_ok);

  // From Provider::Observer.
  virtual void OnOrientationUpdate(const Orientation& orientation);

 private:
  virtual ~DispatcherHost();
  friend class base::RefCountedThreadSafe<DispatcherHost>;

  void OnStartUpdating(int render_view_id);
  void OnStopUpdating(int render_view_id);

  int process_id_;
  std::set<int> render_view_ids_;
  scoped_refptr<Provider> provider_;

  DISALLOW_COPY_AND_ASSIGN(DispatcherHost);
};

}  // namespace device_orientation

#endif  // CHROME_BROWSER_DEVICE_ORIENTATION_DISPATCHER_HOST_H_
