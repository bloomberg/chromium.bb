// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PDF_RENDERER_PDF_RESOURCE_UTIL_H_
#define COMPONENTS_PDF_RENDERER_PDF_RESOURCE_UTIL_H_

#include <string>

#include "ppapi/c/private/ppb_pdf.h"

namespace gfx {
class ImageSkia;
}

namespace pdf {

gfx::ImageSkia* GetImageResource(PP_ResourceImage image_id);

std::string GetStringResource(PP_ResourceString string_id);

}  // namespace pdf

#endif  // COMPONENTS_PDF_RENDERER_PDF_RESOURCE_UTIL_H_
