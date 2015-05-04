// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/test/test_browser_state.h"

#include "base/files/file_path.h"
#include "base/test/null_task_runner.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"

namespace {

class TestContextURLRequestContextGetter : public net::URLRequestContextGetter {
 public:
  TestContextURLRequestContextGetter()
      : null_task_runner_(new base::NullTaskRunner) {}

  net::URLRequestContext* GetURLRequestContext() override { return &context_; }

  scoped_refptr<base::SingleThreadTaskRunner>
  GetNetworkTaskRunner() const override {
    return null_task_runner_;
  }

 private:
  ~TestContextURLRequestContextGetter() override {}

  net::TestURLRequestContext context_;
  scoped_refptr<base::SingleThreadTaskRunner> null_task_runner_;
};

}  // namespace

namespace web {
TestBrowserState::TestBrowserState() {
}

TestBrowserState::~TestBrowserState() {
}

bool TestBrowserState::IsOffTheRecord() const {
  return false;
}

base::FilePath TestBrowserState::GetStatePath() const {
  return base::FilePath();
}

net::URLRequestContextGetter* TestBrowserState::GetRequestContext() {
  if (!request_context_)
    request_context_ = new TestContextURLRequestContextGetter();
  return request_context_.get();
}

}  // namespace web
