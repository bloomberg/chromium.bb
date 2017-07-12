// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/push_messaging_param_traits.h"

#include "content/public/common/push_messaging_status.mojom.h"

namespace mojo {

// PushErrorType
static_assert(blink::WebPushError::ErrorType::kErrorTypeAbort ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::ABORT),
              "PushErrorType enums must match, ABORT");

static_assert(blink::WebPushError::ErrorType::kErrorTypeNetwork ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::NETWORK),
              "PushErrorType enums must match, NETWORK");

static_assert(blink::WebPushError::ErrorType::kErrorTypeNone ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::NONE),
              "PushErrorType enums must match, NONE");

static_assert(blink::WebPushError::ErrorType::kErrorTypeNotAllowed ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::NOT_ALLOWED),
              "PushErrorType enums must match, NOT_ALLOWED");

static_assert(blink::WebPushError::ErrorType::kErrorTypeNotFound ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::NOT_FOUND),
              "PushErrorType enums must match, NOT_FOUND");

static_assert(blink::WebPushError::ErrorType::kErrorTypeNotSupported ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::NOT_SUPPORTED),
              "PushErrorType enums must match, NOT_SUPPORTED");

static_assert(blink::WebPushError::ErrorType::kErrorTypeInvalidState ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::INVALID_STATE),
              "PushErrorType enums must match, INVALID_STATE");

static_assert(blink::WebPushError::ErrorType::kErrorTypeLast ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::LAST),
              "PushErrorType enums must match, LAST");

// PushPermissionStatus
static_assert(blink::WebPushPermissionStatus::kWebPushPermissionStatusGranted ==
                  static_cast<blink::WebPushPermissionStatus>(
                      content::mojom::PushPermissionStatus::GRANTED),
              "PushPermissionStatus enums must match, GRANTED");

static_assert(blink::WebPushPermissionStatus::kWebPushPermissionStatusDenied ==
                  static_cast<blink::WebPushPermissionStatus>(
                      content::mojom::PushPermissionStatus::DENIED),
              "PushPermissionStatus enums must match, DENIED");

static_assert(blink::WebPushPermissionStatus::kWebPushPermissionStatusPrompt ==
                  static_cast<blink::WebPushPermissionStatus>(
                      content::mojom::PushPermissionStatus::PROMPT),
              "PushPermissionStatus enums must match, PROMPT");

static_assert(blink::WebPushPermissionStatus::kWebPushPermissionStatusLast ==
                  static_cast<blink::WebPushPermissionStatus>(
                      content::mojom::PushPermissionStatus::LAST),
              "PushPermissionStatus enums must match, LAST");

// static
bool StructTraits<content::mojom::PushSubscriptionOptionsDataView,
                  content::PushSubscriptionOptions>::
    Read(content::mojom::PushSubscriptionOptionsDataView data,
         content::PushSubscriptionOptions* out) {
  out->user_visible_only = data.user_visible_only();
  if (!data.ReadSenderInfo(&out->sender_info)) {
    return false;
  }
  return true;
}

// static
content::mojom::PushErrorType
EnumTraits<content::mojom::PushErrorType, blink::WebPushError::ErrorType>::
    ToMojom(blink::WebPushError::ErrorType input) {
  if (input >= blink::WebPushError::ErrorType::kErrorTypeAbort &&
      input <= blink::WebPushError::ErrorType::kErrorTypeInvalidState) {
    return static_cast<content::mojom::PushErrorType>(input);
  }

  NOTREACHED();
  return content::mojom::PushErrorType::ABORT;
}

// static
bool EnumTraits<content::mojom::PushErrorType, blink::WebPushError::ErrorType>::
    FromMojom(content::mojom::PushErrorType input,
              blink::WebPushError::ErrorType* output) {
  if (input >= content::mojom::PushErrorType::ABORT &&
      input <= content::mojom::PushErrorType::INVALID_STATE) {
    *output = static_cast<blink::WebPushError::ErrorType>(input);
    return true;
  }

  NOTREACHED();
  return false;
}

// static
content::mojom::PushPermissionStatus EnumTraits<
    content::mojom::PushPermissionStatus,
    blink::WebPushPermissionStatus>::ToMojom(blink::WebPushPermissionStatus
                                                 input) {
  if (input >=
          blink::WebPushPermissionStatus::kWebPushPermissionStatusGranted &&
      input <= blink::WebPushPermissionStatus::kWebPushPermissionStatusLast) {
    return static_cast<content::mojom::PushPermissionStatus>(input);
  }

  NOTREACHED();
  return content::mojom::PushPermissionStatus::DENIED;
}

// static
bool EnumTraits<content::mojom::PushPermissionStatus,
                blink::WebPushPermissionStatus>::
    FromMojom(content::mojom::PushPermissionStatus input,
              blink::WebPushPermissionStatus* output) {
  if (input >= content::mojom::PushPermissionStatus::GRANTED &&
      input <= content::mojom::PushPermissionStatus::LAST) {
    *output = static_cast<blink::WebPushPermissionStatus>(input);
    return true;
  }

  NOTREACHED();
  return false;
}

}  // namespace mojo
