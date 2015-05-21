// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CRNET_CRNET_NET_LOG_H_
#define IOS_CRNET_CRNET_NET_LOG_H_

#include "base/memory/scoped_ptr.h"
#include "base/threading/thread_checker.h"
#include "net/log/net_log.h"

namespace base {
class FilePath;
class DictionaryValue;
}

namespace net {
class WriteToFileNetLogObserver;
}

// CrNet-specific NetLog subclass.
// This class can be used as a NetLog where needed; it logs all log entries to
// the file specified in Start().
class CrNetNetLog : public net::NetLog {
 public:
  enum Mode {
    LOG_ALL_BYTES,
    LOG_STRIP_PRIVATE_DATA,
  };

  CrNetNetLog();
  ~CrNetNetLog() override;

  // Starts logging to the file named |log_file|. If |mode| is LOG_ALL_BYTES,
  // logs all network traffic, including raw bytes. If |mode| is
  // LOG_STRIP_PRIVATE_DATA, strips cookies and other private data, and does
  // not log raw bytes.
  bool Start(const base::FilePath& log_file, Mode mode);

  // Stops logging.
  void Stop();

 private:
  // Underlying file writer. This observer observes NetLog events and writes
  // them back to the file this class is logging to.
  scoped_ptr<net::WriteToFileNetLogObserver> write_to_file_observer_;

  base::ThreadChecker thread_checker_;
};

#endif  // IOS_CRNET_CRNET_NET_LOG_H
