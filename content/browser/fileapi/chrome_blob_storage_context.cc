// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/chrome_blob_storage_context.h"

#include "base/bind.h"
#include "base/guid.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "storage/browser/blob/blob_storage_context.h"

using base::UserDataAdapter;
using storage::BlobStorageContext;

namespace content {

namespace {

const char kBlobStorageContextKeyName[] = "content_blob_storage_context";

class BlobHandleImpl : public BlobHandle {
 public:
  explicit BlobHandleImpl(scoped_ptr<storage::BlobDataHandle> handle)
      : handle_(handle.Pass()) {
  }

  virtual ~BlobHandleImpl() {}

  virtual std::string GetUUID() OVERRIDE {
    return handle_->uuid();
  }

 private:
  scoped_ptr<storage::BlobDataHandle> handle_;
};

}  // namespace

ChromeBlobStorageContext::ChromeBlobStorageContext() {}

ChromeBlobStorageContext* ChromeBlobStorageContext::GetFor(
    BrowserContext* context) {
  if (!context->GetUserData(kBlobStorageContextKeyName)) {
    scoped_refptr<ChromeBlobStorageContext> blob =
        new ChromeBlobStorageContext();
    context->SetUserData(
        kBlobStorageContextKeyName,
        new UserDataAdapter<ChromeBlobStorageContext>(blob.get()));
    // Check first to avoid memory leak in unittests.
    if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&ChromeBlobStorageContext::InitializeOnIOThread, blob));
    }
  }

  return UserDataAdapter<ChromeBlobStorageContext>::Get(
      context, kBlobStorageContextKeyName);
}

void ChromeBlobStorageContext::InitializeOnIOThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  context_.reset(new BlobStorageContext());
}

scoped_ptr<BlobHandle> ChromeBlobStorageContext::CreateMemoryBackedBlob(
    const char* data, size_t length) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  std::string uuid(base::GenerateGUID());
  scoped_refptr<storage::BlobData> blob_data = new storage::BlobData(uuid);
  blob_data->AppendData(data, length);

  scoped_ptr<storage::BlobDataHandle> blob_data_handle =
      context_->AddFinishedBlob(blob_data.get());
  if (!blob_data_handle)
    return scoped_ptr<BlobHandle>();

  scoped_ptr<BlobHandle> blob_handle(
      new BlobHandleImpl(blob_data_handle.Pass()));
  return blob_handle.Pass();
}

ChromeBlobStorageContext::~ChromeBlobStorageContext() {}

void ChromeBlobStorageContext::DeleteOnCorrectThread() const {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO) &&
      !BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
    return;
  }
  delete this;
}

}  // namespace content
