// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/next_proto.h"

namespace net {

NextProtoVector NextProtosDefaults() {
  NextProtoVector next_protos;
  next_protos.push_back(kProtoHTTP2);
  next_protos.push_back(kProtoSPDY31);
  next_protos.push_back(kProtoHTTP11);
  return next_protos;
}

NextProtoVector NextProtosWithSpdyAndQuic(bool spdy_enabled,
                                          bool quic_enabled) {
  NextProtoVector next_protos;
  if (quic_enabled)
    next_protos.push_back(kProtoQUIC1SPDY3);
  if (spdy_enabled) {
    next_protos.push_back(kProtoHTTP2);
    next_protos.push_back(kProtoSPDY31);
  }
  next_protos.push_back(kProtoHTTP11);
  return next_protos;
}

NextProtoVector NextProtosSpdy31() {
  NextProtoVector next_protos;
  next_protos.push_back(kProtoQUIC1SPDY3);
  next_protos.push_back(kProtoSPDY31);
  next_protos.push_back(kProtoHTTP11);
  return next_protos;
}

bool NextProtoIsSPDY(NextProto next_proto) {
  return next_proto >= kProtoSPDYMinimumVersion &&
         next_proto <= kProtoSPDYMaximumVersion;
}

void DisableHTTP2(NextProtoVector* next_protos) {
  for (NextProtoVector::iterator it = next_protos->begin();
       it != next_protos->end();) {
    if (*it == kProtoHTTP2) {
      it = next_protos->erase(it);
      continue;
    }
    ++it;
  }
}

}  // namespace net
