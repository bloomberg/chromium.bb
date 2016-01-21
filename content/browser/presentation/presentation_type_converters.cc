// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/presentation/presentation_type_converters.h"

#include "base/logging.h"

namespace content {

presentation::PresentationErrorType PresentationErrorTypeToMojo(
    content::PresentationErrorType input) {
  switch (input) {
    case content::PRESENTATION_ERROR_NO_AVAILABLE_SCREENS:
      return presentation::PresentationErrorType::NO_AVAILABLE_SCREENS;
    case content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED:
      return presentation::PresentationErrorType::SESSION_REQUEST_CANCELLED;
    case content::PRESENTATION_ERROR_NO_PRESENTATION_FOUND:
      return presentation::PresentationErrorType::NO_PRESENTATION_FOUND;
    case content::PRESENTATION_ERROR_UNKNOWN:
      return presentation::PresentationErrorType::UNKNOWN;
  }
  NOTREACHED();
  return presentation::PresentationErrorType::UNKNOWN;
}

presentation::PresentationConnectionState PresentationConnectionStateToMojo(
    content::PresentationConnectionState state) {
  switch (state) {
    case content::PRESENTATION_CONNECTION_STATE_CONNECTING:
      return presentation::PresentationConnectionState::CONNECTING;
    case content::PRESENTATION_CONNECTION_STATE_CONNECTED:
      return presentation::PresentationConnectionState::CONNECTED;
    case content::PRESENTATION_CONNECTION_STATE_CLOSED:
      return presentation::PresentationConnectionState::CLOSED;
    case content::PRESENTATION_CONNECTION_STATE_TERMINATED:
      return presentation::PresentationConnectionState::TERMINATED;
  }
  NOTREACHED();
  return presentation::PresentationConnectionState::TERMINATED;
}

}  // namespace content

