// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_CHROME_URL_DATA_MANAGER_BACKEND_H_
#define CHROME_BROWSER_WEBUI_CHROME_URL_DATA_MANAGER_BACKEND_H_
#pragma once

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/webui/chrome_url_data_manager.h"

#include <map>
#include <string>
#include <vector>

class FilePath;
class GURL;
class URLRequestChromeJob;

namespace net {
class URLRequest;
class URLRequestJob;
}

// ChromeURLDataManagerBackend is used internally by ChromeURLDataManager on the
// IO thread. In most cases you can use the API in ChromeURLDataManager and
// ignore this class. ChromeURLDataManagerBackend is owned by
// ChromeURLRequestContext.
class ChromeURLDataManagerBackend {
 public:
  typedef int RequestID;

  ChromeURLDataManagerBackend();
  ~ChromeURLDataManagerBackend();

  // Invoked to register the protocol factories.
  static void Register();

  // Adds a DataSource to the collection of data sources.
  void AddDataSource(ChromeURLDataManager::DataSource* source);

  // Add/remove a path from the collection of file sources.
  // A file source acts like a file:// URL to the specified path.
  // Calling this from threads other the IO thread must be done via
  // InvokeLater.
  void AddFileSource(const std::string& source_name, const FilePath& path);

  // DataSource invokes this. Sends the data to the URLRequest.
  void DataAvailable(RequestID request_id, RefCountedMemory* bytes);

  static net::URLRequestJob* Factory(net::URLRequest* request,
                                     const std::string& scheme);

 private:
  friend class URLRequestChromeJob;

  typedef std::map<std::string,
      scoped_refptr<ChromeURLDataManager::DataSource> > DataSourceMap;
  typedef std::map<std::string, FilePath> FileSourceMap;
  typedef std::map<RequestID, URLRequestChromeJob*> PendingRequestMap;

  // Parse a URL into the components used to resolve its request.
  void URLToRequest(const GURL& url, std::string* source, std::string* path);

  // Translate a chrome resource URL into a local file path if there is one.
  // Returns false if there is no file handler for this URL
  bool URLToFilePath(const GURL& url, FilePath* file_path);

  // Called by the job when it's starting up.
  // Returns false if |url| is not a URL managed by this object.
  bool StartRequest(const GURL& url, URLRequestChromeJob* job);

  // Remove a request from the list of pending requests.
  void RemoveRequest(URLRequestChromeJob* job);

  // Returns true if the job exists in |pending_requests_|. False otherwise.
  // Called by ~URLRequestChromeJob to verify that |pending_requests_| is kept
  // up to date.
  bool HasPendingJob(URLRequestChromeJob* job) const;

  // File sources of data, keyed by source name (e.g. "inspector").
  FileSourceMap file_sources_;

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

#endif  // CHROME_BROWSER_WEBUI_CHROME_URL_DATA_MANAGER_BACKEND_H_
