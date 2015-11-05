// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_BLIMP_CONNECTION_H_
#define BLIMP_NET_BLIMP_CONNECTION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/net/blimp_net_export.h"

namespace blimp {

class PacketReader;
class PacketWriter;

class BLIMP_NET_EXPORT BlimpConnection {
 public:
  virtual ~BlimpConnection();

 protected:
  BlimpConnection(scoped_ptr<PacketReader> reader,
                  scoped_ptr<PacketWriter> writer);

 private:
  scoped_ptr<PacketReader> reader_;
  scoped_ptr<PacketWriter> writer_;
};

}  // namespace blimp

#endif  // BLIMP_NET_BLIMP_CONNECTION_H_
