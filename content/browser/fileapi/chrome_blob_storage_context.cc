// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/fileapi/chrome_blob_storage_context.h"

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/browser/blob/blob_storage_context.h"

using base::UserDataAdapter;
using webkit_blob::BlobStorageContext;

namespace content {

static const char* kBlobStorageContextKeyName = "content_blob_storage_context";

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
