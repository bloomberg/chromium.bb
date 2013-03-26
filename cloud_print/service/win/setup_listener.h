// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_SETUP_LISTENER_H_
#define CLOUD_PRINT_SERVICE_SETUP_LISTENER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "ipc/ipc_listener.h"

namespace base {
class Thread;
class TimeDelta;
class WaitableEvent;
}  // base

namespace IPC {
class Channel;
}  // IPC

// Simple IPC listener to run on setup utility size wait message with data about
// environment from service process.
class SetupListener : public IPC::Listener {
 public:
  static const char kXpsAvailibleJsonValueName[];
  static const char kChromePathJsonValueName[];
  static const char kPrintersJsonValueName[];
  static const char kUserDataDirJsonValueName[];
  static const char kUserNameJsonValueName[];
  static const wchar_t kSetupPipeName[];

  explicit SetupListener(const string16& user);
  virtual ~SetupListener();

  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  bool WaitResponce(const base::TimeDelta& delta);

  const base::FilePath& chrome_path() const {
    return chrome_path_;
  }

  const base::FilePath& user_data_dir() const {
    return user_data_dir_;
  }

  const string16& user_name() const {
    return user_name_;
  }

  const std::vector<std::string>& printers() const {
    return printers_;
  }

  bool is_xps_availible() const {
    return is_xps_availible_;
  }

 private:
  void Disconnect();
  void Connect(const string16& user);

  base::FilePath chrome_path_;
  base::FilePath user_data_dir_;
  string16 user_name_;
  std::vector<std::string> printers_;
  bool is_xps_availible_;
  bool succeded_;

  scoped_ptr<base::WaitableEvent> done_event_;
  scoped_ptr<base::Thread> ipc_thread_;
  scoped_ptr<IPC::Channel> channel_;
};

#endif  // CLOUD_PRINT_SERVICE_SETUP_LISTENER_H_

