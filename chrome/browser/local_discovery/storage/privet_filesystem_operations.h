// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_OPERATIONS_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_OPERATIONS_H_

#include "chrome/browser/local_discovery/privet_device_resolver.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "chrome/browser/local_discovery/storage/path_util.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request_context.h"
#include "webkit/browser/fileapi/async_file_util.h"

namespace local_discovery {

class PrivetFileSystemAsyncOperation {
 public:
  virtual ~PrivetFileSystemAsyncOperation() {}

  virtual void Start() = 0;
};

class PrivetFileSystemAsyncOperationContainer {
 public:
  virtual ~PrivetFileSystemAsyncOperationContainer() {}

  virtual void RemoveOperation(
      PrivetFileSystemAsyncOperation* operation) = 0;
  virtual void RemoveAllOperations() = 0;
};

class PrivetFileSystemAsyncOperationUtil {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // |http_client| is the client for the resolved service, or NULL if
    // resolution failed. |path| is the canonical path within the privet
    // service.
    virtual void PrivetFileSystemResolved(
        PrivetHTTPClient* http_client,
        const std::string& path) = 0;
  };

  // full_path
  PrivetFileSystemAsyncOperationUtil(
      const base::FilePath& full_path,
      net::URLRequestContextGetter* request_context,
      Delegate* delegate);
  ~PrivetFileSystemAsyncOperationUtil();

  void Start();

 private:
  void OnGotDeviceDescription(bool success,
                              const DeviceDescription& device_description);
  void OnGotPrivetHTTP(scoped_ptr<PrivetHTTPClient> privet_http_client);

  ParsedPrivetPath parsed_path_;
  scoped_refptr<net::URLRequestContextGetter> request_context_;
  Delegate* delegate_;

  scoped_refptr<ServiceDiscoverySharedClient> service_discovery_client_;
  scoped_ptr<PrivetDeviceResolver> privet_device_resolver_;
  scoped_ptr<PrivetHTTPAsynchronousFactory> privet_async_factory_;
  scoped_ptr<PrivetHTTPResolution> privet_http_resolution_;
  scoped_ptr<PrivetHTTPClient> privet_client_;
};

class PrivetFileSystemListOperation
    : public PrivetFileSystemAsyncOperationUtil::Delegate,
      public local_discovery::PrivetFileSystemAsyncOperation {
 public:
  PrivetFileSystemListOperation(
      const base::FilePath& full_path,
      content::BrowserContext* browser_context,
      PrivetFileSystemAsyncOperationContainer* async_file_util,
      const fileapi::AsyncFileUtil::ReadDirectoryCallback& callback);
  virtual ~PrivetFileSystemListOperation();

  virtual void Start() OVERRIDE;

  virtual void PrivetFileSystemResolved(
        PrivetHTTPClient* http_client,
        const std::string& path) OVERRIDE;

 private:
  void StartOnUIThread();
  void OnStorageListResult(const base::DictionaryValue* value);
  void TriggerCallback(base::File::Error result,
                       const fileapi::AsyncFileUtil::EntryList& file_list,
                       bool has_more);

  void TriggerCallbackOnIOThread(
      base::File::Error result,
      fileapi::AsyncFileUtil::EntryList file_list,
      bool has_more);

  base::FilePath full_path_;
  content::BrowserContext* browser_context_;
  scoped_ptr<PrivetFileSystemAsyncOperationUtil> core_;
  PrivetFileSystemAsyncOperationContainer* async_file_util_;
  fileapi::AsyncFileUtil::ReadDirectoryCallback callback_;

  scoped_ptr<PrivetJSONOperation> list_operation_;
  base::WeakPtrFactory<PrivetFileSystemListOperation> weak_factory_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_STORAGE_PRIVET_FILESYSTEM_OPERATIONS_H_
