// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_BLOB_URL_REQUEST_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_MOCK_BLOB_URL_REQUEST_CONTEXT_H_

#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory_impl.h"

namespace fileapi {
class FileSystemContext;
}

namespace webkit_blob {
class BlobDataHandle;
class BlobStorageContext;
}

namespace content {

class MockBlobURLRequestContext : public net::URLRequestContext {
 public:
  MockBlobURLRequestContext(fileapi::FileSystemContext* file_system_context);
  virtual ~MockBlobURLRequestContext();

  webkit_blob::BlobStorageContext* blob_storage_context() const {
    return blob_storage_context_.get();
  }

 private:
  net::URLRequestJobFactoryImpl job_factory_;
  scoped_ptr<webkit_blob::BlobStorageContext> blob_storage_context_;

  DISALLOW_COPY_AND_ASSIGN(MockBlobURLRequestContext);
};

class ScopedTextBlob {
 public:
  // Registers a blob with the given |id| that contains |data|.
  ScopedTextBlob(const MockBlobURLRequestContext& request_context,
                 const std::string& blob_id,
                 const std::string& data);
  ~ScopedTextBlob();

  // Returns a BlobDataHandle referring to the scoped blob.
  scoped_ptr<webkit_blob::BlobDataHandle> GetBlobDataHandle();

 private:
  const std::string blob_id_;
  webkit_blob::BlobStorageContext* context_;
  scoped_ptr<webkit_blob::BlobDataHandle> handle_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTextBlob);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_BLOB_URL_REQUEST_CONTEXT_H_
