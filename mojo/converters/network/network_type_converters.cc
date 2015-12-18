// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/converters/network/network_type_converters.h"

namespace mojo {

// static
URLRequestPtr TypeConverter<URLRequestPtr, std::string>::Convert(
    const std::string& input) {
  URLRequestPtr result(URLRequest::New());
  result->url = input;
  return result;
}

}  // namespace mojo
