// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/csp_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/csp_validator.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handlers/sandboxed_page_info.h"

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

using extensions::csp_validator::ContentSecurityPolicyIsLegal;
using extensions::csp_validator::ContentSecurityPolicyIsSecure;

namespace extensions {

namespace {

const char kDefaultContentSecurityPolicy[] =
    "script-src 'self' chrome-extension-resource:; object-src 'self'";

#define PLATFORM_APP_LOCAL_CSP_SOURCES \
    "'self' data: chrome-extension-resource:"
const char kDefaultPlatformAppContentSecurityPolicy[] =
    // Platform apps can only use local resources by default.
    "default-src 'self' chrome-extension-resource:;"
    // For remote resources, they can fetch them via XMLHttpRequest.
    "connect-src *;"
    // And serve them via data: or same-origin (blob:, filesystem:) URLs
    "style-src " PLATFORM_APP_LOCAL_CSP_SOURCES " 'unsafe-inline';"
    "img-src " PLATFORM_APP_LOCAL_CSP_SOURCES ";"
    "frame-src " PLATFORM_APP_LOCAL_CSP_SOURCES ";"
    "font-src " PLATFORM_APP_LOCAL_CSP_SOURCES ";"
    // Media can be loaded from remote resources since:
    // 1. <video> and <audio> have good fallback behavior when offline or under
    //    spotty connectivity.
    // 2. Fetching via XHR and serving via blob: URLs currently does not allow
    //    streaming or partial buffering.
    "media-src *;";

}  // namespace

CSPInfo::CSPInfo(const std::string& security_policy)
    : content_security_policy(security_policy) {
}

CSPInfo::~CSPInfo() {
}

// static
const std::string& CSPInfo::GetContentSecurityPolicy(
    const Extension* extension) {
  CSPInfo* csp_info = static_cast<CSPInfo*>(
          extension->GetManifestData(keys::kContentSecurityPolicy));
  return csp_info ? csp_info->content_security_policy : EmptyString();
}

// static
const std::string& CSPInfo::GetResourceContentSecurityPolicy(
    const Extension* extension,
    const std::string& relative_path) {
  return SandboxedPageInfo::IsSandboxedPage(extension, relative_path) ?
      SandboxedPageInfo::GetContentSecurityPolicy(extension) :
      GetContentSecurityPolicy(extension);
}

CSPHandler::CSPHandler(bool is_platform_app)
    : is_platform_app_(is_platform_app) {
}

CSPHandler::~CSPHandler() {
}

bool CSPHandler::Parse(Extension* extension, string16* error) {
  const std::string key = Keys()[0];
  if (!extension->manifest()->HasPath(key)) {
    if (extension->manifest_version() >= 2) {
      // TODO(abarth): Should we continue to let extensions override the
      //               default Content-Security-Policy?
      std::string content_security_policy = is_platform_app_ ?
          kDefaultPlatformAppContentSecurityPolicy :
          kDefaultContentSecurityPolicy;

      CHECK(ContentSecurityPolicyIsSecure(content_security_policy,
                                          extension->GetType()));
      extension->SetManifestData(keys::kContentSecurityPolicy,
                                 new CSPInfo(content_security_policy));
    }
    return true;
  }

  std::string content_security_policy;
  if (!extension->manifest()->GetString(key, &content_security_policy)) {
    *error = ASCIIToUTF16(errors::kInvalidContentSecurityPolicy);
    return false;
  }
  if (!ContentSecurityPolicyIsLegal(content_security_policy)) {
    *error = ASCIIToUTF16(errors::kInvalidContentSecurityPolicy);
    return false;
  }
  if (extension->manifest_version() >= 2 &&
      !ContentSecurityPolicyIsSecure(content_security_policy,
                                     extension->GetType())) {
    *error = ASCIIToUTF16(errors::kInsecureContentSecurityPolicy);
    return false;
  }

  extension->SetManifestData(keys::kContentSecurityPolicy,
                             new CSPInfo(content_security_policy));
  return true;
}

bool CSPHandler::AlwaysParseForType(Manifest::Type type) const {
  if (is_platform_app_)
    return type == Manifest::TYPE_PLATFORM_APP;
  else
    return type == Manifest::TYPE_EXTENSION ||
        type == Manifest::TYPE_LEGACY_PACKAGED_APP;
}

const std::vector<std::string> CSPHandler::Keys() const {
  const std::string& key = is_platform_app_ ?
      keys::kPlatformAppContentSecurityPolicy : keys::kContentSecurityPolicy;
  return SingleKey(key);
}

}  // namespace extensions
