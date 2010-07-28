// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBKITCLIENT_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBKITCLIENT_IMPL_H_
#pragma once

#include "base/platform_file.h"
#include "base/scoped_ptr.h"
#include "webkit/glue/webkitclient_impl.h"

class WebSharedWorkerRepositoryImpl;

namespace IPC {
class SyncMessage;
}

namespace webkit_glue {
class WebClipboardImpl;
}

class RendererWebKitClientImpl : public webkit_glue::WebKitClientImpl {
 public:
  RendererWebKitClientImpl();
  virtual ~RendererWebKitClientImpl();

  // WebKitClient methods:
  virtual WebKit::WebClipboard* clipboard();
  virtual WebKit::WebMimeRegistry* mimeRegistry();
  virtual WebKit::WebFileSystem* fileSystem();
  virtual WebKit::WebSandboxSupport* sandboxSupport();
  virtual WebKit::WebCookieJar* cookieJar();
  virtual bool sandboxEnabled();
  virtual unsigned long long visitedLinkHash(
      const char* canonicalURL, size_t length);
  virtual bool isLinkVisited(unsigned long long linkHash);
  virtual WebKit::WebMessagePortChannel* createMessagePortChannel();
  virtual void prefetchHostName(const WebKit::WebString&);
  virtual void cacheMetadata(
      const WebKit::WebURL&, double, const char*, size_t);
  virtual WebKit::WebString defaultLocale();
  virtual void suddenTerminationChanged(bool enabled);
  virtual WebKit::WebStorageNamespace* createLocalStorageNamespace(
      const WebKit::WebString& path, unsigned quota);
  virtual void dispatchStorageEvent(
      const WebKit::WebString& key, const WebKit::WebString& old_value,
      const WebKit::WebString& new_value, const WebKit::WebString& origin,
      const WebKit::WebURL& url, bool is_local_storage);

  virtual WebKit::WebKitClient::FileHandle databaseOpenFile(
      const WebKit::WebString& vfs_file_name, int desired_flags);
  virtual int databaseDeleteFile(const WebKit::WebString& vfs_file_name,
                                 bool sync_dir);
  virtual long databaseGetFileAttributes(
      const WebKit::WebString& vfs_file_name);
  virtual long long databaseGetFileSize(
      const WebKit::WebString& vfs_file_name);
  virtual WebKit::WebString signedPublicKeyAndChallengeString(
      unsigned key_size_index,
      const WebKit::WebString& challenge,
      const WebKit::WebURL& url);
  virtual WebKit::WebIndexedDatabase* indexedDatabase();

  virtual WebKit::WebSharedWorkerRepository* sharedWorkerRepository();
  virtual WebKit::WebGraphicsContext3D* createGraphicsContext3D();
  virtual WebKit::WebGLES2Context* createGLES2Context();

 private:
  bool CheckPreparsedJsCachingEnabled() const;

  // Helper function to send synchronous message from any thread.
  static bool SendSyncMessageFromAnyThread(IPC::SyncMessage* msg);

  scoped_ptr<webkit_glue::WebClipboardImpl> clipboard_;

  class FileSystem;
  scoped_ptr<FileSystem> file_system_;

  class MimeRegistry;
  scoped_ptr<MimeRegistry> mime_registry_;

  class SandboxSupport;
  scoped_ptr<SandboxSupport> sandbox_support_;

  // This counter keeps track of the number of times sudden termination is
  // enabled or disabled. It starts at 0 (enabled) and for every disable
  // increments by 1, for every enable decrements by 1. When it reaches 0,
  // we tell the browser to enable fast termination.
  int sudden_termination_disables_;

  // Implementation of the WebSharedWorkerRepository APIs (provides an interface
  // to WorkerService on the browser thread.
  scoped_ptr<WebSharedWorkerRepositoryImpl> shared_worker_repository_;

  scoped_ptr<WebKit::WebIndexedDatabase> web_indexed_database_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBKITCLIENT_IMPL_H_
