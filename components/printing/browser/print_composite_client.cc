// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/printing/browser/print_composite_client.h"

#include <memory>
#include <utility>

#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/service_manager_connection.h"
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

}  // namespace printing
