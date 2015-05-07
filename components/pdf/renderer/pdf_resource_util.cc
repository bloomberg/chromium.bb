// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/pdf/renderer/pdf_resource_util.h"

#include "base/strings/utf_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "grit/components_scaled_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace pdf {

std::string GetStringResource(PP_ResourceString string_id) {
  int resource_id = 0;
  switch (string_id) {
    case PP_RESOURCESTRING_PDFGETPASSWORD:
      resource_id = IDS_PDF_NEED_PASSWORD;
      break;
    case PP_RESOURCESTRING_PDFLOADING:
      resource_id = IDS_PDF_PAGE_LOADING;
      break;
    case PP_RESOURCESTRING_PDFLOAD_FAILED:
      resource_id = IDS_PDF_PAGE_LOAD_FAILED;
      break;
    case PP_RESOURCESTRING_PDFPROGRESSLOADING:
      resource_id = IDS_PDF_PROGRESS_LOADING;
      break;
  }

  return l10n_util::GetStringUTF8(resource_id);
}

}  // namespace pdf
