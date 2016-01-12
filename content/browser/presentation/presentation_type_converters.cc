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
      return presentation::PRESENTATION_ERROR_TYPE_NO_AVAILABLE_SCREENS;
    case content::PRESENTATION_ERROR_SESSION_REQUEST_CANCELLED:
      return presentation::PRESENTATION_ERROR_TYPE_SESSION_REQUEST_CANCELLED;
    case content::PRESENTATION_ERROR_NO_PRESENTATION_FOUND:
      return presentation::PRESENTATION_ERROR_TYPE_NO_PRESENTATION_FOUND;
    case content::PRESENTATION_ERROR_UNKNOWN:
      return presentation::PRESENTATION_ERROR_TYPE_UNKNOWN;
  }
  NOTREACHED();
  return presentation::PRESENTATION_ERROR_TYPE_UNKNOWN;
}

presentation::PresentationConnectionState PresentationConnectionStateToMojo(
    content::PresentationConnectionState state) {
  switch (state) {
    case content::PRESENTATION_CONNECTION_STATE_CONNECTING:
      return presentation::PRESENTATION_CONNECTION_STATE_CONNECTING;
    case content::PRESENTATION_CONNECTION_STATE_CONNECTED:
      return presentation::PRESENTATION_CONNECTION_STATE_CONNECTED;
    case content::PRESENTATION_CONNECTION_STATE_CLOSED:
      return presentation::PRESENTATION_CONNECTION_STATE_CLOSED;
    case content::PRESENTATION_CONNECTION_STATE_TERMINATED:
      return presentation::PRESENTATION_CONNECTION_STATE_TERMINATED;
  }
  NOTREACHED();
  return presentation::PRESENTATION_CONNECTION_STATE_TERMINATED;
}

}  // namespace content

