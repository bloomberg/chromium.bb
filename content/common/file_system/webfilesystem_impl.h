// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FILE_SYSTEM_WEBFILESYSTEM_IMPL_H_
#define CONTENT_COMMON_FILE_SYSTEM_WEBFILESYSTEM_IMPL_H_

#include "base/basictypes.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystem.h"

namespace WebKit {
class WebFileWriter;
class WebFileWriterClient;
}

class WebFileSystemImpl : public WebKit::WebFileSystem {
 public:
  WebFileSystemImpl();
  virtual ~WebFileSystemImpl() { }

  virtual void move(
      const WebKit::WebString& src_path,
      const WebKit::WebString& dest_path,
      WebKit::WebFileSystemCallbacks*);

  virtual void copy(
      const WebKit::WebString& src_path,
      const WebKit::WebString& dest_path,
      WebKit::WebFileSystemCallbacks*);

  virtual void remove(
      const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks*);

  virtual void removeRecursively(
      const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks*);

  virtual void readMetadata(
      const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks*);

  virtual void createFile(
      const WebKit::WebString& path,
      bool exclusive,
      WebKit::WebFileSystemCallbacks*);

  virtual void createDirectory(
      const WebKit::WebString& path,
      bool exclusive,
      WebKit::WebFileSystemCallbacks*);

  virtual void fileExists(
      const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks*);

  virtual void directoryExists(
      const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks*);

  virtual void readDirectory(
      const WebKit::WebString& path,
      WebKit::WebFileSystemCallbacks*);

  virtual WebKit::WebFileWriter* createFileWriter(
      const WebKit::WebString& path, WebKit::WebFileWriterClient*);
};

#endif  // CONTENT_COMMON_FILE_SYSTEM_WEBFILESYSTEM_IMPL_H_
