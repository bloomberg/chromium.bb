// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_RENDERER_WEBKITCLIENT_IMPL_H_
#define CHROME_RENDERER_RENDERER_WEBKITCLIENT_IMPL_H_
#pragma once

#include "base/platform_file.h"
#include "base/scoped_ptr.h"
#include "chrome/renderer/websharedworkerrepository_impl.h"
#include "webkit/glue/simple_webmimeregistry_impl.h"
#include "webkit/glue/webclipboard_impl.h"
#include "webkit/glue/webfilesystem_impl.h"
#include "webkit/glue/webkitclient_impl.h"

#if defined(OS_WIN)
#include "third_party/WebKit/WebKit/chromium/public/win/WebSandboxSupport.h"
#elif defined(OS_LINUX)
#include <string>
#include <map>
#include "base/lock.h"
#include "third_party/WebKit/WebKit/chromium/public/linux/WebSandboxSupport.h"
#elif defined(OS_MACOSX)
#include "third_party/WebKit/WebKit/chromium/public/mac/WebSandboxSupport.h"
#endif

namespace IPC {
class SyncMessage;
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
  class MimeRegistry : public webkit_glue::SimpleWebMimeRegistryImpl {
   public:
    virtual WebKit::WebString mimeTypeForExtension(const WebKit::WebString&);
    virtual WebKit::WebString mimeTypeFromFile(const WebKit::WebString&);
    virtual WebKit::WebString preferredExtensionForMIMEType(
        const WebKit::WebString&);
  };

  class FileSystem : public webkit_glue::WebFileSystemImpl {
   public:
    virtual bool getFileSize(const WebKit::WebString& path, long long& result);
    virtual bool getFileModificationTime(const WebKit::WebString& path,
                                         double& result);
    virtual base::PlatformFile openFile(const WebKit::WebString& path,
                                        int mode);
  };

  bool CheckPreparsedJsCachingEnabled() const;

#if defined(OS_WIN)
  class SandboxSupport : public WebKit::WebSandboxSupport {
   public:
    virtual bool ensureFontLoaded(HFONT);
  };
#elif defined(OS_LINUX)
  class SandboxSupport : public WebKit::WebSandboxSupport {
   public:
    virtual WebKit::WebString getFontFamilyForCharacters(
        const WebKit::WebUChar* characters, size_t numCharacters);
    virtual void getRenderStyleForStrike(
        const char* family, int sizeAndStyle, WebKit::WebFontRenderStyle* out);

   private:
    // WebKit likes to ask us for the correct font family to use for a set of
    // unicode code points. It needs this information frequently so we cache it
    // here. The key in this map is an array of 16-bit UTF16 values from WebKit.
    // The value is a string containing the correct font family.
    Lock unicode_font_families_mutex_;
    std::map<std::string, std::string> unicode_font_families_;
  };
#elif defined(OS_MACOSX)
  class SandboxSupport : public WebKit::WebSandboxSupport {
   public:
    virtual bool loadFont(NSFont* srcFont, ATSFontContainerRef* out);
  };
#endif

  // Helper function to send synchronous message from any thread.
  static bool SendSyncMessageFromAnyThread(IPC::SyncMessage* msg);

  webkit_glue::WebClipboardImpl clipboard_;

  FileSystem file_system_;

  MimeRegistry mime_registry_;

  SandboxSupport sandbox_support_;

  // This counter keeps track of the number of times sudden termination is
  // enabled or disabled. It starts at 0 (enabled) and for every disable
  // increments by 1, for every enable decrements by 1. When it reaches 0,
  // we tell the browser to enable fast termination.
  int sudden_termination_disables_;

  // Implementation of the WebSharedWorkerRepository APIs (provides an interface
  // to WorkerService on the browser thread.
  WebSharedWorkerRepositoryImpl shared_worker_repository_;

  scoped_ptr<WebKit::WebIndexedDatabase> web_indexed_database_;
};

#endif  // CHROME_RENDERER_RENDERER_WEBKITCLIENT_IMPL_H_
