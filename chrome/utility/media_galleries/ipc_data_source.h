// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_MEDIA_GALLERIES_IPC_DATA_SOURCE_H_
#define CHROME_UTILITY_MEDIA_GALLERIES_IPC_DATA_SOURCE_H_

#include <map>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "chrome/utility/utility_message_handler.h"
#include "media/base/data_source.h"

namespace base {
class TaskRunner;
}

namespace metadata {

// Provides the metadata parser with bytes from the browser process via IPC.
// Class must be created and destroyed on the utility thread. Class may be used
// as a DataSource on a different thread. The utility thread must not be blocked
// for read operations to succeed.
class IPCDataSource: public media::DataSource,
                     public UtilityMessageHandler {
 public:
  // May only be called on the utility thread.
  explicit IPCDataSource(int64 total_size);
  virtual ~IPCDataSource();

  // Implementation of DataSource. These methods may be called on any single
  // thread. First usage of these methods attaches a thread checker.
  virtual void Stop(const base::Closure& callback) OVERRIDE;
  virtual void Read(int64 position, int size, uint8* data,
                    const ReadCB& read_cb) OVERRIDE;
  virtual bool GetSize(int64* size_out) OVERRIDE;
  virtual bool IsStreaming() OVERRIDE;
  virtual void SetBitrate(int bitrate) OVERRIDE;

  // Implementation of UtilityMessageHandler. May only be called on the utility
  // thread.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  struct Request {
    Request();
    ~Request();
    uint8* destination;
    ReadCB callback;
  };

  void ReadOnUtilityThread(int64 position, int size, uint8* data,
                           const ReadCB& read_cb);

  void OnRequestBlobBytesFinished(int64 request_id,
                                  const std::string& bytes);

  const int64 total_size_;

  scoped_refptr<base::TaskRunner> utility_task_runner_;
  std::map<int64, Request> pending_requests_;
  int64 next_request_id_;

  base::ThreadChecker utility_thread_checker_;

  // Enforces that the DataSource methods are called on one other thread only.
  base::ThreadChecker data_source_thread_checker_;
};

}  // namespace metadata

#endif  // CHROME_UTILITY_MEDIA_GALLERIES_IPC_DATA_SOURCE_H_
