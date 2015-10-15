// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CONVERTERS_NETWORK_NETWORK_TYPE_CONVERTERS_H_
#define MOJO_CONVERTERS_NETWORK_NETWORK_TYPE_CONVERTERS_H_

#include <string>

#include "mojo/services/network/public/interfaces/url_loader.mojom.h"

namespace mojo {

template <>
struct TypeConverter<URLRequestPtr, std::string> {
  static URLRequestPtr Convert(const std::string& input);
};

}  // namespace mojo

#endif  // MOJO_CONVERTERS_NETWORK_NETWORK_TYPE_CONVERTERS_H_
