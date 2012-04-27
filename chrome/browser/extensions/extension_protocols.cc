// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_protocols.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_info_map.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/resource_request_info.h"
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

using content::ResourceRequestInfo;

namespace {

net::HttpResponseHeaders* BuildHttpHeaders(
    const std::string& content_security_policy, bool send_cors_header) {
  std::string raw_headers;
  raw_headers.append("HTTP/1.1 200 OK");
  if (!content_security_policy.empty()) {
    raw_headers.append(1, '\0');
    raw_headers.append("X-WebKit-CSP: ");
    raw_headers.append(content_security_policy);
  }

  if (send_cors_header) {
    raw_headers.append(1, '\0');
    raw_headers.append("Access-Control-Allow-Origin: *");
  }
  raw_headers.append(2, '\0');
  return new net::HttpResponseHeaders(raw_headers);
}

class URLRequestResourceBundleJob : public net::URLRequestSimpleJob {
 public:
  URLRequestResourceBundleJob(
      net::URLRequest* request, const FilePath& filename, int resource_id,
      const std::string& content_security_policy, bool send_cors_header)
      : net::URLRequestSimpleJob(request),
        filename_(filename),
        resource_id_(resource_id) {
    response_info_.headers = BuildHttpHeaders(content_security_policy,
                                              send_cors_header);
  }

  // Overridden from URLRequestSimpleJob:
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const OVERRIDE {
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

class GeneratedBackgroundPageJob : public net::URLRequestSimpleJob {
 public:
  GeneratedBackgroundPageJob(net::URLRequest* request,
                             const scoped_refptr<const Extension> extension,
                             const std::string& content_security_policy)
      : net::URLRequestSimpleJob(request),
        extension_(extension) {
    const bool send_cors_headers = false;
    response_info_.headers = BuildHttpHeaders(content_security_policy,
                                              send_cors_headers);
  }

  // Overridden from URLRequestSimpleJob:
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const OVERRIDE {
    *mime_type = "text/html";
    *charset = "utf-8";

    *data = "<!DOCTYPE html>\n<body>\n";
    for (size_t i = 0; i < extension_->background_scripts().size(); ++i) {
      *data += "<script src=\"";
      *data += extension_->background_scripts()[i];
      *data += "\"></script>\n";
    }

    return true;
  }

  virtual void GetResponseInfo(net::HttpResponseInfo* info) {
    *info = response_info_;
  }

 private:
  virtual ~GeneratedBackgroundPageJob() {}

  scoped_refptr<const Extension> extension_;
  net::HttpResponseInfo response_info_;
};

class URLRequestExtensionJob : public net::URLRequestFileJob {
 public:
  URLRequestExtensionJob(net::URLRequest* request,
                         const FilePath& filename,
                         const std::string& content_security_policy,
                         bool send_cors_header)
    : net::URLRequestFileJob(request, filename) {
      response_info_.headers = BuildHttpHeaders(content_security_policy,
                                                send_cors_header);
  }

  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE {
    *info = response_info_;
  }

 private:
  virtual ~URLRequestExtensionJob() {}

  net::HttpResponseInfo response_info_;
};

bool ExtensionCanLoadInIncognito(const ResourceRequestInfo* info,
                                 const std::string& extension_id,
                                 ExtensionInfoMap* extension_info_map) {
  if (!extension_info_map->IsIncognitoEnabled(extension_id))
    return false;

  // Only allow incognito toplevel navigations to extension resources in
  // split mode. In spanning mode, the extension must run in a single process,
  // and an incognito tab prevents that.
  if (info->GetResourceType() == ResourceType::MAIN_FRAME) {
    const Extension* extension =
        extension_info_map->extensions().GetByID(extension_id);
    return extension && extension->incognito_split_mode();
  }

  return true;
}

// Returns true if an chrome-extension:// resource should be allowed to load.
// TODO(aa): This should be moved into ExtensionResourceRequestPolicy, but we
// first need to find a way to get CanLoadInIncognito state into the renderers.
bool AllowExtensionResourceLoad(net::URLRequest* request,
                                bool is_incognito,
                                ExtensionInfoMap* extension_info_map) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  // We have seen crashes where info is NULL: crbug.com/52374.
  if (!info) {
    LOG(ERROR) << "Allowing load of " << request->url().spec()
               << "from unknown origin. Could not find user data for "
               << "request.";
    return true;
  }

  if (is_incognito && !ExtensionCanLoadInIncognito(info, request->url().host(),
                                                   extension_info_map)) {
    return false;
  }

  return true;
}

// Returns true if the given URL references an icon in the given extension.
bool URLIsForExtensionIcon(const GURL& url, const Extension* extension) {
  DCHECK(url.SchemeIs(chrome::kExtensionScheme));

  if (!extension)
    return false;

  std::string path = url.path();
  DCHECK_EQ(url.host(), extension->id());
  DCHECK(path.length() > 0 && path[0] == '/');
  path = path.substr(1);
  return extension->icons().ContainsPath(path);
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
    return new net::URLRequestErrorJob(request, net::ERR_ADDRESS_UNREACHABLE);
  }

  // chrome-extension://extension-id/resource/path.js
  const std::string& extension_id = request->url().host();
  const Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);
  FilePath directory_path;
  if (extension)
    directory_path = extension->path();
  if (directory_path.value().empty()) {
    const Extension* disabled_extension =
        extension_info_map_->disabled_extensions().GetByID(extension_id);
    if (URLIsForExtensionIcon(request->url(), disabled_extension))
      directory_path = disabled_extension->path();
    if (directory_path.value().empty()) {
      LOG(WARNING) << "Failed to GetPathForExtension: " << extension_id;
      return NULL;
    }
  }

  std::string content_security_policy;
  bool send_cors_header = false;
  if (extension) {
    content_security_policy = extension->content_security_policy();
    if ((extension->manifest_version() >= 2 ||
             extension->HasWebAccessibleResources()) &&
        extension->IsResourceWebAccessible(request->url().path()))
      send_cors_header = true;
  }

  std::string path = request->url().path();
  if (path.size() > 1 &&
      path.substr(1) == extension_filenames::kGeneratedBackgroundPageFilename) {
    return new GeneratedBackgroundPageJob(
        request, extension, content_security_policy);
  }

  FilePath resources_path;
  if (PathService::Get(chrome::DIR_RESOURCES, &resources_path) &&
      directory_path.DirName() == resources_path) {
    FilePath relative_path = directory_path.BaseName().Append(
        extension_file_util::ExtensionURLToRelativeFilePath(request->url()));
    relative_path = relative_path.NormalizePathSeparators();

    // TODO(tc): Make a map of FilePath -> resource ids so we don't have to
    // covert to FilePaths all the time.  This will be more useful as we add
    // more resources.
    for (size_t i = 0; i < kComponentExtensionResourcesSize; ++i) {
      FilePath bm_resource_path =
          FilePath().AppendASCII(kComponentExtensionResources[i].name);
      bm_resource_path = bm_resource_path.NormalizePathSeparators();
      if (relative_path == bm_resource_path) {
        return new URLRequestResourceBundleJob(request, relative_path,
            kComponentExtensionResources[i].value, content_security_policy,
            send_cors_header);
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
                                    content_security_policy, send_cors_header);
}

}  // namespace

net::URLRequestJobFactory::ProtocolHandler* CreateExtensionProtocolHandler(
    bool is_incognito,
    ExtensionInfoMap* extension_info_map) {
  return new ExtensionProtocolHandler(is_incognito, extension_info_map);
}
