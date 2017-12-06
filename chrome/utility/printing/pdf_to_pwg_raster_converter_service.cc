// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/printing/pdf_to_pwg_raster_converter_service.h"

#include "build/build_config.h"
#include "chrome/utility/printing/pdf_to_pwg_raster_converter_impl.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace printing {

namespace {

void OnPdfToPwgRasterConverterRequest(
    service_manager::ServiceContextRefFactory* ref_factory,
    printing::mojom::PdfToPwgRasterConverterRequest request) {
  mojo::MakeStrongBinding(
      std::make_unique<printing::PdfToPwgRasterConverterImpl>(
          ref_factory->CreateRef()),
      std::move(request));
}

}  // namespace

PdfToPwgRasterConverterService::PdfToPwgRasterConverterService() {}

PdfToPwgRasterConverterService::~PdfToPwgRasterConverterService() {}

std::unique_ptr<service_manager::Service>
PdfToPwgRasterConverterService::CreateService() {
  return std::make_unique<PdfToPwgRasterConverterService>();
}

void PdfToPwgRasterConverterService::OnStart() {
  ref_factory_ = std::make_unique<service_manager::ServiceContextRefFactory>(
      base::Bind(&service_manager::ServiceContext::RequestQuit,
                 base::Unretained(context())));
  registry_.AddInterface(
      base::Bind(&OnPdfToPwgRasterConverterRequest, ref_factory_.get()));
}

void PdfToPwgRasterConverterService::OnBindInterface(
    const service_manager::BindSourceInfo& source_info,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  registry_.BindInterface(interface_name, std::move(interface_pipe));
}

}  // namespace printing
