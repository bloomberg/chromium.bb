// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_REMOTING_CHROMOTING_HOST_INFO_H_
#define CHROME_COMMON_REMOTING_CHROMOTING_HOST_INFO_H_

#include <string>

namespace remoting {

// This struct is used for ServiceHostMsg_ChromotingHost_Info IPC message.
struct ChromotingHostInfo {
  ChromotingHostInfo();
  ~ChromotingHostInfo();

  std::string host_id;
  std::string hostname;
  std::string public_key;
  std::string login;
  bool enabled;
};

}  // namespace remoting

#endif  // CHROME_COMMON_REMOTING_CHROMOTING_HOST_INFO_H_
