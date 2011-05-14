// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_protocols.h"

#include <algorithm>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "googleurl/src/url_util.h"
#include "grit/component_extension_resources_map.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_info.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_simple_job.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

net::HttpResponseHeaders* BuildHttpHeaders(
    const std::string& content_security_policy) {
  std::string raw_headers;
  raw_headers.append("HTTP/1.1 200 OK");
  if (!content_security_policy.empty()) {
    raw_headers.append(1, '\0');
    raw_headers.append("X-WebKit-CSP: ");
    raw_headers.append(content_security_policy);
  }
  raw_headers.append(2, '\0');
  return new net::HttpResponseHeaders(raw_headers);
}

class URLRequestResourceBundleJob : public net::URLRequestSimpleJob {
 public:
  URLRequestResourceBundleJob(
      net::URLRequest* request, const FilePath& filename, int resource_id,
      const std::string& content_security_policy)
      : net::URLRequestSimpleJob(request),
        filename_(filename),
        resource_id_(resource_id) {
    response_info_.headers = BuildHttpHeaders(content_security_policy);
  }

  // Overridden from URLRequestSimpleJob:
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const {
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    *data = rb.GetRawDataResource(resource_id_).as_string();

    // Requests should not block on the disk!  On Windows this goes to the
    // registry.
    //   http://code.google.com/p/chromium/issues/detail?id=59849
    bool result;
    {
      base::ThreadRestrictions::ScopedAllowIO allow_io;
      result = net::GetMimeTypeFromFile(filename_, mime_type);
    }

    if (StartsWithASCII(*mime_type, "text/", false)) {
      // All of our HTML files should be UTF-8 and for other resource types
      // (like images), charset doesn't matter.
      DCHECK(IsStringUTF8(*data));
      *charset = "utf-8";
    }
    return result;
  }

  virtual void GetResponseInfo(net::HttpResponseInfo* info) {
    *info = response_info_;
  }

 private:
  virtual ~URLRequestResourceBundleJob() { }

  // We need the filename of the resource to determine the mime type.
  FilePath filename_;

  // The resource bundle id to load.
  int resource_id_;

  net::HttpResponseInfo response_info_;
};

class URLRequestExtensionJob : public net::URLRequestFileJob {
 public:
  URLRequestExtensionJob(net::URLRequest* request,
                         const FilePath& filename,
                         const std::string& content_security_policy)
    : net::URLRequestFileJob(request, filename) {
    response_info_.headers = BuildHttpHeaders(content_security_policy);
  }

  virtual void GetResponseInfo(net::HttpResponseInfo* info) {
    *info = response_info_;
  }

  net::HttpResponseInfo response_info_;
};

// Returns true if an chrome-extension:// resource should be allowed to load.
// TODO(aa): This should be moved into ExtensionResourceRequestPolicy, but we
// first need to find a way to get CanLoadInIncognito state into the renderers.
bool AllowExtensionResourceLoad(net::URLRequest* request,
                                bool is_incognito,
                                ExtensionInfoMap* extension_info_map) {
  const ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);

  // We have seen crashes where info is NULL: crbug.com/52374.
  if (!info) {
    LOG(ERROR) << "Allowing load of " << request->url().spec()
               << "from unknown origin. Could not find user data for "
               << "request.";
    return true;
  }

  // Don't allow toplevel navigations to extension resources in incognito mode.
  // This is because an extension must run in a single process, and an
  // incognito tab prevents that.
  if (is_incognito &&
      info->resource_type() == ResourceType::MAIN_FRAME &&
      !extension_info_map->ExtensionCanLoadInIncognito(request->url().host())) {
    LOG(ERROR) << "Denying load of " << request->url().spec() << " from "
               << "incognito tab.";
    return false;
  }

  return true;
}

class ExtensionProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ExtensionProtocolHandler(bool is_incognito,
                           ExtensionInfoMap* extension_info_map)
      : is_incognito_(is_incognito),
        extension_info_map_(extension_info_map) {}

  virtual ~ExtensionProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request) const OVERRIDE;

 private:
  const bool is_incognito_;
  ExtensionInfoMap* const extension_info_map_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionProtocolHandler);
};

// Creates URLRequestJobs for extension:// URLs.
net::URLRequestJob*
ExtensionProtocolHandler::MaybeCreateJob(net::URLRequest* request) const {
  // TODO(mpcomplete): better error code.
  if (!AllowExtensionResourceLoad(
           request, is_incognito_, extension_info_map_)) {
    LOG(ERROR) << "disallowed in extension protocols";
    return new net::URLRequestErrorJob(request, net::ERR_ADDRESS_UNREACHABLE);
  }

  // chrome-extension://extension-id/resource/path.js
  const std::string& extension_id = request->url().host();
  FilePath directory_path = extension_info_map_->
      GetPathForExtension(extension_id);
  if (directory_path.value().empty()) {
    if (extension_info_map_->URLIsForExtensionIcon(request->url()))
      directory_path = extension_info_map_->
          GetPathForDisabledExtension(extension_id);
    if (directory_path.value().empty()) {
      LOG(WARNING) << "Failed to GetPathForExtension: " << extension_id;
      return NULL;
    }
  }

  const std::string& content_security_policy = extension_info_map_->
      GetContentSecurityPolicyForExtension(extension_id);

  FilePath resources_path;
  if (PathService::Get(chrome::DIR_RESOURCES, &resources_path) &&
      directory_path.DirName() == resources_path) {
    FilePath relative_path = directory_path.BaseName().Append(
        extension_file_util::ExtensionURLToRelativeFilePath(request->url()));
#if defined(OS_WIN)
    relative_path = relative_path.NormalizeWindowsPathSeparators();
#endif

    // TODO(tc): Make a map of FilePath -> resource ids so we don't have to
    // covert to FilePaths all the time.  This will be more useful as we add
    // more resources.
    for (size_t i = 0; i < kComponentExtensionResourcesSize; ++i) {
      FilePath bm_resource_path =
          FilePath().AppendASCII(kComponentExtensionResources[i].name);
#if defined(OS_WIN)
      bm_resource_path = bm_resource_path.NormalizeWindowsPathSeparators();
#endif
      if (relative_path == bm_resource_path) {
        return new URLRequestResourceBundleJob(request, relative_path,
            kComponentExtensionResources[i].value, content_security_policy);
      }
    }
  }
  // TODO(tc): Move all of these files into resources.pak so we don't break
  // when updating on Linux.
  ExtensionResource resource(extension_id, directory_path,
      extension_file_util::ExtensionURLToRelativeFilePath(request->url()));

  FilePath resource_file_path;
  {
    // Getting the file path will touch the file system.  Fixing
    // crbug.com/59849 would also fix this.  Suppress the error for now.
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    resource_file_path = resource.GetFilePath();
  }

  return new URLRequestExtensionJob(request, resource_file_path,
                                    content_security_policy);
}

class UserScriptProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  UserScriptProtocolHandler(const FilePath& user_script_dir_path,
                            ExtensionInfoMap* extension_info_map)
      : user_script_dir_path_(user_script_dir_path),
        extension_info_map_(extension_info_map) {}

  virtual ~UserScriptProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request) const OVERRIDE;

 private:
  const FilePath user_script_dir_path_;
  ExtensionInfoMap* const extension_info_map_;
};

// Factory registered with net::URLRequest to create URLRequestJobs for
// chrome-user-script:/ URLs.
net::URLRequestJob* UserScriptProtocolHandler::MaybeCreateJob(
    net::URLRequest* request) const {
  // chrome-user-script:/user-script-name.user.js
  ExtensionResource resource(
      request->url().host(), user_script_dir_path_,
      extension_file_util::ExtensionURLToRelativeFilePath(request->url()));

  return new net::URLRequestFileJob(request, resource.GetFilePath());
}

}  // namespace

net::URLRequestJobFactory::ProtocolHandler* CreateExtensionProtocolHandler(
    bool is_incognito,
    ExtensionInfoMap* extension_info_map) {
  return new ExtensionProtocolHandler(is_incognito, extension_info_map);
}

net::URLRequestJobFactory::ProtocolHandler* CreateUserScriptProtocolHandler(
    const FilePath& user_script_dir_path,
    ExtensionInfoMap* extension_info_map) {
  return new UserScriptProtocolHandler(
      user_script_dir_path, extension_info_map);
}
