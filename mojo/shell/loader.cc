// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/loader.h"

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/switches.h"
#include "net/base/load_flags.h"
#include "net/base/network_delegate.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_status.h"

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
  net::URLRequestStatus status = source->GetStatus();
  if (!status.is_success()) {
    LOG(ERROR) << "URL fetch didn't succeed: status = " << status.status()
               << ", error = " << status.error();
  } else if (source->GetResponseCode() != 200) {
    LOG(ERROR) << "HTTP response not OK: code = " << source->GetResponseCode();
  }
  // TODO: Do something else in the error cases?

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
#if defined(MOJO_SHELL_DEBUG)
  base::FilePath tmp_dir;
  base::GetTempDir(&tmp_dir);
  // If MOJO_SHELL_DEBUG is set we want to dowload to a well known location.
  // This makes it easier to do the necessary links so that symbols are found.
  job->fetcher_->SaveResponseToFileAtPath(
      tmp_dir.Append("link-me"),
      file_runner_.get());
#else
  job->fetcher_->SaveResponseToTemporaryFile(file_runner_.get());
#endif
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDisableCache))
    job->fetcher_->SetLoadFlags(net::LOAD_DISABLE_CACHE);
  job->fetcher_->Start();
  return job.Pass();
}

}  // namespace shell
}  // namespace mojo
