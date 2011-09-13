// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_URL_FETCHERS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_URL_FETCHERS_H_
#pragma once

#include <string>

#include "base/message_loop.h"
#include "base/task.h"
#include "content/common/net/url_fetcher.h"
#include "googleurl/src/gurl.h"
#include "net/url_request/url_request_status.h"

namespace chromeos {

// Simulates a URL fetch by posting a delayed task.  This fetch expects to be
// canceled, and fails the test if it is not
class ExpectCanceledFetcher : public URLFetcher {
 public:
  ExpectCanceledFetcher(bool success,
                        const GURL& url,
                        const std::string& results,
                        URLFetcher::RequestType request_type,
                        URLFetcher::Delegate* d);
  virtual ~ExpectCanceledFetcher();

  virtual void Start();

  void CompleteFetch();

 private:
  ScopedRunnableMethodFactory<ExpectCanceledFetcher> complete_fetch_factory_;
  DISALLOW_COPY_AND_ASSIGN(ExpectCanceledFetcher);
};

class GotCanceledFetcher : public URLFetcher {
 public:
  GotCanceledFetcher(bool success,
                     const GURL& url,
                     const std::string& results,
                     URLFetcher::RequestType request_type,
                     URLFetcher::Delegate* d);
  virtual ~GotCanceledFetcher();

  virtual void Start();

 private:
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(GotCanceledFetcher);
};

class SuccessFetcher : public URLFetcher {
 public:
  SuccessFetcher(bool success,
                 const GURL& url,
                 const std::string& results,
                 URLFetcher::RequestType request_type,
                 URLFetcher::Delegate* d);
  virtual ~SuccessFetcher();

  virtual void Start();

 private:
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(SuccessFetcher);
};

class FailFetcher : public URLFetcher {
 public:
  FailFetcher(bool success,
              const GURL& url,
              const std::string& results,
              URLFetcher::RequestType request_type,
              URLFetcher::Delegate* d);
  virtual ~FailFetcher();

  virtual void Start();

 private:
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(FailFetcher);
};

class CaptchaFetcher : public URLFetcher {
 public:
  CaptchaFetcher(bool success,
                 const GURL& url,
                 const std::string& results,
                 URLFetcher::RequestType request_type,
                 URLFetcher::Delegate* d);
  virtual ~CaptchaFetcher();

  static std::string GetCaptchaToken();
  static std::string GetCaptchaUrl();
  static std::string GetUnlockUrl();

  virtual void Start();

 private:
  static const char kCaptchaToken[];
  static const char kCaptchaUrlBase[];
  static const char kCaptchaUrlFragment[];
  static const char kUnlockUrl[];
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(CaptchaFetcher);
};

class HostedFetcher : public URLFetcher {
 public:
  HostedFetcher(bool success,
                const GURL& url,
                const std::string& results,
                URLFetcher::RequestType request_type,
                URLFetcher::Delegate* d);
  virtual ~HostedFetcher();

  virtual void Start();

 private:
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(HostedFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MOCK_URL_FETCHERS_H_
