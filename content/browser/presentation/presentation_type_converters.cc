// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_type_converters.h"

#include "base/logging.h"

namespace content {

mojom::PresentationErrorType PresentationErrorTypeToMojo(
    content::PresentationErrorType input) {
  switch (input) {
    case content::PRESENTATION_ERROR_NO_AVAILABLE_SCREENS:
      return mojom::PresentationErrorType::NO_AVAILABLE_SCREENS;
    case content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED:
      return mojom::PresentationErrorType::SESSION_REQUEST_CANCELLED;
    case content::PRESENTATION_ERROR_NO_PRESENTATION_FOUND:
      return mojom::PresentationErrorType::NO_PRESENTATION_FOUND;
    case content::PRESENTATION_ERROR_UNKNOWN:
      return mojom::PresentationErrorType::UNKNOWN;
  }
  NOTREACHED();
  return mojom::PresentationErrorType::UNKNOWN;
}

mojom::PresentationConnectionState PresentationConnectionStateToMojo(
    content::PresentationConnectionState state) {
  switch (state) {
    case content::PRESENTATION_CONNECTION_STATE_CONNECTING:
      return mojom::PresentationConnectionState::CONNECTING;
    case content::PRESENTATION_CONNECTION_STATE_CONNECTED:
      return mojom::PresentationConnectionState::CONNECTED;
    case content::PRESENTATION_CONNECTION_STATE_CLOSED:
      return mojom::PresentationConnectionState::CLOSED;
    case content::PRESENTATION_CONNECTION_STATE_TERMINATED:
      return mojom::PresentationConnectionState::TERMINATED;
  }
  NOTREACHED();
  return mojom::PresentationConnectionState::TERMINATED;
}

mojom::PresentationConnectionCloseReason
PresentationConnectionCloseReasonToMojo(
    content::PresentationConnectionCloseReason reason) {
  switch (reason) {
    case content::PRESENTATION_CONNECTION_CLOSE_REASON_CONNECTION_ERROR:
      return mojom::PresentationConnectionCloseReason::CONNECTION_ERROR;
    case content::PRESENTATION_CONNECTION_CLOSE_REASON_CLOSED:
      return mojom::PresentationConnectionCloseReason::CLOSED;
    case content::PRESENTATION_CONNECTION_CLOSE_REASON_WENT_AWAY:
      return mojom::PresentationConnectionCloseReason::WENT_AWAY;
  }
  NOTREACHED();
  return mojom::PresentationConnectionCloseReason::CONNECTION_ERROR;
}

}  // namespace content

