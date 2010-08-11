// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_orientation/dispatcher_host.h"

#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/device_orientation/orientation.h"
#include "chrome/browser/device_orientation/provider.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/render_messages.h"
#include "ipc/ipc_message.h"

namespace device_orientation {

DispatcherHost::DispatcherHost(int process_id)
    : process_id_(process_id) {
}

DispatcherHost::~DispatcherHost() {
  if (provider_)
    provider_->RemoveObserver(this);
}

bool DispatcherHost::OnMessageReceived(const IPC::Message& msg,
                                       bool* msg_was_ok) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(DispatcherHost, msg, *msg_was_ok)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DeviceOrientation_StartUpdating,
                        OnStartUpdating)
    IPC_MESSAGE_HANDLER(ViewHostMsg_DeviceOrientation_StopUpdating,
                        OnStopUpdating)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DispatcherHost::OnOrientationUpdate(const Orientation& orientation) {
  ViewMsg_DeviceOrientationUpdated_Params params;
  params.can_provide_alpha = orientation.can_provide_alpha_;
  params.alpha = orientation.alpha_;
  params.can_provide_beta = orientation.can_provide_beta_;
  params.beta = orientation.beta_;
  params.can_provide_gamma = orientation.can_provide_gamma_;
  params.gamma = orientation.gamma_;

  typedef std::set<int>::const_iterator Iterator;
  for (Iterator i = render_view_ids_.begin(), e = render_view_ids_.end();
       i != e; ++i) {
    IPC::Message* message = new ViewMsg_DeviceOrientationUpdated(*i, params);
    CallRenderViewHost(process_id_, *i, &RenderViewHost::Send, message);
  }
}

void DispatcherHost::OnStartUpdating(int render_view_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  render_view_ids_.insert(render_view_id);
  if (render_view_ids_.size() == 1) {
    DCHECK(!provider_);
    provider_ = Provider::GetInstance();
    provider_->AddObserver(this);
  }
}

void DispatcherHost::OnStopUpdating(int render_view_id) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));

  render_view_ids_.erase(render_view_id);
  if (render_view_ids_.empty()) {
    provider_->RemoveObserver(this);
    provider_ = NULL;
  }
}

}  // namespace device_orientation
