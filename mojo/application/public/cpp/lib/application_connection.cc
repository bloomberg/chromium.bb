// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/application/public/cpp/application_connection.h"

#include "base/logging.h"

namespace mojo {

ApplicationConnection::ApplicationConnection() : connection_closed_(false) {
}

void ApplicationConnection::CloseConnection() {
  if (connection_closed_)
    return;
  OnCloseConnection();
  connection_closed_ = true;
  delete this;
}

ApplicationConnection::~ApplicationConnection() {
  // If this DCHECK fails then something has tried to delete this object without
  // calling CloseConnection.
  DCHECK(connection_closed_);
}

}  // namespace mojo
