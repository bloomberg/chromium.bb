// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/presentation_info.h"

namespace content {

PresentationInfo::PresentationInfo(const GURL& presentation_url,
                                   const std::string& presentation_id)
    : presentation_url(presentation_url), presentation_id(presentation_id) {}

PresentationInfo::~PresentationInfo() {}

PresentationError::PresentationError()
    : error_type(PRESENTATION_ERROR_UNKNOWN) {}

PresentationError::PresentationError(PresentationErrorType error_type,
                                     const std::string& message)
    : error_type(error_type), message(message) {}

PresentationError::~PresentationError() {}

}  // namespace content
