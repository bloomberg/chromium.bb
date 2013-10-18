// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/loader/loader.h"

#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "mojo/loader/url_request_context_getter.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace mojo {
namespace loader {

namespace {

class JobImpl : public net::URLFetcherDelegate, public Job {
 public:
  JobImpl(const GURL& app_url, Job::Delegate* delegate);
  virtual ~JobImpl();

  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  net::URLFetcher* fetcher() const { return fetcher_.get(); }

 private:
  scoped_ptr<net::URLFetcher> fetcher_;
  Job::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(JobImpl);
};

JobImpl::JobImpl(const GURL& app_url, Job::Delegate* delegate)
    : delegate_(delegate) {
  fetcher_.reset(net::URLFetcher::Create(app_url, net::URLFetcher::GET, this));
}

JobImpl::~JobImpl() {
}

void JobImpl::OnURLFetchComplete(const net::URLFetcher* source) {
  delegate_->DidCompleteLoad(source->GetURL());
}

scoped_ptr<base::Thread> CreateIOThread(const char* name) {
  scoped_ptr<base::Thread> thread(new base::Thread(name));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread->StartWithOptions(options);
  return thread.Pass();
}

}  // namespace

class Loader::Data {
 public:
  scoped_ptr<base::Thread> cache_thread;
  scoped_refptr<URLRequestContextGetter> url_request_context_getter;
};

Loader::Loader(base::SingleThreadTaskRunner* network_runner,
               base::FilePath base_path)
    : data_(new Data()) {
  data_->cache_thread = CreateIOThread("cache_thread");
  data_->url_request_context_getter = new URLRequestContextGetter(
      base_path, network_runner, data_->cache_thread->message_loop_proxy());
}

Loader::~Loader() {
}

scoped_ptr<Job> Loader::Load(const GURL& app_url, Job::Delegate* delegate) {
  JobImpl* job = new JobImpl(app_url, delegate);
  job->fetcher()->SetRequestContext(data_->url_request_context_getter.get());
  job->fetcher()->Start();
  return make_scoped_ptr(static_cast<Job*>(job));
}

}  // namespace loader
}  // namespace mojo
