// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/network/net_adapters.h"

#include "net/base/net_errors.h"

namespace mojo {

NetworkErrorPtr MakeNetworkError(int error_code) {
  NetworkErrorPtr error = NetworkError::New();
  error->code = error_code;
  if (error_code <= 0)
    error->description = net::ErrorToString(error_code);
  return error.Pass();
}

}  // namespace mojo
