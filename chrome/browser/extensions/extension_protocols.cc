// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_protocols.h"

#include "base/string_util.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host_request_info.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_file_util.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/url_util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_file_job.h"
#include "net/url_request/url_request_error_job.h"

// Factory registered with URLRequest to create URLRequestJobs for extension://
// URLs.
static URLRequestJob* CreateExtensionURLRequestJob(URLRequest* request,
                                                   const std::string& scheme) {
  ChromeURLRequestContext* context =
      static_cast<ChromeURLRequestContext*>(request->context());

  // Don't allow toplevel navigations to extension resources in incognito mode.
  // This is because an extension must run in a single process, and an incognito
  // tab prevents that.
  // TODO(mpcomplete): better error code.
  const ResourceDispatcherHostRequestInfo* info =
      ResourceDispatcherHost::InfoForRequest(request);
  if (context->is_off_the_record() &&
      info && info->resource_type() == ResourceType::MAIN_FRAME)
    return new URLRequestErrorJob(request, net::ERR_ADDRESS_UNREACHABLE);

  // chrome-extension://extension-id/resource/path.js
  FilePath directory_path = context->GetPathForExtension(request->url().host());
  if (directory_path.value().empty()) {
    LOG(WARNING) << "Failed to GetPathForExtension: " << request->url().host();
    return NULL;
  }

  ExtensionResource resource(directory_path,
      extension_file_util::ExtensionURLToRelativeFilePath(request->url()));

  return new URLRequestFileJob(request,
                               resource.GetFilePathOnAnyThreadHack());
}

// Factory registered with URLRequest to create URLRequestJobs for
// chrome-user-script:/ URLs.
static URLRequestJob* CreateUserScriptURLRequestJob(URLRequest* request,
                                                    const std::string& scheme) {
  ChromeURLRequestContext* context =
      static_cast<ChromeURLRequestContext*>(request->context());

  // chrome-user-script:/user-script-name.user.js
  FilePath directory_path = context->user_script_dir_path();

  ExtensionResource resource(directory_path,
      extension_file_util::ExtensionURLToRelativeFilePath(request->url()));

  return new URLRequestFileJob(request, resource.GetFilePath());
}

void RegisterExtensionProtocols() {
  // Being a standard scheme allows us to resolve relative paths. This is used
  // by extensions, but not by standalone user scripts.
  url_util::AddStandardScheme(chrome::kExtensionScheme);

  URLRequest::RegisterProtocolFactory(chrome::kExtensionScheme,
                                      &CreateExtensionURLRequestJob);
  URLRequest::RegisterProtocolFactory(chrome::kUserScriptScheme,
                                      &CreateUserScriptURLRequestJob);
}
