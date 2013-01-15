// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_FILTER_H_

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/common/process_type.h"
#include "webkit/glue/resource_type.h"

namespace fileapi {
class FileSystemContext;
}  // namespace fileapi

namespace net {
class URLRequestContext;
}  // namespace net


namespace content {
class ChromeAppCacheService;
class ChromeBlobStorageContext;
class ResourceContext;

// This class filters out incoming IPC messages for network requests and
// processes them on the IPC thread.  As a result, network requests are not
// delayed by costly UI processing that may be occuring on the main thread of
// the browser.  It also means that any hangs in starting a network request
// will not interfere with browser UI.
class CONTENT_EXPORT ResourceMessageFilter : public BrowserMessageFilter {
 public:
  // Allows selecting the net::URLRequestContext used to service requests.
  class URLRequestContextSelector {
   public:
    URLRequestContextSelector() {}
    virtual ~URLRequestContextSelector() {}

    virtual net::URLRequestContext* GetRequestContext(
        ResourceType::Type request_type) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(URLRequestContextSelector);
  };

  ResourceMessageFilter(
      int child_id,
      ProcessType process_type,
      ResourceContext* resource_context,
      ChromeAppCacheService* appcache_service,
      ChromeBlobStorageContext* blob_storage_context,
      fileapi::FileSystemContext* file_system_context,
      URLRequestContextSelector* url_request_context_selector);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  ResourceContext* resource_context() const {
    return resource_context_;
  }

  ChromeAppCacheService* appcache_service() const {
    return appcache_service_;
  }

  ChromeBlobStorageContext* blob_storage_context() const {
    return blob_storage_context_;
  }

  fileapi::FileSystemContext* file_system_context() const {
    return file_system_context_;
  }

  // Returns the net::URLRequestContext for the given request.
  net::URLRequestContext* GetURLRequestContext(
      ResourceType::Type request_type);

  int child_id() const { return child_id_; }
  ProcessType process_type() const { return process_type_; }

 protected:
  // Protected destructor so that we can be overriden in tests.
  virtual ~ResourceMessageFilter();

 private:
  // The ID of the child process.
  int child_id_;

  ProcessType process_type_;

  // Owned by ProfileIOData* which is guaranteed to outlive us.
  ResourceContext* resource_context_;

  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  scoped_refptr<fileapi::FileSystemContext> file_system_context_;

  const scoped_ptr<URLRequestContextSelector> url_request_context_selector_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ResourceMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_RESOURCE_MESSAGE_FILTER_H_
