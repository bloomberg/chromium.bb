// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUARTC_QUARTC_SESSION_VISITOR_INTERFACE_H_
#define NET_QUIC_QUARTC_QUARTC_SESSION_VISITOR_INTERFACE_H_

#include "net/quic/core/quic_connection.h"
#include "net/quic/platform/api/quic_export.h"

namespace net {

class QUIC_EXPORT_PRIVATE QuartcSessionVisitor {
 public:
  virtual ~QuartcSessionVisitor() {}

  // Sets the |QuicConnection| for this debug visitor.  Called before
  // |GetConnectionVisitor|.
  virtual void SetQuicConnection(QuicConnection* connection) = 0;

  // Gets the |QuicConnectionDebugVisitor| associated with this Quartc visitor.
  virtual QuicConnectionDebugVisitor* GetConnectionVisitor() const = 0;
};

}  // namespace net

#endif  // NET_QUIC_QUARTC_QUARTC_SESSION_VISITOR_INTERFACE_H_
