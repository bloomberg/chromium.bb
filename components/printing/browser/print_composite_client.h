// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PRINTING_BROWSER_PRINT_COMPOSITE_CLIENT_H_
#define COMPONENTS_PRINTING_BROWSER_PRINT_COMPOSITE_CLIENT_H_

#include <map>
#include <memory>

#include "base/optional.h"
#include "components/printing/service/public/interfaces/pdf_compositor.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/service_manager/public/cpp/connector.h"

namespace printing {

// Class to manage print requests and their communication with pdf
// compositor service.
// Each composite request have a separate interface pointer to connect
// with remote service.
class PrintCompositeClient
    : public content::WebContentsUserData<PrintCompositeClient> {
 public:
  using ContentToFrameMap = std::unordered_map<uint32_t, uint64_t>;

  explicit PrintCompositeClient(content::WebContents* web_contents);
  ~PrintCompositeClient() override;

  // NOTE: |handle| must be a READ-ONLY base::SharedMemoryHandle, i.e. one
  // acquired by base::SharedMemory::GetReadOnlyHandle().

  // Printing single pages is only used by print preview for early return of
  // rendered results. In this case, the pages share the content with printed
  // document. The entire document will always be printed and sent at the end.
  // This is for compositing such a single preview page.
  void DoCompositePageToPdf(
      int cookie,
      uint64_t frame_guid,
      int page_num,
      base::SharedMemoryHandle handle,
      uint32_t data_size,
      const ContentToFrameMap& subframe_content_map,
      mojom::PdfCompositor::CompositePageToPdfCallback callback);

  // Used for compositing the entire document for print preview or actual
  // printing.
  void DoCompositeDocumentToPdf(
      int cookie,
      uint64_t frame_guid,
      base::SharedMemoryHandle handle,
      uint32_t data_size,
      const ContentToFrameMap& subframe_content_map,
      mojom::PdfCompositor::CompositeDocumentToPdfCallback callback);

  void set_for_preview(bool for_preview) { for_preview_ = for_preview; }
  bool for_preview() const { return for_preview_; }

 private:
  // Since page number is always non-negative, use this value to indicate it is
  // for the whole document -- no page number specified.
  static constexpr int kPageNumForWholeDoc = -1;

  // Callback functions for getting the replies.
  void OnDidCompositePageToPdf(
      printing::mojom::PdfCompositor::CompositePageToPdfCallback callback,
      printing::mojom::PdfCompositor::Status status,
      mojo::ScopedSharedBufferHandle handle);

  void OnDidCompositeDocumentToPdf(
      int document_cookie,
      printing::mojom::PdfCompositor::CompositeDocumentToPdfCallback callback,
      printing::mojom::PdfCompositor::Status status,
      mojo::ScopedSharedBufferHandle handle);

  // Get the request or create a new one if none exists.
  // Since printed pages always share content with it document, they share the
  // same composite request.
  mojom::PdfCompositorPtr& GetCompositeRequest(int cookie);

  // Remove an existing request from |compositor_map_|.
  void RemoveCompositeRequest(int cookie);

  mojom::PdfCompositorPtr CreateCompositeRequest();

  // Whether this client is created for print preview dialog.
  bool for_preview_;

  std::unique_ptr<service_manager::Connector> connector_;

  // Stores the mapping between document cookies and their corresponding
  // requests.
  std::map<int, mojom::PdfCompositorPtr> compositor_map_;

  DISALLOW_COPY_AND_ASSIGN(PrintCompositeClient);
};

}  // namespace printing

#endif  // COMPONENTS_PRINTING_BROWSER_PRINT_COMPOSITE_CLIENT_H_
