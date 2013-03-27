// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/sandboxed_page_info.h"

#include "base/lazy_instance.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/extensions/csp_validator.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/url_pattern.h"

namespace extensions {

namespace {

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

const char kDefaultSandboxedPageContentSecurityPolicy[] =
    "sandbox allow-scripts allow-forms allow-popups";

static base::LazyInstance<SandboxedPageInfo> g_empty_sandboxed_info =
    LAZY_INSTANCE_INITIALIZER;

const SandboxedPageInfo& GetSandboxedPageInfo(const Extension* extension) {
  SandboxedPageInfo* info = static_cast<SandboxedPageInfo*>(
      extension->GetManifestData(keys::kSandboxedPages));
  return info ? *info : g_empty_sandboxed_info.Get();
}

}  // namespace

SandboxedPageInfo::SandboxedPageInfo() {
}

SandboxedPageInfo::~SandboxedPageInfo() {
}

const std::string& SandboxedPageInfo::GetContentSecurityPolicy(
    const Extension* extension) {
  return GetSandboxedPageInfo(extension).content_security_policy;
}

const URLPatternSet& SandboxedPageInfo::GetPages(const Extension* extension) {
  return GetSandboxedPageInfo(extension).pages;
}

bool SandboxedPageInfo::IsSandboxedPage(const Extension* extension,
                                    const std::string& relative_path) {
  return extension->ResourceMatches(GetPages(extension), relative_path);
}

SandboxedPageHandler::SandboxedPageHandler() {
}

SandboxedPageHandler::~SandboxedPageHandler() {
}

bool SandboxedPageHandler::Parse(Extension* extension, string16* error) {
  scoped_ptr<SandboxedPageInfo> sandboxed_info(new SandboxedPageInfo);

  const base::ListValue* list_value = NULL;
  if (!extension->manifest()->GetList(keys::kSandboxedPages, &list_value)) {
    *error = ASCIIToUTF16(errors::kInvalidSandboxedPagesList);
    return false;
  }

  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    std::string relative_path;
    if (!list_value->GetString(i, &relative_path)) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidSandboxedPage, base::IntToString(i));
      return false;
    }
    URLPattern pattern(URLPattern::SCHEME_EXTENSION);
    if (pattern.Parse(extension->url().spec()) != URLPattern::PARSE_SUCCESS) {
      *error = ErrorUtils::FormatErrorMessageUTF16(
          errors::kInvalidURLPatternError, extension->url().spec());
      return false;
    }
    while (relative_path[0] == '/')
      relative_path = relative_path.substr(1, relative_path.length() - 1);
    pattern.SetPath(pattern.path() + relative_path);
    sandboxed_info->pages.AddPattern(pattern);
  }

  if (extension->manifest()->HasPath(keys::kSandboxedPagesCSP)) {
    if (!extension->manifest()->GetString(
            keys::kSandboxedPagesCSP,
            &sandboxed_info->content_security_policy)) {
      *error = ASCIIToUTF16(errors::kInvalidSandboxedPagesCSP);
      return false;
    }

    if (!csp_validator::ContentSecurityPolicyIsLegal(
            sandboxed_info->content_security_policy) ||
        !csp_validator::ContentSecurityPolicyIsSandboxed(
            sandboxed_info->content_security_policy, extension->GetType())) {
      *error = ASCIIToUTF16(errors::kInvalidSandboxedPagesCSP);
      return false;
    }
  } else {
    sandboxed_info->content_security_policy =
        kDefaultSandboxedPageContentSecurityPolicy;
    CHECK(csp_validator::ContentSecurityPolicyIsSandboxed(
        sandboxed_info->content_security_policy, extension->GetType()));
  }

  extension->SetManifestData(keys::kSandboxedPages, sandboxed_info.release());
  return true;
}

const std::vector<std::string> SandboxedPageHandler::Keys() const {
  return SingleKey(keys::kSandboxedPages);
}

}  // namespace extensions
