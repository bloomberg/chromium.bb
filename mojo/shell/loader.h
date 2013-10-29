// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_LOADER_H_
#define MOJO_SHELL_LOADER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "mojo/shell/url_request_context_getter.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

namespace net {
class NetworkDelegate;
}

namespace mojo {
namespace shell {

class Loader {
 public:
  class Delegate {
   public:
    virtual void DidCompleteLoad(const GURL& app_url,
                                 const base::FilePath& app_path) = 0;

   protected:
    virtual ~Delegate();
  };

  class Job : public net::URLFetcherDelegate {
   public:
    // You can cancel a job by deleting it.
    virtual ~Job();

   private:
    friend class Loader;

    // You can create a job using Loader::Load.
    Job(const GURL& app_url, Delegate* delegate);
    virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

    scoped_ptr<net::URLFetcher> fetcher_;
    Delegate* delegate_;

    DISALLOW_COPY_AND_ASSIGN(Job);
  };

  Loader(base::SingleThreadTaskRunner* network_runner,
         base::SingleThreadTaskRunner* file_runner,
         scoped_ptr<net::NetworkDelegate> network_delegate,
         base::FilePath base_path);
  ~Loader();

  scoped_ptr<Job> Load(const GURL& app_url, Delegate* delegate);

 private:
  scoped_refptr<base::SingleThreadTaskRunner> file_runner_;
  scoped_ptr<base::Thread> cache_thread_;
  scoped_refptr<URLRequestContextGetter> url_request_context_getter_;

  DISALLOW_COPY_AND_ASSIGN(Loader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_LOADER_H_
