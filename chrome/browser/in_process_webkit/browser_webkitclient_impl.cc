// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/in_process_webkit/browser_webkitclient_impl.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "chrome/browser/in_process_webkit/dom_storage_message_filter.h"
#include "chrome/browser/in_process_webkit/indexed_db_key_utility_client.h"
#include "chrome/common/indexed_db_key.h"
#include "chrome/common/serialized_script_value.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSerializedScriptValue.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "webkit/glue/webkit_glue.h"

BrowserWebKitClientImpl::BrowserWebKitClientImpl() {
  file_utilities_.set_sandbox_enabled(false);
}

BrowserWebKitClientImpl::~BrowserWebKitClientImpl() {
}

WebKit::WebClipboard* BrowserWebKitClientImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

WebKit::WebMimeRegistry* BrowserWebKitClientImpl::mimeRegistry() {
  NOTREACHED();
  return NULL;
}

WebKit::WebFileUtilities* BrowserWebKitClientImpl::fileUtilities() {
  return &file_utilities_;
}

WebKit::WebSandboxSupport* BrowserWebKitClientImpl::sandboxSupport() {
  NOTREACHED();
  return NULL;
}

bool BrowserWebKitClientImpl::sandboxEnabled() {
  return false;
}

unsigned long long BrowserWebKitClientImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool BrowserWebKitClientImpl::isLinkVisited(unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

WebKit::WebMessagePortChannel*
BrowserWebKitClientImpl::createMessagePortChannel() {
  NOTREACHED();
  return NULL;
}

void BrowserWebKitClientImpl::setCookies(
    const WebKit::WebURL& url,
    const WebKit::WebURL& first_party_for_cookies,
    const WebKit::WebString& value) {
  NOTREACHED();
}

WebKit::WebString BrowserWebKitClientImpl::cookies(
    const WebKit::WebURL& url,
    const WebKit::WebURL& first_party_for_cookies) {
  NOTREACHED();
  return WebKit::WebString();
}

void BrowserWebKitClientImpl::prefetchHostName(const WebKit::WebString&) {
  NOTREACHED();
}

WebKit::WebString BrowserWebKitClientImpl::defaultLocale() {
  NOTREACHED();
  return WebKit::WebString();
}

WebKit::WebThemeEngine* BrowserWebKitClientImpl::themeEngine() {
  NOTREACHED();
  return NULL;
}

WebKit::WebURLLoader* BrowserWebKitClientImpl::createURLLoader() {
  NOTREACHED();
  return NULL;
}

WebKit::WebSocketStreamHandle*
    BrowserWebKitClientImpl::createSocketStreamHandle() {
  NOTREACHED();
  return NULL;
}

void BrowserWebKitClientImpl::getPluginList(bool refresh,
    WebKit::WebPluginListBuilder* builder) {
  NOTREACHED();
}

WebKit::WebData BrowserWebKitClientImpl::loadResource(const char* name) {
  NOTREACHED();
  return WebKit::WebData();
}

WebKit::WebStorageNamespace*
BrowserWebKitClientImpl::createLocalStorageNamespace(
    const WebKit::WebString& path, unsigned quota) {
  // The "WebStorage" interface is used for renderer WebKit -> browser WebKit
  // communication only.  "WebStorageClient" will be used for browser WebKit ->
  // renderer WebKit.  So this will never be implemented.
  NOTREACHED();
  return 0;
}

void BrowserWebKitClientImpl::dispatchStorageEvent(
    const WebKit::WebString& key, const WebKit::WebString& old_value,
    const WebKit::WebString& new_value, const WebKit::WebString& origin,
    const WebKit::WebURL& url, bool is_local_storage) {
  // TODO(jorlow): Implement
  if (!is_local_storage)
    return;

  DOMStorageMessageFilter::DispatchStorageEvent(key, old_value, new_value,
                                                origin, url, is_local_storage);
}

WebKit::WebSharedWorkerRepository*
BrowserWebKitClientImpl::sharedWorkerRepository() {
    NOTREACHED();
    return NULL;
}

int BrowserWebKitClientImpl::databaseDeleteFile(
    const WebKit::WebString& vfs_file_name, bool sync_dir) {
  const FilePath path = webkit_glue::WebStringToFilePath(vfs_file_name);
  return file_util::Delete(path, false) ? 0 : 1;
}

void BrowserWebKitClientImpl::createIDBKeysFromSerializedValuesAndKeyPath(
    const WebKit::WebVector<WebKit::WebSerializedScriptValue>& values,
    const WebKit::WebString& keyPath,
    WebKit::WebVector<WebKit::WebIDBKey>& keys) {

  std::vector<SerializedScriptValue> std_values;
  size_t size = values.size();
  std_values.reserve(size);
  for (size_t i = 0; i < size; ++i)
    std_values.push_back(SerializedScriptValue(values[i]));

  std::vector<IndexedDBKey> std_keys;
  IndexedDBKeyUtilityClient::
      CreateIDBKeysFromSerializedValuesAndKeyPath(std_values, keyPath,
                                                  &std_keys);

  keys = std_keys;
}
