// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_resource_protocols.h"

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_file_job.h"

namespace {

class ExtensionResourcesJob : public net::URLRequestFileJob {
 public:
  explicit ExtensionResourcesJob(net::URLRequest* request)
    : net::URLRequestFileJob(request, FilePath()) { }

  virtual void Start() OVERRIDE;

 protected:
  ~ExtensionResourcesJob() { }

  void ResolvePath();
  void ResolvePathDone();

 private:
  content::BrowserThread::ID thread_id_;
};

void ExtensionResourcesJob::Start() {
  bool result =
      content::BrowserThread::GetCurrentThreadIdentifier(&thread_id_);
  CHECK(result) << "Can not get thread id.";
  content::BrowserThread::PostTask(
      content::BrowserThread::FILE, FROM_HERE,
      base::Bind(&ExtensionResourcesJob::ResolvePath, this));
}

void ExtensionResourcesJob::ResolvePath() {
  FilePath root_path;
  PathService::Get(chrome::DIR_RESOURCES_EXTENSION, &root_path);
  file_path_ = extension_file_util::ExtensionResourceURLToFilePath(
      request()->url(), root_path);
  content::BrowserThread::PostTask(
      thread_id_, FROM_HERE,
      base::Bind(&ExtensionResourcesJob::ResolvePathDone, this));
}

void ExtensionResourcesJob::ResolvePathDone() {
  net::URLRequestFileJob::Start();
}

class ExtensionResourceProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ExtensionResourceProtocolHandler() {}
  virtual ~ExtensionResourceProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request) const OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionResourceProtocolHandler);
};

// Creates URLRequestJobs for chrome-extension-resource:// URLs.
net::URLRequestJob*
ExtensionResourceProtocolHandler::MaybeCreateJob(
    net::URLRequest* request) const {
  return new ExtensionResourcesJob(request);
}

}  // namespace

net::URLRequestJobFactory::ProtocolHandler*
CreateExtensionResourceProtocolHandler() {
  return new ExtensionResourceProtocolHandler();
}
