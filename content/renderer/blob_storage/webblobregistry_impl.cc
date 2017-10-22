// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/blob_storage/webblobregistry_impl.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "content/child/child_thread_impl.h"
#include "content/child/thread_safe_sender.h"
#include "content/renderer/blob_storage/blob_consolidation.h"
#include "content/renderer/blob_storage/blob_transport_controller.h"
#include "content/common/fileapi/webblob_messages.h"
#include "third_party/WebKit/public/platform/FilePathConversion.h"
#include "third_party/WebKit/public/platform/WebBlobData.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebThreadSafeData.h"
#include "third_party/WebKit/public/platform/WebURL.h"

using blink::WebBlobData;
using blink::WebString;
using blink::WebThreadSafeData;
using blink::WebURL;
using blink::WebBlobRegistry;
using storage::DataElement;

namespace content {

WebBlobRegistryImpl::WebBlobRegistryImpl(
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_runner,
    scoped_refptr<ThreadSafeSender> sender)
    : io_runner_(std::move(io_runner)),
      main_runner_(std::move(main_runner)),
      sender_(std::move(sender)) {
  // Record a dummy trace event on startup so the 'Storage' category shows up
  // in the chrome://tracing viewer.
  TRACE_EVENT0("Blob", "Init");
}

WebBlobRegistryImpl::~WebBlobRegistryImpl() {
}

std::unique_ptr<WebBlobRegistry::Builder> WebBlobRegistryImpl::CreateBuilder(
    const blink::WebString& uuid,
    const blink::WebString& content_type) {
  return base::WrapUnique(new BuilderImpl(uuid, content_type, sender_.get(),
                                          io_runner_, main_runner_));
}

void WebBlobRegistryImpl::RegisterBlobData(const blink::WebString& uuid,
                                           const blink::WebBlobData& data) {
  TRACE_EVENT0("Blob", "Registry::RegisterBlob");
  std::unique_ptr<Builder> builder(CreateBuilder(uuid, data.ContentType()));

  // This is temporary until we move to createBuilder() as our blob creation
  // method.
  size_t i = 0;
  WebBlobData::Item data_item;
  while (data.ItemAt(i++, data_item)) {
    if (data_item.length == 0) {
      continue;
    }
    switch (data_item.type) {
      case WebBlobData::Item::kTypeData: {
        // WebBlobData does not allow partial data items.
        DCHECK(!data_item.offset && data_item.length == -1);
        builder->AppendData(data_item.data);
        break;
      }
      case WebBlobData::Item::kTypeFile:
        builder->AppendFile(data_item.file_path,
                            static_cast<uint64_t>(data_item.offset),
                            static_cast<uint64_t>(data_item.length),
                            data_item.expected_modification_time);
        break;
      case WebBlobData::Item::kTypeBlob:
        builder->AppendBlob(data_item.blob_uuid, data_item.offset,
                            data_item.length);
        break;
      case WebBlobData::Item::kTypeFileSystemURL:
        // We only support filesystem URL as of now.
        DCHECK(GURL(data_item.file_system_url).SchemeIsFileSystem());
        builder->AppendFileSystemURL(data_item.file_system_url,
                                     static_cast<uint64_t>(data_item.offset),
                                     static_cast<uint64_t>(data_item.length),
                                     data_item.expected_modification_time);
        break;
      default:
        NOTREACHED();
    }
  }
  builder->Build();
}

void WebBlobRegistryImpl::AddBlobDataRef(const WebString& uuid) {
  sender_->Send(new BlobHostMsg_IncrementRefCount(uuid.Utf8()));
}

void WebBlobRegistryImpl::RemoveBlobDataRef(const WebString& uuid) {
  sender_->Send(new BlobHostMsg_DecrementRefCount(uuid.Utf8()));
}

void WebBlobRegistryImpl::RegisterPublicBlobURL(const WebURL& url,
                                                const WebString& uuid) {
  // Measure how much jank the following synchronous IPC introduces.
  SCOPED_UMA_HISTOGRAM_TIMER("Storage.Blob.RegisterPublicURLTime");

  sender_->Send(new BlobHostMsg_RegisterPublicURL(url, uuid.Utf8()));
}

void WebBlobRegistryImpl::RevokePublicBlobURL(const WebURL& url) {
  sender_->Send(new BlobHostMsg_RevokePublicURL(url));
}

WebBlobRegistryImpl::BuilderImpl::BuilderImpl(
    const blink::WebString& uuid,
    const blink::WebString& content_type,
    ThreadSafeSender* sender,
    scoped_refptr<base::SingleThreadTaskRunner> io_runner,
    scoped_refptr<base::SingleThreadTaskRunner> main_runner)
    : uuid_(uuid.Utf8()),
      content_type_(content_type.Utf8()),
      consolidation_(new BlobConsolidation()),
      sender_(sender),
      io_runner_(std::move(io_runner)),
      main_runner_(std::move(main_runner)) {}

WebBlobRegistryImpl::BuilderImpl::~BuilderImpl() {
}

void WebBlobRegistryImpl::BuilderImpl::AppendData(
    const WebThreadSafeData& data) {
  consolidation_->AddDataItem(data);
}

void WebBlobRegistryImpl::BuilderImpl::AppendBlob(const WebString& uuid,
                                                  uint64_t offset,
                                                  uint64_t length) {
  consolidation_->AddBlobItem(uuid.Utf8(), offset, length);
}

void WebBlobRegistryImpl::BuilderImpl::AppendFile(
    const WebString& path,
    uint64_t offset,
    uint64_t length,
    double expected_modification_time) {
  consolidation_->AddFileItem(blink::WebStringToFilePath(path), offset, length,
                              expected_modification_time);
}

void WebBlobRegistryImpl::BuilderImpl::AppendFileSystemURL(
    const WebURL& fileSystemURL,
    uint64_t offset,
    uint64_t length,
    double expected_modification_time) {
  DCHECK(GURL(fileSystemURL).SchemeIsFileSystem());
  consolidation_->AddFileSystemItem(GURL(fileSystemURL), offset, length,
                                    expected_modification_time);
}

void WebBlobRegistryImpl::BuilderImpl::Build() {
  BlobTransportController::InitiateBlobTransfer(
      uuid_, content_type_, std::move(consolidation_), sender_,
      io_runner_.get(), main_runner_);
}

}  // namespace content
