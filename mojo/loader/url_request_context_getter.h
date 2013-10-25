// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_URL_REQUEST_CONTEXT_GETTER_H_
#define MOJO_URL_REQUEST_CONTEXT_GETTER_H_

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop_proxy.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_context_storage.h"

namespace mojo {
namespace loader {

class URLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  URLRequestContextGetter(
      base::FilePath base_path,
      base::SingleThreadTaskRunner* network_task_runner,
      base::MessageLoopProxy* cache_task_runner);

  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetNetworkTaskRunner() const OVERRIDE;

 protected:
  virtual ~URLRequestContextGetter();

 private:
  base::FilePath base_path_;
  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_refptr<base::MessageLoopProxy> cache_task_runner_;
  scoped_ptr<net::NetLog> net_log_;
  scoped_ptr<net::URLRequestContextStorage> storage_;
  scoped_ptr<net::URLRequestContext> url_request_context_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestContextGetter);
};

}  // namespace loader
}  // namespace mojo

#endif  // MOJO_URL_REQUEST_CONTEXT_GETTER_H_
