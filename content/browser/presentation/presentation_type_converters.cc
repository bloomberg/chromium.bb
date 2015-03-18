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
    default:
      NOTREACHED();
      return presentation::PRESENTATION_ERROR_TYPE_UNKNOWN;
  }
}

}  // namespace content

