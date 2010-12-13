// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_ORIENTATION_MESSAGE_FILTER_H_
#define CHROME_BROWSER_DEVICE_ORIENTATION_MESSAGE_FILTER_H_

#include <map>

#include "chrome/browser/browser_message_filter.h"
#include "chrome/browser/device_orientation/provider.h"

namespace device_orientation {

class Orientation;

class MessageFilter : public BrowserMessageFilter {
 public:
  MessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok);

 private:
  virtual ~MessageFilter();

  void OnStartUpdating(int render_view_id);
  void OnStopUpdating(int render_view_id);

  // Helper class that observes a Provider and forwards updates to a RenderView.
  class ObserverDelegate;

  // map from render_view_id to ObserverDelegate.
  std::map<int, scoped_refptr<ObserverDelegate> > observers_map_;

  scoped_refptr<Provider> provider_;

  DISALLOW_COPY_AND_ASSIGN(MessageFilter);
};

}  // namespace device_orientation

#endif  // CHROME_BROWSER_DEVICE_ORIENTATION_MESSAGE_FILTER_H_
