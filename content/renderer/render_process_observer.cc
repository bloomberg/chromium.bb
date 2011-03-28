// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_process_observer.h"

RenderProcessObserver::~RenderProcessObserver() {
}

bool RenderProcessObserver::OnControlMessageReceived(
    const IPC::Message& message) {
  return false;
}

void RenderProcessObserver::OnRenderProcessShutdown() {
}
