// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_connection.h"

#include "base/macros.h"
#include "blimp/net/packet_reader.h"
#include "blimp/net/packet_writer.h"

namespace blimp {

BlimpConnection::BlimpConnection(scoped_ptr<PacketReader> reader,
                                 scoped_ptr<PacketWriter> writer)
    : reader_(reader.Pass()), writer_(writer.Pass()) {
  DCHECK(reader_);
  DCHECK(writer_);
}

BlimpConnection::~BlimpConnection() {}

}  // namespace blimp
