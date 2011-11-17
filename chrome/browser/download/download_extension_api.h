// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_DOWNLOAD_EXTENSION_API_H_
#define CHROME_BROWSER_DOWNLOAD_DOWNLOAD_EXTENSION_API_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/file_path.h"
#include "base/memory/singleton.h"
#include "base/string16.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/browser/download/download_item.h"
#include "content/browser/download/download_manager.h"
#include "content/browser/download/download_state_info.h"

class ResourceDispatcherHost;

namespace content {
class ResourceContext;
}

// Functions in the chrome.experimental.downloads namespace facilitate
// controlling downloads from extensions. See the full API doc at
// http://goo.gl/6hO1n

class DownloadsFunctionInterface {
 public:
  enum DownloadsFunctionName {
    DOWNLOADS_FUNCTION_DOWNLOAD = 0,
    DOWNLOADS_FUNCTION_SEARCH = 1,
    DOWNLOADS_FUNCTION_PAUSE = 2,
    DOWNLOADS_FUNCTION_RESUME = 3,
    DOWNLOADS_FUNCTION_CANCEL = 4,
    DOWNLOADS_FUNCTION_ERASE = 5,
    DOWNLOADS_FUNCTION_SET_DESTINATION = 6,
    DOWNLOADS_FUNCTION_ACCEPT_DANGER = 7,
    DOWNLOADS_FUNCTION_SHOW = 8,
    DOWNLOADS_FUNCTION_DRAG = 9,
    // Insert new values here, not at the beginning.
    DOWNLOADS_FUNCTION_LAST
  };

 protected:
  // Return true if args_ is well-formed, otherwise set error_ and return false.
  virtual bool ParseArgs() = 0;

  // Implementation-specific logic. "Do the thing that you do."
  virtual void RunInternal() = 0;

  // Which subclass is this.
  virtual DownloadsFunctionName function() const = 0;

  // Wrap ParseArgs(), RunInternal().
  static bool RunImplImpl(DownloadsFunctionInterface* pimpl);
};

class SyncDownloadsFunction : public SyncExtensionFunction,
                              public DownloadsFunctionInterface {
 public:
  virtual bool RunImpl() OVERRIDE;

 protected:
  explicit SyncDownloadsFunction(DownloadsFunctionName function);
  virtual ~SyncDownloadsFunction();
  virtual DownloadsFunctionName function() const OVERRIDE;

 private:
  DownloadsFunctionName function_;

  DISALLOW_COPY_AND_ASSIGN(SyncDownloadsFunction);
};

class AsyncDownloadsFunction : public AsyncExtensionFunction,
                               public DownloadsFunctionInterface {
 public:
  virtual bool RunImpl() OVERRIDE;

 protected:
  explicit AsyncDownloadsFunction(DownloadsFunctionName function);
  virtual ~AsyncDownloadsFunction();
  virtual DownloadsFunctionName function() const OVERRIDE;

 private:
  DownloadsFunctionName function_;

  DISALLOW_COPY_AND_ASSIGN(AsyncDownloadsFunction);
};

class DownloadsDownloadFunction : public AsyncDownloadsFunction {
 public:
  DownloadsDownloadFunction();
  virtual ~DownloadsDownloadFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.download");

 protected:
  virtual bool ParseArgs() OVERRIDE;
  virtual void RunInternal() OVERRIDE;

 private:
  struct IOData {
   public:
    IOData();
    ~IOData();

    GURL url;
    string16 filename;
    bool save_as;
    base::ListValue* extra_headers;
    std::string method;
    std::string post_body;
    ResourceDispatcherHost* rdh;
    const content::ResourceContext* resource_context;
    int render_process_host_id;
    int render_view_host_routing_id;
  };
  void BeginDownloadOnIOThread();
  void OnStarted(DownloadId dl_id, net::Error error);
  void RespondOnUIThread(int dl_id, net::Error error);

  scoped_ptr<IOData> iodata_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsDownloadFunction);
};

class DownloadsSearchFunction : public SyncDownloadsFunction {
 public:
  DownloadsSearchFunction();
  virtual ~DownloadsSearchFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.search");

 protected:
  virtual bool ParseArgs() OVERRIDE;
  virtual void RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsSearchFunction);
};

class DownloadsPauseFunction : public SyncDownloadsFunction {
 public:
  DownloadsPauseFunction();
  virtual ~DownloadsPauseFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.pause");

 protected:
  virtual bool ParseArgs() OVERRIDE;
  virtual void RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsPauseFunction);
};

class DownloadsResumeFunction : public AsyncDownloadsFunction {
 public:
  DownloadsResumeFunction();
  virtual ~DownloadsResumeFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.resume");

 protected:
  virtual bool ParseArgs() OVERRIDE;
  virtual void RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsResumeFunction);
};

class DownloadsCancelFunction : public AsyncDownloadsFunction {
 public:
  DownloadsCancelFunction();
  virtual ~DownloadsCancelFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.cancel");

 protected:
  virtual bool ParseArgs() OVERRIDE;
  virtual void RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsCancelFunction);
};

class DownloadsEraseFunction : public AsyncDownloadsFunction {
 public:
  DownloadsEraseFunction();
  virtual ~DownloadsEraseFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.erase");

 protected:
  virtual bool ParseArgs() OVERRIDE;
  virtual void RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsEraseFunction);
};

class DownloadsSetDestinationFunction : public AsyncDownloadsFunction {
 public:
  DownloadsSetDestinationFunction();
  virtual ~DownloadsSetDestinationFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.setDestination");

 protected:
  virtual bool ParseArgs() OVERRIDE;
  virtual void RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsSetDestinationFunction);
};

class DownloadsAcceptDangerFunction : public AsyncDownloadsFunction {
 public:
  DownloadsAcceptDangerFunction();
  virtual ~DownloadsAcceptDangerFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.acceptDanger");

 protected:
  virtual bool ParseArgs() OVERRIDE;
  virtual void RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsAcceptDangerFunction);
};

class DownloadsShowFunction : public AsyncDownloadsFunction {
 public:
  DownloadsShowFunction();
  virtual ~DownloadsShowFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.show");

 protected:
  virtual bool ParseArgs() OVERRIDE;
  virtual void RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsShowFunction);
};

class DownloadsDragFunction : public AsyncDownloadsFunction {
 public:
  DownloadsDragFunction();
  virtual ~DownloadsDragFunction();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.drag");

 protected:
  virtual bool ParseArgs() OVERRIDE;
  virtual void RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsDragFunction);
};

class ExtensionDownloadsEventRouter : public DownloadManager::Observer {
 public:
  explicit ExtensionDownloadsEventRouter(Profile* profile);
  virtual ~ExtensionDownloadsEventRouter();

  virtual void ModelChanged() OVERRIDE;
  virtual void ManagerGoingDown() OVERRIDE;

 private:
  void DispatchEvent(const char* event_name, base::Value* json_arg);
  typedef base::hash_map<int, DownloadItem*> ItemMap;
  typedef std::set<int> DownloadIdSet;

  Profile* profile_;
  DownloadManager* manager_;
  DownloadIdSet downloads_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDownloadsEventRouter);
};
#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_EXTENSION_API_H_
