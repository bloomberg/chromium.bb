// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FILE_SYSTEM_WEBFILEWRITER_IMPL_H_
#define CONTENT_COMMON_FILE_SYSTEM_WEBFILEWRITER_IMPL_H_

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "webkit/fileapi/webfilewriter_base.h"

// An implementation of WebFileWriter for use in chrome renderers and workers.
class WebFileWriterImpl : public fileapi::WebFileWriterBase,
                          public base::SupportsWeakPtr<WebFileWriterImpl> {
 public:
  WebFileWriterImpl(const GURL& path, WebKit::WebFileWriterClient* client);
  virtual ~WebFileWriterImpl();

 protected:
  // WebFileWriterBase overrides
  virtual void DoTruncate(const GURL& path, int64 offset) OVERRIDE;
  virtual void DoWrite(const GURL& path, const GURL& blob_url,
                       int64 offset) OVERRIDE;
  virtual void DoCancel() OVERRIDE;

 private:
  class CallbackDispatcher;
  int request_id_;
};

#endif  // CONTENT_COMMON_FILE_SYSTEM_WEBFILEWRITER_IMPL_H_
