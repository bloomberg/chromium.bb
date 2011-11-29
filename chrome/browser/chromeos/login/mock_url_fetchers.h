// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_URL_FETCHERS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_URL_FETCHERS_H_
#pragma once

#include <string>

#include "base/memory/weak_ptr.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "content/test/test_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

namespace content {
class URLFetcherDelegate;
}

namespace chromeos {

// Simulates a URL fetch by posting a delayed task. This fetch expects to be
// canceled, and fails the test if it is not
class ExpectCanceledFetcher : public TestURLFetcher {
 public:
  ExpectCanceledFetcher(bool success,
                        const GURL& url,
                        const std::string& results,
                        content::URLFetcher::RequestType request_type,
                        content::URLFetcherDelegate* d);
  virtual ~ExpectCanceledFetcher();

  virtual void Start() OVERRIDE;

  void CompleteFetch();

 private:
  base::WeakPtrFactory<ExpectCanceledFetcher> weak_factory_;
  DISALLOW_COPY_AND_ASSIGN(ExpectCanceledFetcher);
};

class GotCanceledFetcher : public TestURLFetcher {
 public:
  GotCanceledFetcher(bool success,
                     const GURL& url,
                     const std::string& results,
                     content::URLFetcher::RequestType request_type,
                     content::URLFetcherDelegate* d);
  virtual ~GotCanceledFetcher();

  virtual void Start() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(GotCanceledFetcher);
};

class SuccessFetcher : public TestURLFetcher {
 public:
  SuccessFetcher(bool success,
                 const GURL& url,
                 const std::string& results,
                 content::URLFetcher::RequestType request_type,
                 content::URLFetcherDelegate* d);
  virtual ~SuccessFetcher();

  virtual void Start() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(SuccessFetcher);
};

class FailFetcher : public TestURLFetcher {
 public:
  FailFetcher(bool success,
              const GURL& url,
              const std::string& results,
              content::URLFetcher::RequestType request_type,
              content::URLFetcherDelegate* d);
  virtual ~FailFetcher();

  virtual void Start() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(FailFetcher);
};

class CaptchaFetcher : public TestURLFetcher {
 public:
  CaptchaFetcher(bool success,
                 const GURL& url,
                 const std::string& results,
                 content::URLFetcher::RequestType request_type,
                 content::URLFetcherDelegate* d);
  virtual ~CaptchaFetcher();

  static std::string GetCaptchaToken();
  static std::string GetCaptchaUrl();
  static std::string GetUnlockUrl();

  virtual void Start() OVERRIDE;

 private:
  static const char kCaptchaToken[];
  static const char kCaptchaUrlBase[];
  static const char kCaptchaUrlFragment[];
  static const char kUnlockUrl[];
  DISALLOW_COPY_AND_ASSIGN(CaptchaFetcher);
};

class HostedFetcher : public TestURLFetcher {
 public:
  HostedFetcher(bool success,
                const GURL& url,
                const std::string& results,
                content::URLFetcher::RequestType request_type,
                content::URLFetcherDelegate* d);
  virtual ~HostedFetcher();

  virtual void Start() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostedFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_URL_FETCHERS_H_
