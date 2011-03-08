// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FILE_SYSTEM_WEBFILEWRITER_IMPL_H_
#define CONTENT_COMMON_FILE_SYSTEM_WEBFILEWRITER_IMPL_H_

#include "base/ref_counted.h"
#include "base/weak_ptr.h"
#include "webkit/fileapi/webfilewriter_base.h"

class FileSystemDispatcher;

// An implementation of WebFileWriter for use in chrome renderers and workers.
class WebFileWriterImpl : public fileapi::WebFileWriterBase,
                          public base::SupportsWeakPtr<WebFileWriterImpl> {
 public:
  WebFileWriterImpl(
      const WebKit::WebString& path, WebKit::WebFileWriterClient* client);
  virtual ~WebFileWriterImpl();

 protected:
  // WebFileWriterBase overrides
  virtual void DoTruncate(const FilePath& path, int64 offset);
  virtual void DoWrite(const FilePath& path, const GURL& blob_url,
                       int64 offset);
  virtual void DoCancel();

 private:
  class CallbackDispatcher;
  int request_id_;
};

#endif  // CONTENT_COMMON_FILE_SYSTEM_WEBFILEWRITER_IMPL_H_
