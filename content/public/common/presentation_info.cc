// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/presentation_info.h"

namespace content {

PresentationInfo::PresentationInfo(const GURL& presentation_url,
                                   const std::string& presentation_id)
    : presentation_url(presentation_url), presentation_id(presentation_id) {}

PresentationInfo::~PresentationInfo() {}

}  // namespace content
