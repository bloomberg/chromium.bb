// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SERVICE_MESSAGES_H_
#define CHROME_COMMON_SERVICE_MESSAGES_H_

#include "chrome/common/service_messages_internal.h"

namespace remoting {
struct ChromotingHostInfo;
}  // namespace remoting

namespace IPC {

template <>
struct ParamTraits<remoting::ChromotingHostInfo> {
  typedef remoting::ChromotingHostInfo param_type;
  static void Write(Message* m, const param_type& p);
  static bool Read(const Message* m, void** iter, param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_SERVICE_MESSAGES_H_
