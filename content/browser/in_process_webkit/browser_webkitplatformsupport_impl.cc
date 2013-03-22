// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/browser_webkitplatformsupport_impl.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/sys_info.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebData.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURL.h"
#include "webkit/base/file_path_string_conversions.h"

namespace content {

BrowserWebKitPlatformSupportImpl::BrowserWebKitPlatformSupportImpl() {
  file_utilities_.set_sandbox_enabled(false);
}

BrowserWebKitPlatformSupportImpl::~BrowserWebKitPlatformSupportImpl() {
}

WebKit::WebClipboard* BrowserWebKitPlatformSupportImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

WebKit::WebMimeRegistry* BrowserWebKitPlatformSupportImpl::mimeRegistry() {
  NOTREACHED();
  return NULL;
}

WebKit::WebFileUtilities* BrowserWebKitPlatformSupportImpl::fileUtilities() {
  return &file_utilities_;
}

WebKit::WebSandboxSupport* BrowserWebKitPlatformSupportImpl::sandboxSupport() {
  NOTREACHED();
  return NULL;
}

bool BrowserWebKitPlatformSupportImpl::sandboxEnabled() {
  return false;
}

unsigned long long BrowserWebKitPlatformSupportImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool BrowserWebKitPlatformSupportImpl::isLinkVisited(
    unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

WebKit::WebMessagePortChannel*
BrowserWebKitPlatformSupportImpl::createMessagePortChannel() {
  NOTREACHED();
  return NULL;
}

void BrowserWebKitPlatformSupportImpl::setCookies(
    const WebKit::WebURL& url,
    const WebKit::WebURL& first_party_for_cookies,
    const WebKit::WebString& value) {
  NOTREACHED();
}

WebKit::WebString BrowserWebKitPlatformSupportImpl::cookies(
    const WebKit::WebURL& url,
    const WebKit::WebURL& first_party_for_cookies) {
  NOTREACHED();
  return WebKit::WebString();
}

void BrowserWebKitPlatformSupportImpl::prefetchHostName(
    const WebKit::WebString&) {
  NOTREACHED();
}

WebKit::WebString BrowserWebKitPlatformSupportImpl::defaultLocale() {
  NOTREACHED();
  return WebKit::WebString();
}

WebKit::WebThemeEngine* BrowserWebKitPlatformSupportImpl::themeEngine() {
  NOTREACHED();
  return NULL;
}

WebKit::WebURLLoader* BrowserWebKitPlatformSupportImpl::createURLLoader() {
  NOTREACHED();
  return NULL;
}

WebKit::WebSocketStreamHandle*
BrowserWebKitPlatformSupportImpl::createSocketStreamHandle() {
  NOTREACHED();
  return NULL;
}

void BrowserWebKitPlatformSupportImpl::getPluginList(bool refresh,
    WebKit::WebPluginListBuilder* builder) {
  NOTREACHED();
}

WebKit::WebData BrowserWebKitPlatformSupportImpl::loadResource(
    const char* name) {
  NOTREACHED();
  return WebKit::WebData();
}

int BrowserWebKitPlatformSupportImpl::databaseDeleteFile(
    const WebKit::WebString& vfs_file_name, bool sync_dir) {
  const base::FilePath path = webkit_base::WebStringToFilePath(vfs_file_name);
  return file_util::Delete(path, false) ? 0 : 1;
}

long long BrowserWebKitPlatformSupportImpl::availableDiskSpaceInBytes(
    const WebKit::WebString& fileName) {
  return base::SysInfo::AmountOfFreeDiskSpace(
      webkit_base::WebStringToFilePath(fileName));
}

}  // namespace content
