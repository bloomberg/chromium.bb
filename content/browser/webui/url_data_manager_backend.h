// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_BACKEND_H_
#define CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_BACKEND_H_

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/supports_user_data.h"
#include "content/browser/webui/url_data_manager.h"
#include "net/url_request/url_request_job_factory.h"

class ChromeURLDataManagerBackend;
class GURL;
class URLRequestChromeJob;

namespace base {
class RefCountedMemory;
}

namespace net {
class URLRequest;
class URLRequestJob;
}

// ChromeURLDataManagerBackend is used internally by ChromeURLDataManager on the
// IO thread. In most cases you can use the API in ChromeURLDataManager and
// ignore this class. ChromeURLDataManagerBackend is owned by
// ChromeURLRequestContext.
class ChromeURLDataManagerBackend : public base::SupportsUserData::Data {
 public:
  typedef int RequestID;

  ChromeURLDataManagerBackend();
  virtual ~ChromeURLDataManagerBackend();

  // Invoked to create the protocol handler for chrome://. |is_incognito| should
  // be set for incognito profiles.
  static net::URLRequestJobFactory::ProtocolHandler* CreateProtocolHandler(
      ChromeURLDataManagerBackend* backend,
      bool is_incognito);

  // Adds a DataSource to the collection of data sources.
  void AddDataSource(URLDataSourceImpl* source);

  // DataSource invokes this. Sends the data to the URLRequest.
  void DataAvailable(RequestID request_id, base::RefCountedMemory* bytes);

  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

 private:
  friend class URLRequestChromeJob;

  typedef std::map<std::string,
      scoped_refptr<URLDataSourceImpl> > DataSourceMap;
  typedef std::map<RequestID, URLRequestChromeJob*> PendingRequestMap;

  // Called by the job when it's starting up.
  // Returns false if |url| is not a URL managed by this object.
  bool StartRequest(const GURL& url, URLRequestChromeJob* job);

  // Helper function to call StartDataRequest on |source|'s delegate. This is
  // needed because while we want to call URLDataSourceDelegate's method, we
  // need to add a refcount on the source.
  static void CallStartRequest(scoped_refptr<URLDataSourceImpl> source,
                               const std::string& path,
                               bool is_incognito,
                               int request_id);

  // Remove a request from the list of pending requests.
  void RemoveRequest(URLRequestChromeJob* job);

  // Returns true if the job exists in |pending_requests_|. False otherwise.
  // Called by ~URLRequestChromeJob to verify that |pending_requests_| is kept
  // up to date.
  bool HasPendingJob(URLRequestChromeJob* job) const;

  // Custom sources of data, keyed by source path (e.g. "favicon").
  DataSourceMap data_sources_;

  // All pending URLRequestChromeJobs, keyed by ID of the request.
  // URLRequestChromeJob calls into this object when it's constructed and
  // destructed to ensure that the pointers in this map remain valid.
  PendingRequestMap pending_requests_;

  // The ID we'll use for the next request we receive.
  RequestID next_request_id_;

  DISALLOW_COPY_AND_ASSIGN(ChromeURLDataManagerBackend);
};

// Creates protocol handler for chrome-devtools://. |is_incognito| should be
// set for incognito profiles.
net::URLRequestJobFactory::ProtocolHandler*
CreateDevToolsProtocolHandler(ChromeURLDataManagerBackend* backend,
                              bool is_incognito);

#endif  // CONTENT_BROWSER_WEBUI_URL_DATA_MANAGER_BACKEND_H_
