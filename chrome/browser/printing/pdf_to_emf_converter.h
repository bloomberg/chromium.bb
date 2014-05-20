// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PDF_TO_EMF_CONVERTER_H_
#define CHROME_BROWSER_PRINTING_PDF_TO_EMF_CONVERTER_H_

#include "base/callback.h"
#include "base/memory/ref_counted_memory.h"

namespace base {
class FilePath;
}

namespace printing {
class PdfRenderSettings;
}

namespace printing {

class PdfToEmfConverter {
 public:
  // Callback for when the PDF is converted to an EMF.
  typedef base::Callback<void(double /*scale_factor*/,
                              const std::vector<base::FilePath>& /*emf_files*/)>
      ResultCallback;
  virtual ~PdfToEmfConverter() {}

  static scoped_ptr<PdfToEmfConverter> CreateDefault();

  virtual void Start(base::RefCountedMemory* data,
                     const printing::PdfRenderSettings& conversion_settings,
                     const ResultCallback& callback) = 0;
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PDF_TO_EMF_CONVERTER_H_
