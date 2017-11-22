// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/service/pdf_compositor_impl.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_handle.h"
#include "components/printing/service/public/cpp/pdf_service_mojo_utils.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "printing/common/pdf_metafile_utils.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDocument.h"
#include "third_party/skia/src/utils/SkMultiPictureDocument.h"

namespace printing {

PdfCompositorImpl::PdfCompositorImpl(
    const std::string& creator,
    std::unique_ptr<service_manager::ServiceContextRef> service_ref)
    : service_ref_(std::move(service_ref)), creator_(creator) {}

PdfCompositorImpl::~PdfCompositorImpl() = default;

void PdfCompositorImpl::CompositePdf(
    mojo::ScopedSharedBufferHandle sk_handle,
    mojom::PdfCompositor::CompositePdfCallback callback) {
  DCHECK(sk_handle.is_valid());

  std::unique_ptr<base::SharedMemory> shm =
      GetShmFromMojoHandle(std::move(sk_handle));

  SkMemoryStream stream(shm->memory(), shm->mapped_size());
  int page_count = SkMultiPictureDocumentReadPageCount(&stream);
  if (!page_count) {
    DLOG(ERROR) << "CompositePdf: No page is read.";
    std::move(callback).Run(mojom::PdfCompositor::Status::CONTENT_FORMAT_ERROR,
                            mojo::ScopedSharedBufferHandle());
    return;
  }

  std::vector<SkDocumentPage> pages(page_count);
  if (!SkMultiPictureDocumentRead(&stream, pages.data(), page_count)) {
    DLOG(ERROR) << "CompositePdf: Page reading failed.";
    std::move(callback).Run(mojom::PdfCompositor::Status::CONTENT_FORMAT_ERROR,
                            mojo::ScopedSharedBufferHandle());
    return;
  }

  SkDynamicMemoryWStream wstream;
  sk_sp<SkDocument> doc = MakePdfDocument(creator_, &wstream);

  for (const auto& page : pages) {
    SkCanvas* canvas = doc->beginPage(page.fSize.width(), page.fSize.height());
    canvas->drawPicture(page.fPicture);
    doc->endPage();
  }
  doc->close();

  mojo::ScopedSharedBufferHandle buffer =
      mojo::SharedBufferHandle::Create(wstream.bytesWritten());
  DCHECK(buffer.is_valid());

  mojo::ScopedSharedBufferMapping mapping = buffer->Map(wstream.bytesWritten());
  DCHECK(mapping);
  wstream.copyToAndReset(mapping.get());

  std::move(callback).Run(mojom::PdfCompositor::Status::SUCCESS,
                          std::move(buffer));
}

}  // namespace printing
