// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TYPE_CONVERTERS_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TYPE_CONVERTERS_H_

#include "device/fido/u2f_transport_protocol.h"
#include "mojo/public/cpp/bindings/type_converter.h"
#include "third_party/WebKit/public/platform/modules/webauth/authenticator.mojom.h"

namespace mojo {

template <>
struct TypeConverter<::device::U2fTransportProtocol,
                     ::webauth::mojom::AuthenticatorTransport> {
  static ::device::U2fTransportProtocol Convert(
      const ::webauth::mojom::AuthenticatorTransport& input);
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TYPE_CONVERTERS_H_
