// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/desktop_capture_device_aura.h"

#include "base/logging.h"
#include "base/timer/timer.h"
#include "content/browser/media/capture/aura_window_capture_machine.h"
#include "content/browser/media/capture/content_video_capture_device_core.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/window.h"

namespace content {

namespace {

void SetCaptureSource(AuraWindowCaptureMachine* machine,
                      const DesktopMediaID& source) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  aura::Window* window = DesktopMediaID::GetAuraWindowById(source);
  machine->SetWindow(window);
}

}  // namespace

DesktopCaptureDeviceAura::DesktopCaptureDeviceAura(
    const DesktopMediaID& source) {
  AuraWindowCaptureMachine* machine = new AuraWindowCaptureMachine();
  core_.reset(new ContentVideoCaptureDeviceCore(make_scoped_ptr(machine)));
  // |core_| owns |machine| and deletes it on UI thread so passing the raw
  // pointer to the UI thread is safe here.
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(&SetCaptureSource, machine, source));
}

DesktopCaptureDeviceAura::~DesktopCaptureDeviceAura() {
  DVLOG(2) << "DesktopCaptureDeviceAura@" << this << " destroying.";
}

// static
media::VideoCaptureDevice* DesktopCaptureDeviceAura::Create(
    const DesktopMediaID& source) {
  return new DesktopCaptureDeviceAura(source);
}

void DesktopCaptureDeviceAura::AllocateAndStart(
    const media::VideoCaptureParams& params,
    scoped_ptr<Client> client) {
  DVLOG(1) << "Allocating " << params.requested_format.frame_size.ToString();
  core_->AllocateAndStart(params, client.Pass());
}

void DesktopCaptureDeviceAura::StopAndDeAllocate() {
  core_->StopAndDeAllocate();
}

}  // namespace content
