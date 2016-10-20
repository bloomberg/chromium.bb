// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "content/common/content_export.h"
#include "ipc/message_filter.h"

namespace content {

// MessageFilter that notifies a (series of) Delegates of incoming video capture
// connections. VideoCaptureMessageFilter is operated on IO thread of render
// process.
class CONTENT_EXPORT VideoCaptureMessageFilter : public IPC::MessageFilter {
 public:
  class CONTENT_EXPORT Delegate {
   public:
    // Called when a delegate has been added to the filter's delegate list.
    // |device_id| is the device id for the delegate.
    virtual void OnDelegateAdded(int32_t device_id) = 0;

   protected:
    virtual ~Delegate() {}
  };

  VideoCaptureMessageFilter();

  // Add a delegate to the map.
  void AddDelegate(Delegate* delegate);
  // Remove a delegate from the map.
  void RemoveDelegate(Delegate* delegate);

  IPC::Channel* channel() const { return channel_; }

  // IPC::MessageFilter override. Called on IO thread.
  bool OnMessageReceived(const IPC::Message& message) override;
  void OnFilterAdded(IPC::Channel* channel) override;
  void OnFilterRemoved() override;
  void OnChannelClosing() override;

  void DoNothing() const {}

 protected:
  ~VideoCaptureMessageFilter() override;

 private:
  // A map of device ids to delegates.
  using Delegates = std::map<int32_t, Delegate*>;
  Delegates delegates_;
  Delegates pending_delegates_;

  int32_t last_device_id_;

  IPC::Channel* channel_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_VIDEO_CAPTURE_MESSAGE_FILTER_H_
