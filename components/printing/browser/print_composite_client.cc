// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_composite_client.h"

#include <memory>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "services/service_manager/public/cpp/connector.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(printing::PrintCompositeClient);

namespace printing {

PrintCompositeClient::PrintCompositeClient(content::WebContents* web_contents)
    : for_preview_(false) {}

PrintCompositeClient::~PrintCompositeClient() {}

void PrintCompositeClient::CreateConnectorRequest() {
  connector_ = service_manager::Connector::Create(&connector_request_);
  content::ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindConnectorRequest(std::move(connector_request_));
}

void PrintCompositeClient::DoComposite(
    base::SharedMemoryHandle handle,
    uint32_t data_size,
    mojom::PdfCompositor::CompositePdfCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(data_size);

  if (!connector_)
    CreateConnectorRequest();
  Composite(connector_.get(), handle, data_size, std::move(callback),
            base::ThreadTaskRunnerHandle::Get());
}

std::unique_ptr<base::SharedMemory> PrintCompositeClient::GetShmFromMojoHandle(
    mojo::ScopedSharedBufferHandle handle) {
  base::SharedMemoryHandle memory_handle;
  size_t memory_size = 0;
  bool read_only_flag = false;

  const MojoResult result = mojo::UnwrapSharedMemoryHandle(
      std::move(handle), &memory_handle, &memory_size, &read_only_flag);
  if (result != MOJO_RESULT_OK)
    return nullptr;
  DCHECK_GT(memory_size, 0u);

  std::unique_ptr<base::SharedMemory> shm =
      base::MakeUnique<base::SharedMemory>(memory_handle, true /* read_only */);
  if (!shm->Map(memory_size)) {
    DLOG(ERROR) << "Map shared memory failed.";
    return nullptr;
  }
  return shm;
}

scoped_refptr<base::RefCountedBytes>
PrintCompositeClient::GetDataFromMojoHandle(
    mojo::ScopedSharedBufferHandle handle) {
  std::unique_ptr<base::SharedMemory> shm =
      GetShmFromMojoHandle(std::move(handle));
  if (!shm)
    return nullptr;

  return base::MakeRefCounted<base::RefCountedBytes>(
      reinterpret_cast<const unsigned char*>(shm->memory()),
      shm->mapped_size());
}

}  // namespace printing
