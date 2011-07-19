// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_process_observer.h"

#include "content/renderer/render_thread.h"

RenderProcessObserver::RenderProcessObserver() {
}

RenderProcessObserver::~RenderProcessObserver() {
}

bool RenderProcessObserver::Send(IPC::Message* message) {
  return RenderThread::current()->Send(message);
}

bool RenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  return false;
}

void RenderProcessObserver::OnRenderProcessShutdown() {
}

void RenderProcessObserver::WebKitInitialized() {
}

void RenderProcessObserver::IdleNotification() {
}
