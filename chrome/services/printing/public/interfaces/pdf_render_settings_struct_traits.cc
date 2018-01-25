// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/printing/public/interfaces/pdf_render_settings_struct_traits.h"

#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

// static
bool StructTraits<printing::mojom::PdfRenderSettingsDataView,
                  printing::PdfRenderSettings>::
    Read(printing::mojom::PdfRenderSettingsDataView data,
         printing::PdfRenderSettings* out) {
  out->autorotate = data.autorotate();
  return data.ReadArea(&out->area) && data.ReadOffsets(&out->offsets) &&
         data.ReadDpi(&out->dpi) && data.ReadMode(&out->mode);
}

}  // namespace mojo
