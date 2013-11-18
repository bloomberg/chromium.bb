// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/pwg_raster_converter.h"

namespace local_discovery {

class PWGRasterConverterImpl : public PWGRasterConverter {
 public:
  PWGRasterConverterImpl();

  virtual ~PWGRasterConverterImpl();

  virtual void Start(base::RefCountedBytes* data,
                     const ResultCallback& callback) OVERRIDE;
};

// static
scoped_ptr<PWGRasterConverter> PWGRasterConverter::CreateDefault() {
  return scoped_ptr<PWGRasterConverter>(new PWGRasterConverterImpl());
}

PWGRasterConverterImpl::PWGRasterConverterImpl() {
}

PWGRasterConverterImpl::~PWGRasterConverterImpl() {
}

void PWGRasterConverterImpl::Start(base::RefCountedBytes* data,
                                   const ResultCallback& callback) {
  NOTIMPLEMENTED();
}

}  // namespace local_discovery
