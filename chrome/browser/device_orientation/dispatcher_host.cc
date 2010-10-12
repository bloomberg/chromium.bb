// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_orientation/dispatcher_host.h"

#include "base/scoped_ptr.h"
#include "chrome/browser/browser_thread.h"
#include "chrome/browser/device_orientation/orientation.h"
#include "chrome/browser/device_orientation/provider.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_view_host_notification_task.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "ipc/ipc_message.h"

namespace device_orientation {

DispatcherHost::DispatcherHost(int process_id)
    : process_id_(process_id),
      observers_map_(),
      provider_(NULL) {
}

DispatcherHost::~DispatcherHost() {
}

class DispatcherHost::ObserverDelegate
    : public base::RefCounted<ObserverDelegate>, public Provider::Observer {
 public:
    // Create ObserverDelegate that observes provider and forwards updates to
    // render_view_id in process_id.
    // Will stop observing provider when destructed.
    ObserverDelegate(Provider* provider,
                     int process_id,
                     int render_view_id);

    // From Provider::Observer.
    virtual void OnOrientationUpdate(const Orientation& orientation);

 private:
  friend class base::RefCounted<ObserverDelegate>;
  virtual ~ObserverDelegate();

  scoped_refptr<Provider> provider_;
  int process_id_;
  int render_view_id_;

  DISALLOW_COPY_AND_ASSIGN(ObserverDelegate);
};

DispatcherHost::ObserverDelegate::ObserverDelegate(Provider* provider,
                                                   int process_id,
                                                   int render_view_id)
    : provider_(provider),
      process_id_(process_id),
      render_view_id_(render_view_id) {
  provider_->AddObserver(this);
}

DispatcherHost::ObserverDelegate::~ObserverDelegate() {
  provider_->RemoveObserver(this);
}

void DispatcherHost::ObserverDelegate::OnOrientationUpdate(
    const Orientation& orientation) {
  ViewMsg_DeviceOrientationUpdated_Params params;
  params.can_provide_alpha = orientation.can_provide_alpha_;
  params.alpha = orientation.alpha_;
  params.can_provide_beta = orientation.can_provide_beta_;
  params.beta = orientation.beta_;
  params.can_provide_gamma = orientation.can_provide_gamma_;
  params.gamma = orientation.gamma_;

  IPC::Message* message = new ViewMsg_DeviceOrientationUpdated(render_view_id_,
                                                               params);
  CallRenderViewHost(process_id_, render_view_id_, &RenderViewHost::Send,
                     message);
}

bool DispatcherHost::OnMessageReceived(const IPC::Message& msg,
                                       bool* msg_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
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

void DispatcherHost::OnStartUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!provider_)
    provider_ = Provider::GetInstance();

  observers_map_[render_view_id] = new ObserverDelegate(provider_,
                                                        process_id_,
                                                        render_view_id);
}

void DispatcherHost::OnStopUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  observers_map_.erase(render_view_id);
}

}  // namespace device_orientation
