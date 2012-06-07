// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_error_params.h"

#include "base/values.h"

namespace net {

SocketErrorParams::SocketErrorParams(int net_error, int os_error)
    : net_error_(net_error),
      os_error_(os_error) {
}

Value* SocketErrorParams::ToValue() const {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetInteger("net_error", net_error_);
  dict->SetInteger("os_error", os_error_);
  return dict;
}

SocketErrorParams::~SocketErrorParams() {}

}  // namespace net
