// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_FILE_SYSTEM_WEBFILEWRITER_IMPL_H_
#define CHROME_COMMON_FILE_SYSTEM_WEBFILEWRITER_IMPL_H_

#include "base/basictypes.h"
#include "base/weak_ptr.h"
#include "chrome/common/file_system/file_system_dispatcher.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFileWriter.h"

namespace WebKit {
class WebFileWriterClient;
class WebString;
class WebURL;
}

class WebFileWriterImpl
  : public WebKit::WebFileWriter,
    public base::SupportsWeakPtr<WebFileWriterImpl> {
 public:
  WebFileWriterImpl(
      const WebKit::WebString& path, WebKit::WebFileWriterClient* client);
  virtual ~WebFileWriterImpl();

  // WebFileWriter implementation
  virtual void truncate(long long length);
  virtual void write(long long position, const WebKit::WebURL& blobURL);
  virtual void cancel();

 private:
  class FileSystemCallbackDispatcherImpl;
  enum OperationType {
    kOperationNone,
    kOperationWrite,
    kOperationTruncate
  };

  enum CancelState {
    kCancelNotInProgress,
    kCancelSent,
    kCancelReceivedWriteResponse,
  };

  void DidSucceed();
  void DidFail(base::PlatformFileError error_code);
  void DidWrite(int64 bytes, bool complete);
  void FinishCancel();

  FilePath path_;
  WebKit::WebFileWriterClient* client_;
  int request_id_;
  OperationType operation_;
  CancelState cancel_state_;
};

#endif  // CHROME_COMMON_FILE_SYSTEM_WEBFILEWRITER_IMPL_H_
