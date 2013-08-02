// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_URL_REQUEST_CONTEXT_GETTER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_URL_REQUEST_CONTEXT_GETTER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "net/url_request/url_request_context_getter.h"

// Used to return a dummy context, which lives on the message loop
// given in the constructor.
class CloudPrintURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  // |task_runner| must not be NULL.
  explicit CloudPrintURLRequestContextGetter(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // URLRequestContextGetter implementation.
  virtual net::URLRequestContext* GetURLRequestContext() OVERRIDE;

  virtual scoped_refptr<base::SingleThreadTaskRunner>
    GetNetworkTaskRunner() const OVERRIDE;

 private:
  virtual ~CloudPrintURLRequestContextGetter();

  scoped_refptr<base::SingleThreadTaskRunner> network_task_runner_;
  scoped_ptr<net::URLRequestContext> context_;

  DISALLOW_COPY_AND_ASSIGN(CloudPrintURLRequestContextGetter);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_URL_REQUEST_CONTEXT_GETTER_H_

