// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MAC_MOCK_LAUNCHD_H_
#define CHROME_COMMON_MAC_MOCK_LAUNCHD_H_

#include <launch.h>

#include <string>

#include "base/files/file_path.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/mac/launchd.h"
#include "chrome/common/multi_process_lock.h"

class MessageLoop;

// TODO(dmaclach): Write this in terms of a real mock.
// http://crbug.com/76923
class MockLaunchd : public Launchd {
 public:
  static bool MakeABundle(const base::FilePath& dst,
                          const std::string& name,
                          base::FilePath* bundle_root,
                          base::FilePath* executable);

  MockLaunchd(const base::FilePath& file, MessageLoop* loop,
              bool create_socket, bool as_service);
  virtual ~MockLaunchd();

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

  void SignalReady();

  bool restart_called() const { return restart_called_; }
  bool remove_called() const { return remove_called_; }
  bool checkin_called() const { return checkin_called_; }
  bool write_called() const { return write_called_; }
  bool delete_called() const { return delete_called_; }

 private:
  base::FilePath file_;
  std::string pipe_name_;
  MessageLoop* message_loop_;
  scoped_ptr<MultiProcessLock> running_lock_;
  bool create_socket_;
  bool as_service_;
  bool restart_called_;
  bool remove_called_;
  bool checkin_called_;
  bool write_called_;
  bool delete_called_;
};

#endif  // CHROME_COMMON_MAC_MOCK_LAUNCHD_H_
