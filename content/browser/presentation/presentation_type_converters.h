// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PRESENTATION_PRESENTATION_TYPE_CONVERTERS_H_
#define CONTENT_BROWSER_PRESENTATION_PRESENTATION_TYPE_CONVERTERS_H_

#include "content/common/content_export.h"
#include "content/common/presentation/presentation_service.mojom.h"
#include "content/public/browser/presentation_session.h"

namespace content {

CONTENT_EXPORT mojom::PresentationErrorType PresentationErrorTypeToMojo(
    PresentationErrorType input);

CONTENT_EXPORT mojom::PresentationConnectionState
PresentationConnectionStateToMojo(PresentationConnectionState state);

CONTENT_EXPORT mojom::PresentationConnectionCloseReason
PresentationConnectionCloseReasonToMojo(
    PresentationConnectionCloseReason reason);
}  // namespace content

namespace mojo {

template <>
struct TypeConverter<content::mojom::PresentationSessionInfoPtr,
                     content::PresentationSessionInfo> {
  static content::mojom::PresentationSessionInfoPtr Convert(
      const content::PresentationSessionInfo& input) {
    content::mojom::PresentationSessionInfoPtr output(
        content::mojom::PresentationSessionInfo::New());
    output->url = input.presentation_url;
    output->id = input.presentation_id;
    return output;
  }
};

template <>
struct TypeConverter<content::PresentationSessionInfo,
                     content::mojom::PresentationSessionInfoPtr> {
  static content::PresentationSessionInfo Convert(
      const content::mojom::PresentationSessionInfoPtr& input) {
    return content::PresentationSessionInfo(input->url, input->id);
  }
};

template <>
struct TypeConverter<content::mojom::PresentationErrorPtr,
                     content::PresentationError> {
  static content::mojom::PresentationErrorPtr Convert(
      const content::PresentationError& input) {
    content::mojom::PresentationErrorPtr output(
        content::mojom::PresentationError::New());
    output->error_type = PresentationErrorTypeToMojo(input.error_type);
    output->message = input.message;
    return output;
  }
};

}  // namespace mojo

#endif  // CONTENT_BROWSER_PRESENTATION_PRESENTATION_TYPE_CONVERTERS_H_
