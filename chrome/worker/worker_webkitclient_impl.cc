// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/worker/worker_webkitclient_impl.h"

#include "base/logging.h"
#include "chrome/common/database_util.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/render_messages_params.h"
#include "chrome/common/webblobregistry_impl.h"
#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/worker/worker_thread.h"
#include "third_party/WebKit/WebKit/chromium/public/WebBlobRegistry.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"

using WebKit::WebBlobRegistry;
using WebKit::WebClipboard;
using WebKit::WebFileSystem;
using WebKit::WebKitClient;
using WebKit::WebMessagePortChannel;
using WebKit::WebMimeRegistry;
using WebKit::WebSandboxSupport;
using WebKit::WebSharedWorkerRepository;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebURL;

WebClipboard* WorkerWebKitClientImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

WebMimeRegistry* WorkerWebKitClientImpl::mimeRegistry() {
  return this;
}

WebKit::WebFileSystem* WorkerWebKitClientImpl::fileSystem() {
  if (!web_file_system_.get())
    web_file_system_.reset(new WebFileSystemImpl());
  return web_file_system_.get();
}

WebKit::WebFileUtilities* WorkerWebKitClientImpl::fileUtilities() {
  return &file_utilities_;
}

WebSandboxSupport* WorkerWebKitClientImpl::sandboxSupport() {
  NOTREACHED();
  return NULL;
}

bool WorkerWebKitClientImpl::sandboxEnabled() {
  // Always return true because WebKit should always act as though the Sandbox
  // is enabled for workers.  See the comment in WebKitClient for more info.
  return true;
}

unsigned long long WorkerWebKitClientImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool WorkerWebKitClientImpl::isLinkVisited(unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

WebMessagePortChannel*
WorkerWebKitClientImpl::createMessagePortChannel() {
  return new WebMessagePortChannelImpl();
}

void WorkerWebKitClientImpl::setCookies(const WebURL& url,
                                        const WebURL& first_party_for_cookies,
                                        const WebString& value) {
  NOTREACHED();
}

WebString WorkerWebKitClientImpl::cookies(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  // WebSocketHandshake may access cookies in worker process.
  return WebString();
}

void WorkerWebKitClientImpl::prefetchHostName(const WebString&) {
  NOTREACHED();
}

bool WorkerWebKitClientImpl::getFileSize(const WebString& path,
                                         long long& result) {
  NOTREACHED();
  return false;
}

WebString WorkerWebKitClientImpl::defaultLocale() {
  NOTREACHED();
  return WebString();
}

WebStorageNamespace* WorkerWebKitClientImpl::createLocalStorageNamespace(
    const WebString& path, unsigned quota) {
  NOTREACHED();
  return 0;
}

void WorkerWebKitClientImpl::dispatchStorageEvent(
    const WebString& key, const WebString& old_value,
    const WebString& new_value, const WebString& origin,
    const WebKit::WebURL& url, bool is_local_storage) {
  NOTREACHED();
}

WebSharedWorkerRepository* WorkerWebKitClientImpl::sharedWorkerRepository() {
    return 0;
}

WebKitClient::FileHandle WorkerWebKitClientImpl::databaseOpenFile(
    const WebString& vfs_file_name, int desired_flags) {
  return DatabaseUtil::databaseOpenFile(vfs_file_name, desired_flags);
}

int WorkerWebKitClientImpl::databaseDeleteFile(
    const WebString& vfs_file_name, bool sync_dir) {
  return DatabaseUtil::databaseDeleteFile(vfs_file_name, sync_dir);
}

long WorkerWebKitClientImpl::databaseGetFileAttributes(
    const WebString& vfs_file_name) {
  return DatabaseUtil::databaseGetFileAttributes(vfs_file_name);
}

long long WorkerWebKitClientImpl::databaseGetFileSize(
    const WebString& vfs_file_name) {
  return DatabaseUtil::databaseGetFileSize(vfs_file_name);
}

WebMimeRegistry::SupportsType WorkerWebKitClientImpl::supportsMIMEType(
    const WebString&) {
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType WorkerWebKitClientImpl::supportsImageMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType WorkerWebKitClientImpl::supportsJavaScriptMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType WorkerWebKitClientImpl::supportsMediaMIMEType(
    const WebString&, const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType WorkerWebKitClientImpl::supportsNonImageMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebString WorkerWebKitClientImpl::mimeTypeForExtension(const WebString&) {
  NOTREACHED();
  return WebString();
}

WebString WorkerWebKitClientImpl::mimeTypeFromFile(const WebString&) {
  NOTREACHED();
  return WebString();
}

WebString WorkerWebKitClientImpl::preferredExtensionForMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebString();
}

WebBlobRegistry* WorkerWebKitClientImpl::blobRegistry() {
  if (!blob_registry_.get())
    blob_registry_.reset(new WebBlobRegistryImpl(WorkerThread::current()));
  return blob_registry_.get();
}
