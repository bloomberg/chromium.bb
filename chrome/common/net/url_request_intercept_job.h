// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NET_URL_REQUEST_INTERCEPT_JOB_H_
#define CHROME_COMMON_NET_URL_REQUEST_INTERCEPT_JOB_H_
#pragma once

#include <string>

#include "base/scoped_ptr.h"
#include "base/task.h"
#include "net/url_request/url_request_job.h"
#include "chrome/common/chrome_plugin_api.h"
#include "chrome/common/chrome_plugin_util.h"
#include "chrome/common/notification_registrar.h"

namespace net {
class URLRequest;
}  // namespace net

class ChromePluginLib;

// A request job that handles network requests intercepted by a Chrome plugin.
class URLRequestInterceptJob : public net::URLRequestJob,
                               public NotificationObserver {
 public:
  static URLRequestInterceptJob* FromCPRequest(CPRequest* request) {
    return ScopableCPRequest::GetData<URLRequestInterceptJob*>(request);
  }

  URLRequestInterceptJob(net::URLRequest* request, ChromePluginLib* plugin,
                         ScopableCPRequest* cprequest);
  virtual ~URLRequestInterceptJob();

  // Plugin callbacks.
  void OnStartCompleted(int result);
  void OnReadCompleted(int bytes_read);

  // net::URLRequestJob
  virtual void Start();
  virtual void Kill();
  virtual bool GetMimeType(std::string* mime_type) const;
  virtual bool GetCharset(std::string* charset);
  virtual void GetResponseInfo(net::HttpResponseInfo* info);
  virtual int GetResponseCode() const;
  virtual bool GetContentEncodings(
      std::vector<net::Filter::FilterType>* encoding_types);
  virtual bool IsRedirectResponse(GURL* location, int* http_status_code);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 protected:
  virtual bool ReadRawData(net::IOBuffer* buf, int buf_size, int* bytes_read);

 private:
  void StartAsync();
  void DetachPlugin();

  NotificationRegistrar registrar_;
  scoped_ptr<ScopableCPRequest> cprequest_;
  ChromePluginLib* plugin_;
  net::IOBuffer* read_buffer_;
  int read_buffer_size_;
  ScopedRunnableMethodFactory<URLRequestInterceptJob> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestInterceptJob);
};

#endif  // CHROME_COMMON_NET_URL_REQUEST_INTERCEPT_JOB_H_
