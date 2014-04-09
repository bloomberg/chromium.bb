// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_protocols.h"

#include <algorithm>

#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/path_service.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_renderer_state.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/csp_info.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/manifest_handlers/shared_module_info.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "grit/component_extension_resources_map.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_simple_job.h"
#include "ui/base/resource/resource_bundle.h"
#include "url/url_util.h"

using content::BrowserThread;
using content::ResourceRequestInfo;
using extensions::Extension;
using extensions::SharedModuleInfo;

namespace {

net::HttpResponseHeaders* BuildHttpHeaders(
    const std::string& content_security_policy, bool send_cors_header,
    const base::Time& last_modified_time) {
  std::string raw_headers;
  raw_headers.append("HTTP/1.1 200 OK");
  if (!content_security_policy.empty()) {
    raw_headers.append(1, '\0');
    raw_headers.append("Content-Security-Policy: ");
    raw_headers.append(content_security_policy);
  }

  if (send_cors_header) {
    raw_headers.append(1, '\0');
    raw_headers.append("Access-Control-Allow-Origin: *");
  }

  if (!last_modified_time.is_null()) {
    // Hash the time and make an etag to avoid exposing the exact
    // user installation time of the extension.
    std::string hash = base::StringPrintf("%" PRId64,
                                          last_modified_time.ToInternalValue());
    hash = base::SHA1HashString(hash);
    std::string etag;
    base::Base64Encode(hash, &etag);
    raw_headers.append(1, '\0');
    raw_headers.append("ETag: \"");
    raw_headers.append(etag);
    raw_headers.append("\"");
    // Also force revalidation.
    raw_headers.append(1, '\0');
    raw_headers.append("cache-control: no-cache");
  }

  raw_headers.append(2, '\0');
  return new net::HttpResponseHeaders(raw_headers);
}

class URLRequestResourceBundleJob : public net::URLRequestSimpleJob {
 public:
  URLRequestResourceBundleJob(net::URLRequest* request,
                              net::NetworkDelegate* network_delegate,
                              const base::FilePath& filename,
                              int resource_id,
                              const std::string& content_security_policy,
                              bool send_cors_header)
      : net::URLRequestSimpleJob(request, network_delegate),
        filename_(filename),
        resource_id_(resource_id),
        weak_factory_(this) {
     // Leave cache headers out of resource bundle requests.
    response_info_.headers = BuildHttpHeaders(content_security_policy,
                                              send_cors_header,
                                              base::Time());
  }

  // Overridden from URLRequestSimpleJob:
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const net::CompletionCallback& callback) const OVERRIDE {
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    *data = rb.GetRawDataResource(resource_id_).as_string();

    // Add the Content-Length header now that we know the resource length.
    response_info_.headers->AddHeader(base::StringPrintf(
        "%s: %s",  net::HttpRequestHeaders::kContentLength,
        base::UintToString(data->size()).c_str()));

    std::string* read_mime_type = new std::string;
    bool posted = base::PostTaskAndReplyWithResult(
        BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&net::GetMimeTypeFromFile, filename_,
                   base::Unretained(read_mime_type)),
        base::Bind(&URLRequestResourceBundleJob::OnMimeTypeRead,
                   weak_factory_.GetWeakPtr(),
                   mime_type, charset, data,
                   base::Owned(read_mime_type),
                   callback));
    DCHECK(posted);

    return net::ERR_IO_PENDING;
  }

  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE {
    *info = response_info_;
  }

 private:
  virtual ~URLRequestResourceBundleJob() { }

  void OnMimeTypeRead(std::string* out_mime_type,
                      std::string* charset,
                      std::string* data,
                      std::string* read_mime_type,
                      const net::CompletionCallback& callback,
                      bool read_result) {
    *out_mime_type = *read_mime_type;
    if (StartsWithASCII(*read_mime_type, "text/", false)) {
      // All of our HTML files should be UTF-8 and for other resource types
      // (like images), charset doesn't matter.
      DCHECK(IsStringUTF8(*data));
      *charset = "utf-8";
    }
    int result = read_result ? net::OK : net::ERR_INVALID_URL;
    callback.Run(result);
  }

  // We need the filename of the resource to determine the mime type.
  base::FilePath filename_;

  // The resource bundle id to load.
  int resource_id_;

  net::HttpResponseInfo response_info_;

  mutable base::WeakPtrFactory<URLRequestResourceBundleJob> weak_factory_;
};

class GeneratedBackgroundPageJob : public net::URLRequestSimpleJob {
 public:
  GeneratedBackgroundPageJob(net::URLRequest* request,
                             net::NetworkDelegate* network_delegate,
                             const scoped_refptr<const Extension> extension,
                             const std::string& content_security_policy)
      : net::URLRequestSimpleJob(request, network_delegate),
        extension_(extension) {
    const bool send_cors_headers = false;
    // Leave cache headers out of generated background page jobs.
    response_info_.headers = BuildHttpHeaders(content_security_policy,
                                              send_cors_headers,
                                              base::Time());
  }

  // Overridden from URLRequestSimpleJob:
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const net::CompletionCallback& callback) const OVERRIDE {
    *mime_type = "text/html";
    *charset = "utf-8";

    *data = "<!DOCTYPE html>\n<body>\n";
    const std::vector<std::string>& background_scripts =
        extensions::BackgroundInfo::GetBackgroundScripts(extension_.get());
    for (size_t i = 0; i < background_scripts.size(); ++i) {
      *data += "<script src=\"";
      *data += background_scripts[i];
      *data += "\"></script>\n";
    }

    return net::OK;
  }

  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE {
    *info = response_info_;
  }

 private:
  virtual ~GeneratedBackgroundPageJob() {}

  scoped_refptr<const Extension> extension_;
  net::HttpResponseInfo response_info_;
};

base::Time GetFileLastModifiedTime(const base::FilePath& filename) {
  if (base::PathExists(filename)) {
    base::File::Info info;
    if (base::GetFileInfo(filename, &info))
      return info.last_modified;
  }
  return base::Time();
}

base::Time GetFileCreationTime(const base::FilePath& filename) {
  if (base::PathExists(filename)) {
    base::File::Info info;
    if (base::GetFileInfo(filename, &info))
      return info.creation_time;
  }
  return base::Time();
}

void ReadResourceFilePathAndLastModifiedTime(
    const extensions::ExtensionResource& resource,
    const base::FilePath& directory,
    base::FilePath* file_path,
    base::Time* last_modified_time) {
  *file_path = resource.GetFilePath();
  *last_modified_time = GetFileLastModifiedTime(*file_path);
  // While we're here, log the delta between extension directory
  // creation time and the resource's last modification time.
  base::ElapsedTimer query_timer;
  base::Time dir_creation_time = GetFileCreationTime(directory);
  UMA_HISTOGRAM_TIMES("Extensions.ResourceDirectoryTimestampQueryLatency",
                      query_timer.Elapsed());
  int64 delta_seconds = (*last_modified_time - dir_creation_time).InSeconds();
  if (delta_seconds >= 0) {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.ResourceLastModifiedDelta",
                                delta_seconds,
                                0,
                                base::TimeDelta::FromDays(30).InSeconds(),
                                50);
  } else {
    UMA_HISTOGRAM_CUSTOM_COUNTS("Extensions.ResourceLastModifiedNegativeDelta",
                                -delta_seconds,
                                1,
                                base::TimeDelta::FromDays(30).InSeconds(),
                                50);
  }
}

class URLRequestExtensionJob : public net::URLRequestFileJob {
 public:
  URLRequestExtensionJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& extension_id,
                         const base::FilePath& directory_path,
                         const base::FilePath& relative_path,
                         const std::string& content_security_policy,
                         bool send_cors_header,
                         bool follow_symlinks_anywhere)
    : net::URLRequestFileJob(
          request, network_delegate, base::FilePath(),
          BrowserThread::GetBlockingPool()->GetTaskRunnerWithShutdownBehavior(
                  base::SequencedWorkerPool::SKIP_ON_SHUTDOWN)),
      directory_path_(directory_path),
      // TODO(tc): Move all of these files into resources.pak so we don't break
      // when updating on Linux.
      resource_(extension_id, directory_path, relative_path),
      content_security_policy_(content_security_policy),
      send_cors_header_(send_cors_header),
      weak_factory_(this) {
    if (follow_symlinks_anywhere) {
      resource_.set_follow_symlinks_anywhere();
    }
  }

  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE {
    *info = response_info_;
  }

  virtual void Start() OVERRIDE {
    base::FilePath* read_file_path = new base::FilePath;
    base::Time* last_modified_time = new base::Time();
    bool posted = BrowserThread::PostBlockingPoolTaskAndReply(
        FROM_HERE,
        base::Bind(&ReadResourceFilePathAndLastModifiedTime, resource_,
                   directory_path_,
                   base::Unretained(read_file_path),
                   base::Unretained(last_modified_time)),
        base::Bind(&URLRequestExtensionJob::OnFilePathAndLastModifiedTimeRead,
                   weak_factory_.GetWeakPtr(),
                   base::Owned(read_file_path),
                   base::Owned(last_modified_time)));
    DCHECK(posted);
  }

 private:
  virtual ~URLRequestExtensionJob() {}

  void OnFilePathAndLastModifiedTimeRead(base::FilePath* read_file_path,
                                         base::Time* last_modified_time) {
    file_path_ = *read_file_path;
    response_info_.headers = BuildHttpHeaders(
        content_security_policy_,
        send_cors_header_,
        *last_modified_time);
    URLRequestFileJob::Start();
  }

  net::HttpResponseInfo response_info_;
  base::FilePath directory_path_;
  extensions::ExtensionResource resource_;
  std::string content_security_policy_;
  bool send_cors_header_;
  base::WeakPtrFactory<URLRequestExtensionJob> weak_factory_;
};

bool ExtensionCanLoadInIncognito(const ResourceRequestInfo* info,
                                 const std::string& extension_id,
                                 extensions::InfoMap* extension_info_map) {
  if (!extension_info_map->IsIncognitoEnabled(extension_id))
    return false;

  // Only allow incognito toplevel navigations to extension resources in
  // split mode. In spanning mode, the extension must run in a single process,
  // and an incognito tab prevents that.
  if (info->GetResourceType() == ResourceType::MAIN_FRAME) {
    const Extension* extension =
        extension_info_map->extensions().GetByID(extension_id);
    return extension && extensions::IncognitoInfo::IsSplitMode(extension);
  }

  return true;
}

// Returns true if an chrome-extension:// resource should be allowed to load.
// TODO(aa): This should be moved into ExtensionResourceRequestPolicy, but we
// first need to find a way to get CanLoadInIncognito state into the renderers.
bool AllowExtensionResourceLoad(net::URLRequest* request,
                                Profile::ProfileType profile_type,
                                const Extension* extension,
                                extensions::InfoMap* extension_info_map) {
  const ResourceRequestInfo* info = ResourceRequestInfo::ForRequest(request);

  // We have seen crashes where info is NULL: crbug.com/52374.
  if (!info) {
    LOG(ERROR) << "Allowing load of " << request->url().spec()
               << "from unknown origin. Could not find user data for "
               << "request.";
    return true;
  }

  if (profile_type == Profile::INCOGNITO_PROFILE &&
      !ExtensionCanLoadInIncognito(info, request->url().host(),
                                   extension_info_map)) {
    return false;
  }

  // The following checks are meant to replicate similar set of checks in the
  // renderer process, performed by ResourceRequestPolicy::CanRequestResource.
  // These are not exactly equivalent, because we don't have the same bits of
  // information. The two checks need to be kept in sync as much as possible, as
  // an exploited renderer can bypass the checks in ResourceRequestPolicy.

  // Check if the extension for which this request is made is indeed loaded in
  // the process sending the request. If not, we need to explicitly check if
  // the resource is explicitly accessible or fits in a set of exception cases.
  // Note: This allows a case where two extensions execute in the same renderer
  // process to request each other's resources. We can't do a more precise
  // check, since the renderer can lie about which extension has made the
  // request.
  if (extension_info_map->process_map().Contains(
      request->url().host(), info->GetChildID())) {
    return true;
  }

  // Check workers so that importScripts works from extension workers.
  if (extension_info_map->worker_process_map().Contains(
      request->url().host(), info->GetChildID())) {
    return true;
  }

  // Extensions with webview: allow loading certain resources by guest renderers
  // with privileged partition IDs as specified in the manifest file.
  ExtensionRendererState* renderer_state =
      ExtensionRendererState::GetInstance();
  ExtensionRendererState::WebViewInfo webview_info;
  bool is_guest = renderer_state->GetWebViewInfo(info->GetChildID(),
                                                 info->GetRouteID(),
                                                 &webview_info);
  std::string resource_path = request->url().path();
  if (is_guest && extensions::WebviewInfo::IsResourceWebviewAccessible(
                      extension, webview_info.partition_id, resource_path)) {
    return true;
  }

  // If the request is for navigations outside of webviews, then it should be
  // allowed. The navigation logic in CrossSiteResourceHandler will properly
  // transfer the navigation to a privileged process before it commits.
  if (ResourceType::IsFrame(info->GetResourceType()) && !is_guest)
    return true;

  if (!content::PageTransitionIsWebTriggerable(info->GetPageTransition()))
    return false;

  // The following checks require that we have an actual extension object. If we
  // don't have it, allow the request handling to continue with the rest of the
  // checks.
  if (!extension)
    return true;

  // Disallow loading of packaged resources for hosted apps. We don't allow
  // hybrid hosted/packaged apps. The one exception is access to icons, since
  // some extensions want to be able to do things like create their own
  // launchers.
  std::string resource_root_relative_path =
      request->url().path().empty() ? std::string()
                                    : request->url().path().substr(1);
  if (extension->is_hosted_app() &&
      !extensions::IconsInfo::GetIcons(extension)
          .ContainsPath(resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << request->url().spec() << " from "
               << "hosted app.";
    return false;
  }

  // Extensions with web_accessible_resources: allow loading by regular
  // renderers. Since not all subresources are required to be listed in a v2
  // manifest, we must allow all loads if there are any web accessible
  // resources. See http://crbug.com/179127.
  if (extension->manifest_version() < 2 ||
      extensions::WebAccessibleResourcesInfo::HasWebAccessibleResources(
      extension)) {
    return true;
  }

  // If there aren't any explicitly marked web accessible resources, the
  // load should be allowed only if it is by DevTools. A close approximation is
  // checking if the extension contains a DevTools page.
  if (extensions::ManifestURL::GetDevToolsPage(extension).is_empty())
    return false;

  return true;
}

// Returns true if the given URL references an icon in the given extension.
bool URLIsForExtensionIcon(const GURL& url, const Extension* extension) {
  DCHECK(url.SchemeIs(extensions::kExtensionScheme));

  if (!extension)
    return false;

  std::string path = url.path();
  DCHECK_EQ(url.host(), extension->id());
  DCHECK(path.length() > 0 && path[0] == '/');
  path = path.substr(1);
  return extensions::IconsInfo::GetIcons(extension).ContainsPath(path);
}

class ExtensionProtocolHandler
    : public net::URLRequestJobFactory::ProtocolHandler {
 public:
  ExtensionProtocolHandler(Profile::ProfileType profile_type,
                           extensions::InfoMap* extension_info_map)
      : profile_type_(profile_type), extension_info_map_(extension_info_map) {}

  virtual ~ExtensionProtocolHandler() {}

  virtual net::URLRequestJob* MaybeCreateJob(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const OVERRIDE;

 private:
  const Profile::ProfileType profile_type_;
  extensions::InfoMap* const extension_info_map_;
  DISALLOW_COPY_AND_ASSIGN(ExtensionProtocolHandler);
};

// Creates URLRequestJobs for extension:// URLs.
net::URLRequestJob*
ExtensionProtocolHandler::MaybeCreateJob(
    net::URLRequest* request, net::NetworkDelegate* network_delegate) const {
  // chrome-extension://extension-id/resource/path.js
  std::string extension_id = request->url().host();
  const Extension* extension =
      extension_info_map_->extensions().GetByID(extension_id);

  // TODO(mpcomplete): better error code.
  if (!AllowExtensionResourceLoad(
          request, profile_type_, extension, extension_info_map_)) {
    return new net::URLRequestErrorJob(
        request, network_delegate, net::ERR_ADDRESS_UNREACHABLE);
  }

  base::FilePath directory_path;
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
  bool follow_symlinks_anywhere = false;
  if (extension) {
    std::string resource_path = request->url().path();
    content_security_policy =
        extensions::CSPInfo::GetResourceContentSecurityPolicy(extension,
                                                              resource_path);
    if ((extension->manifest_version() >= 2 ||
         extensions::WebAccessibleResourcesInfo::HasWebAccessibleResources(
             extension)) &&
        extensions::WebAccessibleResourcesInfo::IsResourceWebAccessible(
            extension, resource_path))
      send_cors_header = true;

    follow_symlinks_anywhere =
        (extension->creation_flags() & Extension::FOLLOW_SYMLINKS_ANYWHERE)
        != 0;
  }

  std::string path = request->url().path();
  if (path.size() > 1 &&
      path.substr(1) == extensions::kGeneratedBackgroundPageFilename) {
    return new GeneratedBackgroundPageJob(
        request, network_delegate, extension, content_security_policy);
  }

  base::FilePath resources_path;
  base::FilePath relative_path;
  // Try to load extension resources from chrome resource file if
  // directory_path is a descendant of resources_path. resources_path
  // corresponds to src/chrome/browser/resources in source tree.
  if (PathService::Get(chrome::DIR_RESOURCES, &resources_path) &&
      // Since component extension resources are included in
      // component_extension_resources.pak file in resources_path, calculate
      // extension relative path against resources_path.
      resources_path.AppendRelativePath(directory_path, &relative_path)) {
    base::FilePath request_path =
        extensions::file_util::ExtensionURLToRelativeFilePath(request->url());
    int resource_id;
    if (extensions::ImageLoader::IsComponentExtensionResource(
        directory_path, request_path, &resource_id)) {
      relative_path = relative_path.Append(request_path);
      relative_path = relative_path.NormalizePathSeparators();
      return new URLRequestResourceBundleJob(
          request,
          network_delegate,
          relative_path,
          resource_id,
          content_security_policy,
          send_cors_header);
    }
  }

  relative_path =
      extensions::file_util::ExtensionURLToRelativeFilePath(request->url());

  if (SharedModuleInfo::IsImportedPath(path)) {
    std::string new_extension_id;
    std::string new_relative_path;
    SharedModuleInfo::ParseImportedPath(path, &new_extension_id,
                                        &new_relative_path);
    const Extension* new_extension =
        extension_info_map_->extensions().GetByID(new_extension_id);

    bool first_party_in_import = false;
    // NB: This first_party_for_cookies call is not for security, it is only
    // used so an exported extension can limit the visible surface to the
    // extension that imports it, more or less constituting its API.
    const std::string& first_party_path =
        request->first_party_for_cookies().path();
    if (SharedModuleInfo::IsImportedPath(first_party_path)) {
      std::string first_party_id;
      std::string dummy;
      SharedModuleInfo::ParseImportedPath(first_party_path, &first_party_id,
                                          &dummy);
      if (first_party_id == new_extension_id) {
        first_party_in_import = true;
      }
    }

    if (SharedModuleInfo::ImportsExtensionById(extension, new_extension_id) &&
        new_extension &&
        (first_party_in_import ||
         SharedModuleInfo::IsExportAllowed(new_extension, new_relative_path))) {
      directory_path = new_extension->path();
      extension_id = new_extension_id;
      relative_path = base::FilePath::FromUTF8Unsafe(new_relative_path);
    } else {
      return NULL;
    }
  }

  return new URLRequestExtensionJob(request,
                                    network_delegate,
                                    extension_id,
                                    directory_path,
                                    relative_path,
                                    content_security_policy,
                                    send_cors_header,
                                    follow_symlinks_anywhere);
}

}  // namespace

net::URLRequestJobFactory::ProtocolHandler* CreateExtensionProtocolHandler(
    Profile::ProfileType profile_type,
    extensions::InfoMap* extension_info_map) {
  return new ExtensionProtocolHandler(profile_type, extension_info_map);
}
