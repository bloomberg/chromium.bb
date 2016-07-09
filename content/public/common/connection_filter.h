// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CONNECTION_FILTER_H_
#define CONTENT_PUBLIC_COMMON_CONNECTION_FILTER_H_

#include "content/common/content_export.h"

namespace shell {
class Connection;
class Connector;
}

namespace content {

// A ConnectionFilter may be used to conditionally expose interfaces on inbound
// connections accepted by MojoShellConnection. ConnectionFilters are used
// exclusively on the IO thread. See MojoShellConnection::AddConnectionFilter
// for details.
class CONTENT_EXPORT ConnectionFilter {
 public:
  virtual ~ConnectionFilter() {}

  // Called for every new connection accepted. Implementations may add
  // interfaces to |connection|, in which case they should return |true|.
  // |connector| is a corresponding outgoing Connector that may be used by any
  // interfaces added to the connection. Note that references to |connector|
  // must not be retained, but an owned copy may be obtained by calling
  // |connector->Clone()|.
  //
  // If a ConnectionFilter is not interested in an incoming connection, it
  // should return |false|.
  virtual bool OnConnect(shell::Connection* connection,
                         shell::Connector* connector) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CONNECTION_FILTER_H_
