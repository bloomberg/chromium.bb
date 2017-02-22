// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PUSH_MESSAGING_PARAM_TRAITS_H_
#define CONTENT_COMMON_PUSH_MESSAGING_PARAM_TRAITS_H_

#include <stddef.h>

#include "content/common/push_messaging.mojom.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<content::mojom::PushSubscriptionOptionsDataView,
                    content::PushSubscriptionOptions> {
  static bool user_visible_only(const content::PushSubscriptionOptions& r) {
    return r.user_visible_only;
  }
  static const std::string& sender_info(
      const content::PushSubscriptionOptions& r) {
    return r.sender_info;
  }
  static bool Read(content::mojom::PushSubscriptionOptionsDataView data,
                   content::PushSubscriptionOptions* out);
};

template <>
struct EnumTraits<content::mojom::PushRegistrationStatus,
                  content::PushRegistrationStatus> {
  static content::mojom::PushRegistrationStatus ToMojom(
      content::PushRegistrationStatus input);
  static bool FromMojom(content::mojom::PushRegistrationStatus input,
                        content::PushRegistrationStatus* output);
};

template <>
struct EnumTraits<content::mojom::PushErrorType,
                  blink::WebPushError::ErrorType> {
  static content::mojom::PushErrorType ToMojom(
      blink::WebPushError::ErrorType input);
  static bool FromMojom(content::mojom::PushErrorType input,
                        blink::WebPushError::ErrorType* output);
};

template <>
struct EnumTraits<content::mojom::PushGetRegistrationStatus,
                  content::PushGetRegistrationStatus> {
  static content::mojom::PushGetRegistrationStatus ToMojom(
      content::PushGetRegistrationStatus input);
  static bool FromMojom(content::mojom::PushGetRegistrationStatus input,
                        content::PushGetRegistrationStatus* output);
};

template <>
struct EnumTraits<content::mojom::PushPermissionStatus,
                  blink::WebPushPermissionStatus> {
  static content::mojom::PushPermissionStatus ToMojom(
      blink::WebPushPermissionStatus input);
  static bool FromMojom(content::mojom::PushPermissionStatus input,
                        blink::WebPushPermissionStatus* output);
};

}  // namespace mojo

#endif  // CONTENT_COMMON_PUSH_MESSAGING_PARAM_TRAITS_H_
