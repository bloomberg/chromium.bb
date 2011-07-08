// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/media_internals/media_internals_proxy.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/media/media_internals.h"
#include "chrome/browser/ui/webui/media_internals/media_internals_handler.h"

MediaInternalsProxy::MediaInternalsProxy() {
  io_thread_ = g_browser_process->io_thread();
}

void MediaInternalsProxy::Attach(MediaInternalsMessageHandler* handler) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  handler_ = handler;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
          &MediaInternalsProxy::ObserveMediaInternalsOnIOThread));
}

void MediaInternalsProxy::Detach() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  handler_ = NULL;
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
          &MediaInternalsProxy::StopObservingMediaInternalsOnIOThread));
}

void MediaInternalsProxy::GetEverything() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &MediaInternalsProxy::GetEverythingOnIOThread));
}

void MediaInternalsProxy::OnUpdate(const string16& update) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(this,
          &MediaInternalsProxy::UpdateUIOnUIThread, update));
}

MediaInternalsProxy::~MediaInternalsProxy() {}

void MediaInternalsProxy::ObserveMediaInternalsOnIOThread() {
  io_thread_->globals()->media.media_internals->AddUI(this);
}

void MediaInternalsProxy::StopObservingMediaInternalsOnIOThread() {
  io_thread_->globals()->media.media_internals->RemoveUI(this);
}

void MediaInternalsProxy::GetEverythingOnIOThread() {
  io_thread_->globals()->media.media_internals->SendEverything();
}

void MediaInternalsProxy::UpdateUIOnUIThread(const string16& update) {
  // Don't forward updates to a destructed UI.
  if (handler_)
    handler_->OnUpdate(update);
}
