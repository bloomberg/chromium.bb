// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FILEAPI_WEBFILESYSTEM_IMPL_H_
#define CONTENT_COMMON_FILEAPI_WEBFILESYSTEM_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebFileSystem.h"

namespace WebKit {
class WebURL;
class WebFileWriter;
class WebFileWriterClient;
}

namespace content {

class WebFileSystemImpl : public WebKit::WebFileSystem {
 public:
  WebFileSystemImpl();
  virtual ~WebFileSystemImpl() { }

  // WebFileSystem implementation.
  virtual void move(
      const WebKit::WebURL& src_path,
      const WebKit::WebURL& dest_path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void copy(
      const WebKit::WebURL& src_path,
      const WebKit::WebURL& dest_path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void remove(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void removeRecursively(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void readMetadata(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void createFile(
      const WebKit::WebURL& path,
      bool exclusive,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void createDirectory(
      const WebKit::WebURL& path,
      bool exclusive,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void fileExists(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void directoryExists(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual void readDirectory(
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
  virtual WebKit::WebFileWriter* createFileWriter(
      const WebKit::WebURL& path, WebKit::WebFileWriterClient*) OVERRIDE;
  virtual void createSnapshotFileAndReadMetadata(
      const WebKit::WebURL& blobURL,
      const WebKit::WebURL& path,
      WebKit::WebFileSystemCallbacks*) OVERRIDE;
};

}  // namespace content

#endif  // CONTENT_COMMON_FILEAPI_WEBFILESYSTEM_IMPL_H_
