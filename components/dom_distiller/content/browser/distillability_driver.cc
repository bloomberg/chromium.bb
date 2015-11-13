// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/dom_distiller/content/browser/distillability_driver.h"
#include "components/dom_distiller/content/common/distiller_messages.h"

#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(
    dom_distiller::DistillabilityDriver);

namespace dom_distiller {

DistillabilityDriver::DistillabilityDriver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
}

DistillabilityDriver::~DistillabilityDriver() {
  CleanUp();
}

void DistillabilityDriver::SetDelegate(
    const base::Callback<void(bool, bool)>& delegate) {
  m_delegate_ = delegate;
}

bool DistillabilityDriver::OnMessageReceived(const IPC::Message &msg,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DistillabilityDriver, msg)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Distillability,
                        OnDistillability)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DistillabilityDriver::OnDistillability(
    bool distillable, bool is_last) {
  if (m_delegate_.is_null()) return;

  m_delegate_.Run(distillable, is_last);
}

void DistillabilityDriver::RenderProcessGone(
    base::TerminationStatus status) {
  CleanUp();
}

void DistillabilityDriver::CleanUp() {
  content::WebContentsObserver::Observe(NULL);
}

}  // namespace dom_distiller
