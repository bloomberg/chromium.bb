// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/push_messaging_param_traits.h"

namespace mojo {

// PushRegistrationStatus
static_assert(
    content::PushRegistrationStatus::
            PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE),
    "PushRegistrationStatus enums must match, SUCCESS_FROM_PUSH_SERVICE");

static_assert(
    content::PushRegistrationStatus::
            PUSH_REGISTRATION_STATUS_NO_SERVICE_WORKER ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::NO_SERVICE_WORKER),
    "PushRegistrationStatus enums must match, NO_SERVICE_WORKER");

static_assert(
    content::PushRegistrationStatus::
            PUSH_REGISTRATION_STATUS_SERVICE_NOT_AVAILABLE ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::SERVICE_NOT_AVAILABLE),
    "PushRegistrationStatus enums must match, SERVICE_NOT_AVAILABLE");

static_assert(
    content::PushRegistrationStatus::PUSH_REGISTRATION_STATUS_LIMIT_REACHED ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::LIMIT_REACHED),
    "PushRegistrationStatus enums must match, LIMIT_REACHED");

static_assert(
    content::PushRegistrationStatus::
            PUSH_REGISTRATION_STATUS_PERMISSION_DENIED ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::PERMISSION_DENIED),
    "PushRegistrationStatus enums must match, PERMISSION_DENIED");

static_assert(
    content::PushRegistrationStatus::PUSH_REGISTRATION_STATUS_SERVICE_ERROR ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::SERVICE_ERROR),
    "PushRegistrationStatus enums must match, SERVICE_ERROR");

static_assert(
    content::PushRegistrationStatus::PUSH_REGISTRATION_STATUS_NO_SENDER_ID ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::NO_SENDER_ID),
    "PushRegistrationStatus enums must match, NO_SENDER_ID");

static_assert(
    content::PushRegistrationStatus::PUSH_REGISTRATION_STATUS_STORAGE_ERROR ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::STORAGE_ERROR),
    "PushRegistrationStatus enums must match, STORAGE_ERROR");

static_assert(
    content::PushRegistrationStatus::
            PUSH_REGISTRATION_STATUS_SUCCESS_FROM_CACHE ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::SUCCESS_FROM_CACHE),
    "PushRegistrationStatus enums must match, SUCCESS_FROM_CACHE");

static_assert(
    content::PushRegistrationStatus::PUSH_REGISTRATION_STATUS_NETWORK_ERROR ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::NETWORK_ERROR),
    "PushRegistrationStatus enums must match, NETWORK_ERROR");

static_assert(
    content::PushRegistrationStatus::
            PUSH_REGISTRATION_STATUS_INCOGNITO_PERMISSION_DENIED ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::
                INCOGNITO_PERMISSION_DENIED),
    "PushRegistrationStatus enums must match, INCOGNITO_PERMISSION_DENIED");

static_assert(
    content::PushRegistrationStatus::
            PUSH_REGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::PUBLIC_KEY_UNAVAILABLE),
    "PushRegistrationStatus enums must match, PUBLIC_KEY_UNAVAILABLE");

static_assert(
    content::PushRegistrationStatus::
            PUSH_REGISTRATION_STATUS_MANIFEST_EMPTY_OR_MISSING ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::MANIFEST_EMPTY_OR_MISSING),
    "PushRegistrationStatus enums must match, MANIFEST_EMPTY_OR_MISSING");

static_assert(
    content::PushRegistrationStatus::
            PUSH_REGISTRATION_STATUS_SENDER_ID_MISMATCH ==
        static_cast<content::PushRegistrationStatus>(
            content::mojom::PushRegistrationStatus::SENDER_ID_MISMATCH),
    "PushRegistrationStatus enums must match, SENDER_ID_MISMATCH");

static_assert(content::PushRegistrationStatus::PUSH_REGISTRATION_STATUS_LAST ==
                  static_cast<content::PushRegistrationStatus>(
                      content::mojom::PushRegistrationStatus::LAST),
              "PushRegistrationStatus enums must match, LAST");

// PushErrorType
static_assert(blink::WebPushError::ErrorType::ErrorTypeAbort ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::ABORT),
              "PushErrorType enums must match, ABORT");

static_assert(blink::WebPushError::ErrorType::ErrorTypeNetwork ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::NETWORK),
              "PushErrorType enums must match, NETWORK");

static_assert(blink::WebPushError::ErrorType::ErrorTypeNone ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::NONE),
              "PushErrorType enums must match, NONE");

static_assert(blink::WebPushError::ErrorType::ErrorTypeNotAllowed ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::NOT_ALLOWED),
              "PushErrorType enums must match, NOT_ALLOWED");

static_assert(blink::WebPushError::ErrorType::ErrorTypeNotFound ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::NOT_FOUND),
              "PushErrorType enums must match, NOT_FOUND");

static_assert(blink::WebPushError::ErrorType::ErrorTypeNotSupported ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::NOT_SUPPORTED),
              "PushErrorType enums must match, NOT_SUPPORTED");

static_assert(blink::WebPushError::ErrorType::ErrorTypeInvalidState ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::INVALID_STATE),
              "PushErrorType enums must match, INVALID_STATE");

static_assert(blink::WebPushError::ErrorType::ErrorTypeLast ==
                  static_cast<blink::WebPushError::ErrorType>(
                      content::mojom::PushErrorType::LAST),
              "PushErrorType enums must match, LAST");

// PushGetRegistrationStatus
static_assert(
    content::PushGetRegistrationStatus::PUSH_GETREGISTRATION_STATUS_SUCCESS ==
        static_cast<content::PushGetRegistrationStatus>(
            content::mojom::PushGetRegistrationStatus::SUCCESS),
    "PushGetRegistrationStatus enums must match, SUCCESS");

static_assert(
    content::PushGetRegistrationStatus::
            PUSH_GETREGISTRATION_STATUS_SERVICE_NOT_AVAILABLE ==
        static_cast<content::PushGetRegistrationStatus>(
            content::mojom::PushGetRegistrationStatus::SERVICE_NOT_AVAILABLE),
    "PushGetRegistrationStatus enums must match, SERVICE_NOT_AVAILABLE");

static_assert(content::PushGetRegistrationStatus::
                      PUSH_GETREGISTRATION_STATUS_STORAGE_ERROR ==
                  static_cast<content::PushGetRegistrationStatus>(
                      content::mojom::PushGetRegistrationStatus::STORAGE_ERROR),
              "PushGetRegistrationStatus enums must match, STORAGE_ERROR");

static_assert(
    content::PushGetRegistrationStatus::
            PUSH_GETREGISTRATION_STATUS_REGISTRATION_NOT_FOUND ==
        static_cast<content::PushGetRegistrationStatus>(
            content::mojom::PushGetRegistrationStatus::REGISTRATION_NOT_FOUND),
    "PushGetRegistrationStatus enums must match, REGISTRATION_NOT_FOUND");

static_assert(
    content::PushGetRegistrationStatus::
            PUSH_GETREGISTRATION_STATUS_INCOGNITO_REGISTRATION_NOT_FOUND ==
        static_cast<content::PushGetRegistrationStatus>(
            content::mojom::PushGetRegistrationStatus::
                INCOGNITO_REGISTRATION_NOT_FOUND),
    "PushGetRegistrationStatus enums must match, "
    "INCOGNITO_REGISTRATION_NOT_FOUND");

static_assert(
    content::PushGetRegistrationStatus::
            PUSH_GETREGISTRATION_STATUS_PUBLIC_KEY_UNAVAILABLE ==
        static_cast<content::PushGetRegistrationStatus>(
            content::mojom::PushGetRegistrationStatus::PUBLIC_KEY_UNAVAILABLE),
    "PushGetRegistrationStatus enums must match, PUBLIC_KEY_UNAVAILABLE");

static_assert(
    content::PushGetRegistrationStatus::PUSH_GETREGISTRATION_STATUS_LAST ==
        static_cast<content::PushGetRegistrationStatus>(
            content::mojom::PushGetRegistrationStatus::LAST),
    "PushGetRegistrationStatus enums must match, LAST");

// PushPermissionStatus
static_assert(blink::WebPushPermissionStatus::WebPushPermissionStatusGranted ==
                  static_cast<blink::WebPushPermissionStatus>(
                      content::mojom::PushPermissionStatus::GRANTED),
              "PushPermissionStatus enums must match, GRANTED");

static_assert(blink::WebPushPermissionStatus::WebPushPermissionStatusDenied ==
                  static_cast<blink::WebPushPermissionStatus>(
                      content::mojom::PushPermissionStatus::DENIED),
              "PushPermissionStatus enums must match, DENIED");

static_assert(blink::WebPushPermissionStatus::WebPushPermissionStatusPrompt ==
                  static_cast<blink::WebPushPermissionStatus>(
                      content::mojom::PushPermissionStatus::PROMPT),
              "PushPermissionStatus enums must match, PROMPT");

static_assert(blink::WebPushPermissionStatus::WebPushPermissionStatusLast ==
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
content::mojom::PushRegistrationStatus EnumTraits<
    content::mojom::PushRegistrationStatus,
    content::PushRegistrationStatus>::ToMojom(content::PushRegistrationStatus
                                                  input) {
  if (input >= content::PushRegistrationStatus::
                   PUSH_REGISTRATION_STATUS_SUCCESS_FROM_PUSH_SERVICE &&
      input <= content::PushRegistrationStatus::PUSH_REGISTRATION_STATUS_LAST) {
    return static_cast<content::mojom::PushRegistrationStatus>(input);
  }

  NOTREACHED();
  return content::mojom::PushRegistrationStatus::SERVICE_ERROR;
}

// static
bool EnumTraits<content::mojom::PushRegistrationStatus,
                content::PushRegistrationStatus>::
    FromMojom(content::mojom::PushRegistrationStatus input,
              content::PushRegistrationStatus* output) {
  if (input >=
          content::mojom::PushRegistrationStatus::SUCCESS_FROM_PUSH_SERVICE &&
      input <= content::mojom::PushRegistrationStatus::LAST) {
    *output = static_cast<content::PushRegistrationStatus>(input);
    return true;
  }

  NOTREACHED();
  return false;
}

// static
content::mojom::PushErrorType
EnumTraits<content::mojom::PushErrorType, blink::WebPushError::ErrorType>::
    ToMojom(blink::WebPushError::ErrorType input) {
  if (input >= blink::WebPushError::ErrorType::ErrorTypeAbort &&
      input <= blink::WebPushError::ErrorType::ErrorTypeInvalidState) {
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
content::mojom::PushGetRegistrationStatus
EnumTraits<content::mojom::PushGetRegistrationStatus,
           content::PushGetRegistrationStatus>::
    ToMojom(content::PushGetRegistrationStatus input) {
  if (input >= content::PushGetRegistrationStatus::
                   PUSH_GETREGISTRATION_STATUS_SUCCESS &&
      input <= content::PushGetRegistrationStatus::
                   PUSH_GETREGISTRATION_STATUS_LAST) {
    return static_cast<content::mojom::PushGetRegistrationStatus>(input);
  }

  NOTREACHED();
  return content::mojom::PushGetRegistrationStatus::SERVICE_NOT_AVAILABLE;
}

// static
bool EnumTraits<content::mojom::PushGetRegistrationStatus,
                content::PushGetRegistrationStatus>::
    FromMojom(content::mojom::PushGetRegistrationStatus input,
              content::PushGetRegistrationStatus* output) {
  if (input >= content::mojom::PushGetRegistrationStatus::SUCCESS &&
      input <= content::mojom::PushGetRegistrationStatus::LAST) {
    *output = static_cast<content::PushGetRegistrationStatus>(input);
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
  if (input >= blink::WebPushPermissionStatus::WebPushPermissionStatusGranted &&
      input <= blink::WebPushPermissionStatus::WebPushPermissionStatusLast) {
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
