// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/message_filter.h"

#include "base/memory/scoped_ptr.h"
#include "content/browser/device_orientation/orientation.h"
#include "content/browser/device_orientation/provider.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/device_orientation_messages.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace device_orientation {

MessageFilter::MessageFilter() : provider_(NULL) {
}

MessageFilter::~MessageFilter() {
}

class MessageFilter::ObserverDelegate
    : public base::RefCounted<ObserverDelegate>, public Provider::Observer {
 public:
    // Create ObserverDelegate that observes provider and forwards updates to
    // render_view_id in process_id.
    // Will stop observing provider when destructed.
    ObserverDelegate(Provider* provider,
                     int render_view_id,
                     IPC::Message::Sender* sender);

    // From Provider::Observer.
    virtual void OnOrientationUpdate(const Orientation& orientation);

 private:
  friend class base::RefCounted<ObserverDelegate>;
  virtual ~ObserverDelegate();

  scoped_refptr<Provider> provider_;
  int render_view_id_;
  IPC::Message::Sender* sender_;  // Weak pointer.

  DISALLOW_COPY_AND_ASSIGN(ObserverDelegate);
};

MessageFilter::ObserverDelegate::ObserverDelegate(Provider* provider,
                                                  int render_view_id,
                                                  IPC::Message::Sender* sender)
    : provider_(provider),
      render_view_id_(render_view_id),
      sender_(sender) {
  provider_->AddObserver(this);
}

MessageFilter::ObserverDelegate::~ObserverDelegate() {
  provider_->RemoveObserver(this);
}

void MessageFilter::ObserverDelegate::OnOrientationUpdate(
    const Orientation& orientation) {
  DeviceOrientationMsg_Updated_Params params;
  params.can_provide_alpha = orientation.can_provide_alpha_;
  params.alpha = orientation.alpha_;
  params.can_provide_beta = orientation.can_provide_beta_;
  params.beta = orientation.beta_;
  params.can_provide_gamma = orientation.can_provide_gamma_;
  params.gamma = orientation.gamma_;

  sender_->Send(new DeviceOrientationMsg_Updated(render_view_id_, params));
}

bool MessageFilter::OnMessageReceived(const IPC::Message& message,
                                      bool* message_was_ok) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(MessageFilter, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(DeviceOrientationHostMsg_StartUpdating, OnStartUpdating)
    IPC_MESSAGE_HANDLER(DeviceOrientationHostMsg_StopUpdating, OnStopUpdating)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void MessageFilter::OnStartUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (!provider_)
    provider_ = Provider::GetInstance();

  observers_map_[render_view_id] = new ObserverDelegate(provider_,
                                                        render_view_id,
                                                        this);
}

void MessageFilter::OnStopUpdating(int render_view_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  observers_map_.erase(render_view_id);
}

}  // namespace device_orientation
