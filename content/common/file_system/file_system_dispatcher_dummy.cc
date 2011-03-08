// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/file_system/file_system_dispatcher.h"

#include "base/compiler_specific.h"

// FileSystemDispatcher --------------------------------------------------------

FileSystemDispatcher::FileSystemDispatcher() {
}

FileSystemDispatcher::~FileSystemDispatcher() {
}

// FileSystemDispatcher implementation -----------------------------------------

bool FileSystemDispatcher::OnMessageReceived(const IPC::Message& message) {
  return false;
}
