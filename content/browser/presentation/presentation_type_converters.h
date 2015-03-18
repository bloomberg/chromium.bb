// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRESENTATION_PRESENTATION_TYPE_CONVERTERS_H_
#define CONTENT_BROWSER_PRESENTATION_PRESENTATION_TYPE_CONVERTERS_H_

#include "content/common/content_export.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/browser/presentation_session.h"

namespace content {

CONTENT_EXPORT presentation::PresentationErrorType PresentationErrorTypeToMojo(
    PresentationErrorType input);

}  // namespace content

namespace mojo {

// MediaSink conversion helpers.
template <>
struct TypeConverter<presentation::PresentationSessionInfoPtr,
                     content::PresentationSessionInfo> {
  static presentation::PresentationSessionInfoPtr Convert(
      const content::PresentationSessionInfo& input) {
    presentation::PresentationSessionInfoPtr output(
        presentation::PresentationSessionInfo::New());
    output->url = input.presentation_url;
    output->id = input.presentation_id;
    return output.Pass();
  }
};

template <>
struct TypeConverter<presentation::PresentationErrorPtr,
                     content::PresentationError> {
  static presentation::PresentationErrorPtr Convert(
      const content::PresentationError& input) {
    presentation::PresentationErrorPtr output(
        presentation::PresentationError::New());
    output->error_type = PresentationErrorTypeToMojo(input.error_type);
    output->message = input.message;
    return output.Pass();
  }
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_TYPE_CONVERTERS_H_
