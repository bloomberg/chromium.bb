// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_composite_client.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/shared_memory_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(printing::PrintCompositeClient);

namespace printing {

PrintCompositeClient::PrintCompositeClient(content::WebContents* web_contents)
    : for_preview_(false) {
  DCHECK(web_contents);
}

PrintCompositeClient::~PrintCompositeClient() {}

void PrintCompositeClient::DoCompositePageToPdf(
    int document_cookie,
    uint64_t frame_guid,
    int page_num,
    base::SharedMemoryHandle handle,
    uint32_t data_size,
    const ContentToFrameMap& subframe_content_map,
    mojom::PdfCompositor::CompositePageToPdfCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto& compositor = GetCompositeRequest(document_cookie);

  DCHECK(data_size);
  mojo::ScopedSharedBufferHandle buffer_handle = mojo::WrapSharedMemoryHandle(
      handle, data_size,
      mojo::UnwrappedSharedMemoryHandleProtection::kReadOnly);
  // Since this class owns compositor, compositor will be gone when this class
  // is destructed. Mojo won't call its callback in that case so it is safe to
  // use unretained |this| pointer here.
  compositor->CompositePageToPdf(
      frame_guid, page_num, std::move(buffer_handle), subframe_content_map,
      base::BindOnce(&PrintCompositeClient::OnDidCompositePageToPdf,
                     base::Unretained(this), std::move(callback)));
}

void PrintCompositeClient::DoCompositeDocumentToPdf(
    int document_cookie,
    uint64_t frame_guid,
    base::SharedMemoryHandle handle,
    uint32_t data_size,
    const ContentToFrameMap& subframe_content_map,
    mojom::PdfCompositor::CompositeDocumentToPdfCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto& compositor = GetCompositeRequest(document_cookie);

  DCHECK(data_size);
  mojo::ScopedSharedBufferHandle buffer_handle = mojo::WrapSharedMemoryHandle(
      handle, data_size,
      mojo::UnwrappedSharedMemoryHandleProtection::kReadOnly);
  // Since this class owns compositor, compositor will be gone when this class
  // is destructed. Mojo won't call its callback in that case so it is safe to
  // use unretained |this| pointer here.
  compositor->CompositeDocumentToPdf(
      frame_guid, std::move(buffer_handle), subframe_content_map,
      base::BindOnce(&PrintCompositeClient::OnDidCompositeDocumentToPdf,
                     base::Unretained(this), document_cookie,
                     std::move(callback)));
}

void PrintCompositeClient::OnDidCompositePageToPdf(
    printing::mojom::PdfCompositor::CompositePageToPdfCallback callback,
    printing::mojom::PdfCompositor::Status status,
    mojo::ScopedSharedBufferHandle handle) {
  std::move(callback).Run(status, std::move(handle));
}

void PrintCompositeClient::OnDidCompositeDocumentToPdf(
    int document_cookie,
    printing::mojom::PdfCompositor::CompositeDocumentToPdfCallback callback,
    printing::mojom::PdfCompositor::Status status,
    mojo::ScopedSharedBufferHandle handle) {
  RemoveCompositeRequest(document_cookie);
  std::move(callback).Run(status, std::move(handle));
}

mojom::PdfCompositorPtr& PrintCompositeClient::GetCompositeRequest(int cookie) {
  auto iter = compositor_map_.find(cookie);
  if (iter != compositor_map_.end())
    return iter->second;

  auto iterator =
      compositor_map_.emplace(cookie, CreateCompositeRequest()).first;
  return iterator->second;
}

void PrintCompositeClient::RemoveCompositeRequest(int cookie) {
  size_t erased = compositor_map_.erase(cookie);
  DCHECK_EQ(erased, 1u);
}

mojom::PdfCompositorPtr PrintCompositeClient::CreateCompositeRequest() {
  if (!connector_) {
    service_manager::mojom::ConnectorRequest connector_request;
    connector_ = service_manager::Connector::Create(&connector_request);
    content::ServiceManagerConnection::GetForProcess()
        ->GetConnector()
        ->BindConnectorRequest(std::move(connector_request));
  }
  mojom::PdfCompositorPtr compositor;
  connector_->BindInterface(mojom::kServiceName, &compositor);
  return compositor;
}

}  // namespace printing
