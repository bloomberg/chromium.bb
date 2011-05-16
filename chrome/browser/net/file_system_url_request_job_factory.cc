// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/file_system_url_request_job_factory.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "webkit/fileapi/file_system_url_request_job.h"
#include "webkit/fileapi/file_system_dir_url_request_job.h"

namespace {

class FileSystemProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  explicit FileSystemProtocolHandler(fileapi::FileSystemContext* context);
  virtual ~FileSystemProtocolHandler();

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request) const OVERRIDE;

 private:
  // No scoped_refptr because |file_system_context_| is owned by the
  // ProfileIOData, which also owns this ProtocolHandler.
  fileapi::FileSystemContext* const file_system_context_;

  DISALLOW_COPY_AND_ASSIGN(FileSystemProtocolHandler);
};

FileSystemProtocolHandler::FileSystemProtocolHandler(
    fileapi::FileSystemContext* context)
    : file_system_context_(context) {
  DCHECK(file_system_context_);
}

FileSystemProtocolHandler::~FileSystemProtocolHandler() {}

net::URLRequestJob* FileSystemProtocolHandler::MaybeCreateJob(
    net::URLRequest* request) const {
  const std::string path = request->url().path();

  // If the path ends with a /, we know it's a directory. If the path refers
  // to a directory and gets dispatched to FileSystemURLRequestJob, that class
  // redirects back here, by adding a / to the URL.
  if (!path.empty() && path[path.size() - 1] == '/') {
    return new fileapi::FileSystemDirURLRequestJob(
        request, file_system_context_,
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
  }
  return new fileapi::FileSystemURLRequestJob(
      request, file_system_context_,
      BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));
}

}  // anonymous namespace

net::URLRequestJobFactory::ProtocolHandler*
CreateFileSystemProtocolHandler(fileapi::FileSystemContext* context) {
  DCHECK(context);
  return new FileSystemProtocolHandler(context);
}
