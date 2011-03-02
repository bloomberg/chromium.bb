// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "net/url_request/url_request_error_job.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_simple_job.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

class URLRequestResourceBundleJob : public net::URLRequestSimpleJob {
 public:
  explicit URLRequestResourceBundleJob(net::URLRequest* request,
      const FilePath& filename, int resource_id)
          : net::URLRequestSimpleJob(request),
            filename_(filename),
            resource_id_(resource_id) { }

  // Overridden from URLRequestSimpleJob:
  virtual bool GetData(std::string* mime_type,
                       std::string* charset,
                       std::string* data) const {
    const ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    *data = rb.GetRawDataResource(resource_id_).as_string();
    bool result = net::GetMimeTypeFromFile(filename_, mime_type);
    if (StartsWithASCII(*mime_type, "text/", false)) {
      // All of our HTML files should be UTF-8 and for other resource types
      // (like images), charset doesn't matter.
      DCHECK(IsStringUTF8(*data));
      *charset = "utf-8";
    }
    return result;
  }

 private:
  virtual ~URLRequestResourceBundleJob() { }

  // We need the filename of the resource to determine the mime type.
  FilePath filename_;

  // The resource bundle id to load.
  int resource_id_;
};

// Returns true if an chrome-extension:// resource should be allowed to load.
// TODO(aa): This should be moved into ExtensionResourceRequestPolicy, but we
// first need to find a way to get CanLoadInIncognito state into the renderers.
bool AllowExtensionResourceLoad(net::URLRequest* request,
                                ChromeURLRequestContext* context,
                                const std::string& scheme) {
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
  if (context->is_off_the_record() &&
      info->resource_type() == ResourceType::MAIN_FRAME &&
      !context->extension_info_map()->
          ExtensionCanLoadInIncognito(request->url().host())) {
    LOG(ERROR) << "Denying load of " << request->url().spec() << " from "
               << "incognito tab.";
    return false;
  }

  return true;
}

}  // namespace

// Factory registered with net::URLRequest to create URLRequestJobs for
// extension:// URLs.
static net::URLRequestJob* CreateExtensionURLRequestJob(
    net::URLRequest* request,
    const std::string& scheme) {
  ChromeURLRequestContext* context =
      static_cast<ChromeURLRequestContext*>(request->context());

  // TODO(mpcomplete): better error code.
  if (!AllowExtensionResourceLoad(request, context, scheme)) {
    LOG(ERROR) << "disallowed in extension protocols";
    return new net::URLRequestErrorJob(request, net::ERR_ADDRESS_UNREACHABLE);
  }

  // chrome-extension://extension-id/resource/path.js
  const std::string& extension_id = request->url().host();
  FilePath directory_path = context->extension_info_map()->
      GetPathForExtension(extension_id);
  if (directory_path.value().empty()) {
    LOG(WARNING) << "Failed to GetPathForExtension: " << extension_id;
    return NULL;
  }

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
            kComponentExtensionResources[i].value);
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
  return new net::URLRequestFileJob(request, resource_file_path);
}

// Factory registered with net::URLRequest to create URLRequestJobs for
// chrome-user-script:/ URLs.
static net::URLRequestJob* CreateUserScriptURLRequestJob(
    net::URLRequest* request,
    const std::string& scheme) {
  ChromeURLRequestContext* context =
      static_cast<ChromeURLRequestContext*>(request->context());

  // chrome-user-script:/user-script-name.user.js
  FilePath directory_path = context->user_script_dir_path();

  ExtensionResource resource(request->url().host(), directory_path,
      extension_file_util::ExtensionURLToRelativeFilePath(request->url()));

  return new net::URLRequestFileJob(request, resource.GetFilePath());
}

void RegisterExtensionProtocols() {
  net::URLRequest::RegisterProtocolFactory(chrome::kExtensionScheme,
                                           &CreateExtensionURLRequestJob);
  net::URLRequest::RegisterProtocolFactory(chrome::kUserScriptScheme,
                                           &CreateUserScriptURLRequestJob);
}
