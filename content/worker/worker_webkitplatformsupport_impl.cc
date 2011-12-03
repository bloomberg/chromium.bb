// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/worker_webkitplatformsupport_impl.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "content/common/database_util.h"
#include "content/common/file_system/webfilesystem_impl.h"
#include "content/common/file_utilities_messages.h"
#include "content/common/mime_registry_messages.h"
#include "content/common/webblobregistry_impl.h"
#include "content/common/webmessageportchannel_impl.h"
#include "content/worker/worker_thread.h"
#include "ipc/ipc_sync_message_filter.h"
#include "net/base/mime_util.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebBlobRegistry.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "webkit/glue/webfileutilities_impl.h"
#include "webkit/glue/webkit_glue.h"

using WebKit::WebBlobRegistry;
using WebKit::WebClipboard;
using WebKit::WebFileSystem;
using WebKit::WebFileUtilities;
using WebKit::WebKitPlatformSupport;
using WebKit::WebMessagePortChannel;
using WebKit::WebMimeRegistry;
using WebKit::WebSandboxSupport;
using WebKit::WebSharedWorkerRepository;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebURL;

// TODO(kinuko): Probably this could be consolidated into
// RendererWebKitPlatformSupportImpl::FileUtilities.
class WorkerWebKitPlatformSupportImpl::FileUtilities
    : public webkit_glue::WebFileUtilitiesImpl {
 public:
  virtual bool getFileSize(const WebKit::WebString& path, long long& result);
  virtual bool getFileModificationTime(const WebKit::WebString& path,
                                       double& result);
};

static bool SendSyncMessageFromAnyThread(IPC::SyncMessage* msg) {
  WorkerThread* worker_thread = WorkerThread::current();
  if (worker_thread)
    return worker_thread->Send(msg);

  scoped_refptr<IPC::SyncMessageFilter> sync_msg_filter(
      ChildThread::current()->sync_message_filter());
  return sync_msg_filter->Send(msg);
}

bool WorkerWebKitPlatformSupportImpl::FileUtilities::getFileSize(
    const WebString& path, long long& result) {
  if (SendSyncMessageFromAnyThread(new FileUtilitiesMsg_GetFileSize(
          webkit_glue::WebStringToFilePath(path),
          reinterpret_cast<int64*>(&result)))) {
    return result >= 0;
  }

  result = -1;
  return false;
}

bool WorkerWebKitPlatformSupportImpl::FileUtilities::getFileModificationTime(
    const WebString& path,
    double& result) {
  base::Time time;
  if (SendSyncMessageFromAnyThread(new FileUtilitiesMsg_GetFileModificationTime(
              webkit_glue::WebStringToFilePath(path), &time))) {
    result = time.ToDoubleT();
    return !time.is_null();
  }

  result = 0;
  return false;
}

//------------------------------------------------------------------------------

WorkerWebKitPlatformSupportImpl::WorkerWebKitPlatformSupportImpl() {
}

WorkerWebKitPlatformSupportImpl::~WorkerWebKitPlatformSupportImpl() {
}

WebClipboard* WorkerWebKitPlatformSupportImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

WebMimeRegistry* WorkerWebKitPlatformSupportImpl::mimeRegistry() {
  return this;
}

WebFileSystem* WorkerWebKitPlatformSupportImpl::fileSystem() {
  if (!web_file_system_.get())
    web_file_system_.reset(new WebFileSystemImpl());
  return web_file_system_.get();
}

WebFileUtilities* WorkerWebKitPlatformSupportImpl::fileUtilities() {
  if (!file_utilities_.get()) {
    file_utilities_.reset(new FileUtilities);
    file_utilities_->set_sandbox_enabled(sandboxEnabled());
  }
  return file_utilities_.get();
}

WebSandboxSupport* WorkerWebKitPlatformSupportImpl::sandboxSupport() {
  NOTREACHED();
  return NULL;
}

bool WorkerWebKitPlatformSupportImpl::sandboxEnabled() {
  // Always return true because WebKit should always act as though the Sandbox
  // is enabled for workers.  See the comment in WebKitPlatformSupport for
  // more info.
  return true;
}

unsigned long long WorkerWebKitPlatformSupportImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool WorkerWebKitPlatformSupportImpl::isLinkVisited(
    unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

WebMessagePortChannel*
WorkerWebKitPlatformSupportImpl::createMessagePortChannel() {
  return new WebMessagePortChannelImpl();
}

void WorkerWebKitPlatformSupportImpl::setCookies(
    const WebURL& url,
    const WebURL& first_party_for_cookies,
    const WebString& value) {
  NOTREACHED();
}

WebString WorkerWebKitPlatformSupportImpl::cookies(
    const WebURL& url, const WebURL& first_party_for_cookies) {
  // WebSocketHandshake may access cookies in worker process.
  return WebString();
}

void WorkerWebKitPlatformSupportImpl::prefetchHostName(const WebString&) {
  NOTREACHED();
}

WebString WorkerWebKitPlatformSupportImpl::defaultLocale() {
  NOTREACHED();
  return WebString();
}

WebStorageNamespace*
WorkerWebKitPlatformSupportImpl::createLocalStorageNamespace(
    const WebString& path, unsigned quota) {
  NOTREACHED();
  return 0;
}

void WorkerWebKitPlatformSupportImpl::dispatchStorageEvent(
    const WebString& key, const WebString& old_value,
    const WebString& new_value, const WebString& origin,
    const WebKit::WebURL& url, bool is_local_storage) {
  NOTREACHED();
}

WebSharedWorkerRepository*
WorkerWebKitPlatformSupportImpl::sharedWorkerRepository() {
    return 0;
}

WebKitPlatformSupport::FileHandle
WorkerWebKitPlatformSupportImpl::databaseOpenFile(
    const WebString& vfs_file_name, int desired_flags) {
  return DatabaseUtil::DatabaseOpenFile(vfs_file_name, desired_flags);
}

int WorkerWebKitPlatformSupportImpl::databaseDeleteFile(
    const WebString& vfs_file_name, bool sync_dir) {
  return DatabaseUtil::DatabaseDeleteFile(vfs_file_name, sync_dir);
}

long WorkerWebKitPlatformSupportImpl::databaseGetFileAttributes(
    const WebString& vfs_file_name) {
  return DatabaseUtil::DatabaseGetFileAttributes(vfs_file_name);
}

long long WorkerWebKitPlatformSupportImpl::databaseGetFileSize(
    const WebString& vfs_file_name) {
  return DatabaseUtil::DatabaseGetFileSize(vfs_file_name);
}

long long WorkerWebKitPlatformSupportImpl::databaseGetSpaceAvailableForOrigin(
    const WebString& origin_identifier) {
  return DatabaseUtil::DatabaseGetSpaceAvailable(origin_identifier);
}

WebMimeRegistry::SupportsType
WorkerWebKitPlatformSupportImpl::supportsMIMEType(
    const WebString&) {
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType
WorkerWebKitPlatformSupportImpl::supportsImageMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType
WorkerWebKitPlatformSupportImpl::supportsJavaScriptMIMEType(const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType
WorkerWebKitPlatformSupportImpl::supportsMediaMIMEType(
    const WebString&, const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebMimeRegistry::SupportsType
WorkerWebKitPlatformSupportImpl::supportsNonImageMIMEType(
    const WebString&) {
  NOTREACHED();
  return WebMimeRegistry::IsSupported;
}

WebString WorkerWebKitPlatformSupportImpl::mimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  SendSyncMessageFromAnyThread(new MimeRegistryMsg_GetMimeTypeFromExtension(
      webkit_glue::WebStringToFilePathString(file_extension), &mime_type));
  return ASCIIToUTF16(mime_type);
}

WebString WorkerWebKitPlatformSupportImpl::wellKnownMimeTypeForExtension(
    const WebString& file_extension) {
  std::string mime_type;
  net::GetWellKnownMimeTypeFromExtension(
      webkit_glue::WebStringToFilePathString(file_extension), &mime_type);
  return ASCIIToUTF16(mime_type);
}

WebString WorkerWebKitPlatformSupportImpl::mimeTypeFromFile(
    const WebString& file_path) {
  std::string mime_type;
  SendSyncMessageFromAnyThread(new MimeRegistryMsg_GetMimeTypeFromFile(
      FilePath(webkit_glue::WebStringToFilePathString(file_path)),
      &mime_type));
  return ASCIIToUTF16(mime_type);
}

WebString WorkerWebKitPlatformSupportImpl::preferredExtensionForMIMEType(
    const WebString& mime_type) {
  FilePath::StringType file_extension;
  SendSyncMessageFromAnyThread(
      new MimeRegistryMsg_GetPreferredExtensionForMimeType(
          UTF16ToASCII(mime_type), &file_extension));
  return webkit_glue::FilePathStringToWebString(file_extension);
}

WebBlobRegistry* WorkerWebKitPlatformSupportImpl::blobRegistry() {
  if (!blob_registry_.get())
    blob_registry_.reset(new WebBlobRegistryImpl(WorkerThread::current()));
  return blob_registry_.get();
}
