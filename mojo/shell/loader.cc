// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/loader.h"

#include "base/message_loop/message_loop.h"
#include "net/base/network_delegate.h"

namespace mojo {
namespace shell {

Loader::Delegate::~Delegate() {
}

Loader::Job::Job(const GURL& app_url, Delegate* delegate)
    : delegate_(delegate) {
  fetcher_.reset(net::URLFetcher::Create(app_url, net::URLFetcher::GET, this));
}

Loader::Job::~Job() {
}

void Loader::Job::OnURLFetchComplete(const net::URLFetcher* source) {
  base::FilePath app_path;
  source->GetResponseAsFilePath(true, &app_path);
  delegate_->DidCompleteLoad(source->GetURL(), app_path);
}

Loader::Loader(base::SingleThreadTaskRunner* network_runner,
               base::SingleThreadTaskRunner* file_runner,
               base::MessageLoopProxy* cache_runner,
               scoped_ptr<net::NetworkDelegate> network_delegate,
               base::FilePath base_path)
    : file_runner_(file_runner),
      url_request_context_getter_(new URLRequestContextGetter(
          base_path,
          network_runner,
          file_runner,
          cache_runner,
          network_delegate.Pass())) {
}

Loader::~Loader() {
}

scoped_ptr<Loader::Job> Loader::Load(const GURL& app_url, Delegate* delegate) {
  scoped_ptr<Job> job(new Job(app_url, delegate));
  job->fetcher_->SetRequestContext(url_request_context_getter_.get());
  job->fetcher_->SaveResponseToTemporaryFile(file_runner_.get());
  job->fetcher_->Start();
  return job.Pass();
}

}  // namespace shell
}  // namespace mojo
