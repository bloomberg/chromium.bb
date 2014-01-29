// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_DELEGATE_H_
#define CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "net/url_request/url_fetcher_delegate.h"

namespace feedback {

typedef base::Callback<void(const std::string&)> ReportDataCallback;

// FeedbackUploaderDelegate is a simple http uploader for a feedback report. On
// succes or failure, it deletes itself, but on failure it also notifies the
// error callback specified when constructing the class instance.
class FeedbackUploaderDelegate : public net::URLFetcherDelegate {
 public:
  FeedbackUploaderDelegate(const std::string& post_body,
                           const base::Closure& success_callback,
                           const ReportDataCallback& error_callback);
  virtual ~FeedbackUploaderDelegate();

 private:
  // Overridden from net::URLFetcherDelegate.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  std::string post_body_;
  base::Closure success_callback_;
  ReportDataCallback error_callback_;

  DISALLOW_COPY_AND_ASSIGN(FeedbackUploaderDelegate);
};

}  // namespace feedback

#endif  // CHROME_BROWSER_FEEDBACK_FEEDBACK_UPLOADER_DELEGATE_H_
