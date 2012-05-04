// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/browser_webkitplatformsupport_impl.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "content/browser/gpu/browser_gpu_channel_host_factory.h"
#include "content/browser/in_process_webkit/indexed_db_key_utility_client.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/public/common/serialized_script_value.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSerializedScriptValue.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/glue/webkit_glue.h"

using content::IndexedDBKey;
using content::IndexedDBKeyPath;
using content::SerializedScriptValue;

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

bool BrowserWebKitPlatformSupportImpl::isLinkVisited(unsigned long long link_hash) {
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

WebKit::WebSharedWorkerRepository*
BrowserWebKitPlatformSupportImpl::sharedWorkerRepository() {
    NOTREACHED();
    return NULL;
}

int BrowserWebKitPlatformSupportImpl::databaseDeleteFile(
    const WebKit::WebString& vfs_file_name, bool sync_dir) {
  const FilePath path = webkit_glue::WebStringToFilePath(vfs_file_name);
  return file_util::Delete(path, false) ? 0 : 1;
}

void
BrowserWebKitPlatformSupportImpl::createIDBKeysFromSerializedValuesAndKeyPath(
    const WebKit::WebVector<WebKit::WebSerializedScriptValue>& values,
    const WebKit::WebIDBKeyPath& keyPath,
    WebKit::WebVector<WebKit::WebIDBKey>& keys) {

  std::vector<SerializedScriptValue> std_values;
  size_t size = values.size();
  std_values.reserve(size);
  for (size_t i = 0; i < size; ++i)
    std_values.push_back(SerializedScriptValue(values[i]));

  std::vector<IndexedDBKey> std_keys;
  IndexedDBKeyUtilityClient::
      CreateIDBKeysFromSerializedValuesAndKeyPath(
          std_values, IndexedDBKeyPath(keyPath), &std_keys);

  keys = std_keys;
}

WebKit::WebSerializedScriptValue
BrowserWebKitPlatformSupportImpl::injectIDBKeyIntoSerializedValue(
    const WebKit::WebIDBKey& key, const WebKit::WebSerializedScriptValue& value,
    const WebKit::WebIDBKeyPath& keyPath) {
  return IndexedDBKeyUtilityClient::InjectIDBKeyIntoSerializedValue(
      IndexedDBKey(key), SerializedScriptValue(value),
      IndexedDBKeyPath(keyPath));
}

GpuChannelHostFactory*
BrowserWebKitPlatformSupportImpl::GetGpuChannelHostFactory() {
  return content::BrowserGpuChannelHostFactory::instance();
}
