// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_PERFTEST_SUPPORT_H_
#define IPC_IPC_PERFTEST_SUPPORT_H_

#include <vector>

#include "ipc/ipc_test_base.h"

namespace IPC {
namespace test {

class ChannelReflectorListener;

class PingPongTestParams {
 public:
  PingPongTestParams(size_t size, int count)
      : message_size_(size), message_count_(count) {
  }

  size_t message_size() const { return message_size_; }
  int message_count() const { return message_count_; }

 private:
  size_t message_size_;
  int message_count_;
};

class IPCChannelPerfTestBase : public IPCTestBase {
 public:
  static std::vector<PingPongTestParams> GetDefaultTestParams();

  void RunTestChannelPingPong(
      const std::vector<PingPongTestParams>& params_list);
  void RunTestChannelProxyPingPong(
      const std::vector<PingPongTestParams>& params_list);
};

class PingPongTestClient {
 public:
  PingPongTestClient();
  virtual ~PingPongTestClient();

  virtual scoped_ptr<Channel> CreateChannel(Listener* listener);
  int RunMain();
  scoped_refptr<base::TaskRunner> task_runner();

 private:
  base::MessageLoopForIO main_message_loop_;
  scoped_ptr<ChannelReflectorListener> listener_;
  scoped_ptr<Channel> channel_;
};

}
}

#endif  // IPC_IPC_PERFTEST_SUPPORT_H_
