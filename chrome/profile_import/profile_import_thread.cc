// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/profile_import/profile_import_thread.h"

#include "chrome/common/child_process.h"

ProfileImportThread::ProfileImportThread() {
  ChildProcess::current()->AddRefProcess();
}

ProfileImportThread::~ProfileImportThread() {
}

void ProfileImportThread::OnControlMessageReceived(const IPC::Message& msg) {
  ChildProcess::current()->ReleaseProcess();
  NOTIMPLEMENTED();
}
