// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/blob_storage/blob_message_filter.h"

#include "content/child/blob_storage/blob_transport_controller.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/fileapi/webblob_messages.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "storage/common/blob_storage/blob_item_bytes_request.h"

namespace content {

BlobMessageFilter::BlobMessageFilter()
    : IPC::MessageFilter(), sender_(nullptr) {}

BlobMessageFilter::~BlobMessageFilter() {}

void BlobMessageFilter::OnFilterAdded(IPC::Sender* sender) {
  sender_ = sender;
}

bool BlobMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BlobMessageFilter, message)
    IPC_MESSAGE_HANDLER(BlobStorageMsg_RequestMemoryItem, OnRequestMemoryItem)
    IPC_MESSAGE_HANDLER(BlobStorageMsg_CancelBuildingBlob, OnCancelBuildingBlob)
    IPC_MESSAGE_HANDLER(BlobStorageMsg_DoneBuildingBlob, OnDoneBuildingBlob)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool BlobMessageFilter::GetSupportedMessageClasses(
    std::vector<uint32_t>* supported_message_classes) const {
  supported_message_classes->push_back(BlobMsgStart);
  return true;
}

void BlobMessageFilter::OnRequestMemoryItem(
    const std::string& uuid,
    const std::vector<storage::BlobItemBytesRequest>& requests,
    std::vector<base::SharedMemoryHandle> memory_handles,
    const std::vector<IPC::PlatformFileForTransit>& file_handles) {
  BlobTransportController::GetInstance()->OnMemoryRequest(
      uuid, requests, &memory_handles, file_handles, sender_);
}

void BlobMessageFilter::OnCancelBuildingBlob(
    const std::string& uuid,
    storage::IPCBlobCreationCancelCode code) {
  BlobTransportController::GetInstance()->OnCancel(uuid, code);
}

void BlobMessageFilter::OnDoneBuildingBlob(const std::string& uuid) {
  BlobTransportController::GetInstance()->OnDone(uuid);
}

}  // namespace content
