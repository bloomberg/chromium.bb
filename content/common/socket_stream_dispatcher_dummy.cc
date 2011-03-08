// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/socket_stream_dispatcher.h"

#include "base/compiler_specific.h"

// SocketStreamDispatcher ------------------------------------------------------

SocketStreamDispatcher::SocketStreamDispatcher() {
}

// SocketStreamDispatcher implementation ---------------------------------------

bool SocketStreamDispatcher::OnMessageReceived(const IPC::Message& message) {
  return false;
}
