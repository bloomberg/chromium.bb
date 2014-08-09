// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/url_request_util.h"

#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/guest_view/web_view/web_view_renderer_state.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/component_extension_resource_manager.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/file_util.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_simple_job.h"
#include "ui/base/resource/resource_bundle.h"

using content::BrowserThread;
using content::ResourceType;
using extensions::ExtensionsBrowserClient;

namespace {

// A request for an extension resource in a Chrome .pak file. These are used
// by component extensions.
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
    response_info_.headers = extensions::BuildHttpHeaders(
        content_security_policy, send_cors_header, base::Time());
  }

  // Overridden from URLRequestSimpleJob:
  virtual int GetData(std::string* mime_type,
                      std::string* charset,
                      std::string* data,
                      const net::CompletionCallback& callback) const OVERRIDE {
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    *data = rb.GetRawDataResource(resource_id_).as_string();

    // Add the Content-Length header now that we know the resource length.
    response_info_.headers->AddHeader(
        base::StringPrintf("%s: %s",
                           net::HttpRequestHeaders::kContentLength,
                           base::UintToString(data->size()).c_str()));

    std::string* read_mime_type = new std::string;
    bool posted = base::PostTaskAndReplyWithResult(
        BrowserThread::GetBlockingPool(),
        FROM_HERE,
        base::Bind(&net::GetMimeTypeFromFile,
                   filename_,
                   base::Unretained(read_mime_type)),
        base::Bind(&URLRequestResourceBundleJob::OnMimeTypeRead,
                   weak_factory_.GetWeakPtr(),
                   mime_type,
                   charset,
                   data,
                   base::Owned(read_mime_type),
                   callback));
    DCHECK(posted);

    return net::ERR_IO_PENDING;
  }

  virtual void GetResponseInfo(net::HttpResponseInfo* info) OVERRIDE {
    *info = response_info_;
  }

 private:
  virtual ~URLRequestResourceBundleJob() {}

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
      DCHECK(base::IsStringUTF8(*data));
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

}  // namespace

namespace extensions {
namespace url_request_util {

bool AllowCrossRendererResourceLoad(net::URLRequest* request,
                                    bool is_incognito,
                                    const Extension* extension,
                                    InfoMap* extension_info_map) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);

  // Check workers so that importScripts works from extension workers.
  if (extension_info_map->worker_process_map().Contains(request->url().host(),
                                                        info->GetChildID())) {
    return true;
  }

  bool is_guest = false;

  // Extensions with webview: allow loading certain resources by guest renderers
  // with privileged partition IDs as specified in the manifest file.
  WebViewRendererState* web_view_renderer_state =
      WebViewRendererState::GetInstance();
  std::string partition_id;
  is_guest = web_view_renderer_state->GetPartitionID(info->GetChildID(),
                                                     &partition_id);
  std::string resource_path = request->url().path();
  if (is_guest && WebviewInfo::IsResourceWebviewAccessible(
                      extension, partition_id, resource_path)) {
    return true;
  }

  // If the request is for navigations outside of webviews, then it should be
  // allowed. The navigation logic in CrossSiteResourceHandler will properly
  // transfer the navigation to a privileged process before it commits.
  if (content::IsResourceTypeFrame(info->GetResourceType()) && !is_guest)
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
      !IconsInfo::GetIcons(extension)
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
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension)) {
    return true;
  }

  // If there aren't any explicitly marked web accessible resources, the
  // load should be allowed only if it is by DevTools. A close approximation is
  // checking if the extension contains a DevTools page.
  if (!ManifestURL::GetDevToolsPage(extension).is_empty())
    return true;

  // No special exception. Block the load.
  return false;
}

net::URLRequestJob* MaybeCreateURLRequestResourceBundleJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const base::FilePath& directory_path,
    const std::string& content_security_policy,
    bool send_cors_header) {
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
    int resource_id = 0;
    if (ExtensionsBrowserClient::Get()->GetComponentExtensionResourceManager()->
        IsComponentExtensionResource(
            directory_path, request_path, &resource_id)) {
      relative_path = relative_path.Append(request_path);
      relative_path = relative_path.NormalizePathSeparators();
      return new URLRequestResourceBundleJob(request,
                                             network_delegate,
                                             relative_path,
                                             resource_id,
                                             content_security_policy,
                                             send_cors_header);
    }
  }
  return NULL;
}

bool IsWebViewRequest(net::URLRequest* request) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  // |info| can be NULL sometimes: http://crbug.com/370070.
  if (!info)
    return false;
  return WebViewRendererState::GetInstance()->IsGuest(info->GetChildID());
}

}  // namespace url_request_util
}  // namespace extensions
