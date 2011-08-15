// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_DOWNLOADS_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_DOWNLOADS_API_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/memory/singleton.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"

namespace base {
class DictionaryValue;
}
class ResourceDispatcherHost;
class TabContents;
namespace content {
class ResourceContext;
}

// Functions in the chrome.experimental.downloads namespace facilitate
// controlling downloads from extensions. See the full API doc at
// http://goo.gl/6hO1n

class DownloadsDownloadFunction : public AsyncExtensionFunction {
 public:
  DownloadsDownloadFunction();
  virtual ~DownloadsDownloadFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.download");

  virtual bool RunImpl() OVERRIDE;

 private:
  std::string url_;
  std::string filename_;
  bool save_as_;
  base::DictionaryValue* extra_headers_;
  std::string method_;
  std::string post_body_;

  ResourceDispatcherHost* rdh_;
  const content::ResourceContext* resource_context_;
  int render_process_host_id_;
  int render_view_host_routing_id_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsDownloadFunction);
};

class DownloadsSearchFunction : public SyncExtensionFunction {
 public:
  DownloadsSearchFunction();
  virtual ~DownloadsSearchFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.search");

  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsSearchFunction);
};

class DownloadsPauseFunction : public SyncExtensionFunction {
 public:
  DownloadsPauseFunction();
  virtual ~DownloadsPauseFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.pause");

  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsPauseFunction);
};

class DownloadsResumeFunction : public AsyncExtensionFunction {
 public:
  DownloadsResumeFunction();
  virtual ~DownloadsResumeFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.resume");

  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsResumeFunction);
};

class DownloadsCancelFunction : public AsyncExtensionFunction {
 public:
  DownloadsCancelFunction();
  virtual ~DownloadsCancelFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.cancel");

  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsCancelFunction);
};

class DownloadsEraseFunction : public AsyncExtensionFunction {
 public:
  DownloadsEraseFunction();
  virtual ~DownloadsEraseFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.erase");

  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsEraseFunction);
};

class DownloadsSetDestinationFunction : public AsyncExtensionFunction {
 public:
  DownloadsSetDestinationFunction();
  virtual ~DownloadsSetDestinationFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.setDestination");

  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsSetDestinationFunction);
};

class DownloadsAcceptDangerFunction : public AsyncExtensionFunction {
 public:
  DownloadsAcceptDangerFunction();
  virtual ~DownloadsAcceptDangerFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.acceptDanger");

  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsAcceptDangerFunction);
};

class DownloadsShowFunction : public AsyncExtensionFunction {
 public:
  DownloadsShowFunction();
  virtual ~DownloadsShowFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.show");

  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsShowFunction);
};

class DownloadsDragFunction : public AsyncExtensionFunction {
 public:
  DownloadsDragFunction();
  virtual ~DownloadsDragFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.drag");

  virtual bool RunImpl() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsDragFunction);
};
#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_DOWNLOADS_API_H_
