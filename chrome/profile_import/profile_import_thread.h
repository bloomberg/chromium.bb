// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_PROFILE_IMPORT_PROFILE_IMPORT_THREAD_H_
#define CHROME_PROFILE_IMPORT_PROFILE_IMPORT_THREAD_H_

#include "chrome/common/child_thread.h"

// This class represents the background thread where the profile import task
// runs.
class ProfileImportThread : public ChildThread {
 public:
  ProfileImportThread();
  virtual ~ProfileImportThread();

  // Returns the one profile import thread.
  static ProfileImportThread* current() {
    return static_cast<ProfileImportThread*>(ChildThread::current());
  }

 private:
  // IPC messages
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  DISALLOW_COPY_AND_ASSIGN(ProfileImportThread);
};

#endif  // CHROME_PROFILE_IMPORT_PROFILE_IMPORT_THREAD_H_
