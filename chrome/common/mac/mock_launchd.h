// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_MOCK_LAUNCHD_H_
#define CHROME_COMMON_MAC_MOCK_LAUNCHD_H_
#pragma once

#include <launch.h>

#include "base/file_path.h"
#include "base/mac/scoped_cftyperef.h"
#include "chrome/common/mac/launchd.h"

class MessageLoop;

// TODO(dmaclach): Write this in terms of a real mock.
// http://crbug.com/76923
class MockLaunchd : public Launchd {
 public:
  static bool MakeABundle(const FilePath& dst,
                          const std::string& name,
                          FilePath* bundle_root,
                          FilePath* executable);

  MockLaunchd(const FilePath& file, MessageLoop* loop)
      : file_(file),
        message_loop_(loop),
        restart_called_(false),
        remove_called_(false),
        checkin_called_(false),
        write_called_(false),
        delete_called_(false) {
  }
  virtual ~MockLaunchd() { }

  virtual CFDictionaryRef CopyExports() OVERRIDE;
  virtual CFDictionaryRef CopyJobDictionary(CFStringRef label) OVERRIDE;
  virtual CFDictionaryRef CopyDictionaryByCheckingIn(CFErrorRef* error)
      OVERRIDE;
  virtual bool RemoveJob(CFStringRef label, CFErrorRef* error) OVERRIDE;
  virtual bool RestartJob(Domain domain,
                          Type type,
                          CFStringRef name,
                          CFStringRef session_type) OVERRIDE;
  virtual CFMutableDictionaryRef CreatePlistFromFile(
      Domain domain,
      Type type,
      CFStringRef name) OVERRIDE;
  virtual bool WritePlistToFile(Domain domain,
                                Type type,
                                CFStringRef name,
                                CFDictionaryRef dict) OVERRIDE;
  virtual bool DeletePlist(Domain domain,
                           Type type,
                           CFStringRef name) OVERRIDE;

  bool restart_called() const { return restart_called_; }
  bool remove_called() const { return remove_called_; }
  bool checkin_called() const { return checkin_called_; }
  bool write_called() const { return write_called_; }
  bool delete_called() const { return delete_called_; }

 private:
  FilePath file_;
  MessageLoop* message_loop_;
  bool restart_called_;
  bool remove_called_;
  bool checkin_called_;
  bool write_called_;
  bool delete_called_;
};

#endif  // CHROME_COMMON_MAC_MOCK_LAUNCHD_H_
