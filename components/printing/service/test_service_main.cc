// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_discardable_memory_allocator.h"
#include "components/printing/service/pdf_compositor_service.h"
#include "services/service_manager/public/c/main.h"
#include "services/service_manager/public/cpp/service_runner.h"

// In order to test PdfCompositorService, this class overrides and
// uses a test discardable memory allocator.
class PdfCompositorTestService : public printing::PdfCompositorService {
 public:
  explicit PdfCompositorTestService(const std::string& creator)
      : PdfCompositorService(creator) {}
  ~PdfCompositorTestService() override {}

  // PdfCompositorService:
  void PrepareToStart() override;

 private:
  base::TestDiscardableMemoryAllocator mem_allocator_;
};

void PdfCompositorTestService::PrepareToStart() {
  base::DiscardableMemoryAllocator::SetInstance(&mem_allocator_);
}

MojoResult ServiceMain(MojoHandle service_request_handle) {
  service_manager::ServiceRunner runner(
      new PdfCompositorTestService("pdf_compositor_service_unittest"));
  return runner.Run(service_request_handle);
}
