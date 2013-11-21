// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/pwg_raster_converter.h"

#include "base/logging.h"

namespace local_discovery {

namespace {

class PWGRasterConverterImpl : public PWGRasterConverter {
 public:
  PWGRasterConverterImpl();

  virtual ~PWGRasterConverterImpl();

  virtual void Start(base::RefCountedMemory* data,
                     const printing::PdfRenderSettings& conversion_settings,
                     const ResultCallback& callback) OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(PWGRasterConverterImpl);
};

PWGRasterConverterImpl::PWGRasterConverterImpl() {
}

PWGRasterConverterImpl::~PWGRasterConverterImpl() {
}

void PWGRasterConverterImpl::Start(
    base::RefCountedMemory* data,
    const printing::PdfRenderSettings& conversion_settings,
    const ResultCallback& callback) {
  NOTIMPLEMENTED();
}

}  // namespace

// static
scoped_ptr<PWGRasterConverter> PWGRasterConverter::CreateDefault() {
  return scoped_ptr<PWGRasterConverter>(new PWGRasterConverterImpl());
}

}  // namespace local_discovery
