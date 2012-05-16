// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/stl_util.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

class DownloadFileIconExtractor;
class DownloadQuery;

namespace content {
class ResourceContext;
class ResourceDispatcherHost;
}

// Functions in the chrome.experimental.downloads namespace facilitate
// controlling downloads from extensions. See the full API doc at
// http://goo.gl/6hO1n

namespace download_extension_errors {

// Errors that can be returned through chrome.extension.lastError.message.
extern const char kGenericError[];
extern const char kIconNotFoundError[];
extern const char kInvalidDangerTypeError[];
extern const char kInvalidFilterError[];
extern const char kInvalidOperationError[];
extern const char kInvalidOrderByError[];
extern const char kInvalidQueryLimit[];
extern const char kInvalidStateError[];
extern const char kInvalidUrlError[];
extern const char kNotImplementedError[];

}  // namespace download_extension_errors

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
    DOWNLOADS_FUNCTION_GET_FILE_ICON = 10,
    // Insert new values here, not at the beginning.
    DOWNLOADS_FUNCTION_LAST
  };

 protected:
  // Return true if args_ is well-formed, otherwise set error_ and return false.
  virtual bool ParseArgs() = 0;

  // Implementation-specific logic. "Do the thing that you do."  Should return
  // true if the call succeeded and false otherwise.
  virtual bool RunInternal() = 0;

  // Which subclass is this.
  virtual DownloadsFunctionName function() const = 0;

  // Wrap ParseArgs(), RunInternal().
  static bool RunImplImpl(DownloadsFunctionInterface* pimpl);
};

class SyncDownloadsFunction : public SyncExtensionFunction,
                              public DownloadsFunctionInterface {
 protected:
  explicit SyncDownloadsFunction(DownloadsFunctionName function);
  virtual ~SyncDownloadsFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // DownloadsFunctionInterface:
  virtual DownloadsFunctionName function() const OVERRIDE;

  content::DownloadItem* GetActiveItem(int download_id);

 private:
  DownloadsFunctionName function_;

  DISALLOW_COPY_AND_ASSIGN(SyncDownloadsFunction);
};

class AsyncDownloadsFunction : public AsyncExtensionFunction,
                               public DownloadsFunctionInterface {
 protected:
  explicit AsyncDownloadsFunction(DownloadsFunctionName function);
  virtual ~AsyncDownloadsFunction();

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

  // DownloadsFunctionInterface:
  virtual DownloadsFunctionName function() const OVERRIDE;

  content::DownloadItem* GetActiveItem(int download_id);

 private:
  DownloadsFunctionName function_;

  DISALLOW_COPY_AND_ASSIGN(AsyncDownloadsFunction);
};

class DownloadsDownloadFunction : public AsyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.download");

  DownloadsDownloadFunction();

 protected:
  virtual ~DownloadsDownloadFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

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
    content::ResourceDispatcherHost* rdh;
    content::ResourceContext* resource_context;
    int render_process_host_id;
    int render_view_host_routing_id;
  };

  void BeginDownloadOnIOThread();
  void OnStarted(content::DownloadId dl_id, net::Error error);

  scoped_ptr<IOData> iodata_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsDownloadFunction);
};

class DownloadsSearchFunction : public SyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.search");

  DownloadsSearchFunction();

 protected:
  virtual ~DownloadsSearchFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

 private:
  bool ParseOrderBy(const base::Value& order_by_value);

  scoped_ptr<DownloadQuery> query_;
  int get_id_;
  bool has_get_id_;

  DISALLOW_COPY_AND_ASSIGN(DownloadsSearchFunction);
};

class DownloadsPauseFunction : public SyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.pause");

  DownloadsPauseFunction();

 protected:
  virtual ~DownloadsPauseFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

 private:
  int download_id_;
  DISALLOW_COPY_AND_ASSIGN(DownloadsPauseFunction);
};

class DownloadsResumeFunction : public SyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.resume");

  DownloadsResumeFunction();

 protected:
  virtual ~DownloadsResumeFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

 private:
  int download_id_;
  DISALLOW_COPY_AND_ASSIGN(DownloadsResumeFunction);
};

class DownloadsCancelFunction : public SyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.cancel");

  DownloadsCancelFunction();

 protected:
  virtual ~DownloadsCancelFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

 private:
  int download_id_;
  DISALLOW_COPY_AND_ASSIGN(DownloadsCancelFunction);
};

class DownloadsEraseFunction : public AsyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.erase");

  DownloadsEraseFunction();

 protected:
  virtual ~DownloadsEraseFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsEraseFunction);
};

class DownloadsSetDestinationFunction : public AsyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.setDestination");

  DownloadsSetDestinationFunction();

 protected:
  virtual ~DownloadsSetDestinationFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsSetDestinationFunction);
};

class DownloadsAcceptDangerFunction : public AsyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.acceptDanger");

  DownloadsAcceptDangerFunction();

 protected:
  virtual ~DownloadsAcceptDangerFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsAcceptDangerFunction);
};

class DownloadsShowFunction : public AsyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.show");

  DownloadsShowFunction();

 protected:
  virtual ~DownloadsShowFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsShowFunction);
};

class DownloadsDragFunction : public AsyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.drag");

  DownloadsDragFunction();

 protected:
  virtual ~DownloadsDragFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsDragFunction);
};

class DownloadsGetFileIconFunction : public AsyncDownloadsFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.downloads.getFileIcon");

  DownloadsGetFileIconFunction();
  void SetIconExtractorForTesting(DownloadFileIconExtractor* extractor);

 protected:
  virtual ~DownloadsGetFileIconFunction();

  // DownloadsFunctionInterface:
  virtual bool ParseArgs() OVERRIDE;
  virtual bool RunInternal() OVERRIDE;

 private:
  void OnIconURLExtracted(const std::string& url);
  FilePath path_;
  int icon_size_;
  scoped_ptr<DownloadFileIconExtractor> icon_extractor_;
  DISALLOW_COPY_AND_ASSIGN(DownloadsGetFileIconFunction);
};

// Observes a single DownloadManager and many DownloadItems and dispatches
// onCreated and onErased events.
class ExtensionDownloadsEventRouter : public content::DownloadManager::Observer,
                                      public content::DownloadItem::Observer {
 public:
  explicit ExtensionDownloadsEventRouter(Profile* profile);
  virtual ~ExtensionDownloadsEventRouter();

  virtual void ModelChanged(content::DownloadManager* manager) OVERRIDE;
  virtual void ManagerGoingDown(content::DownloadManager* manager) OVERRIDE;
  virtual void OnDownloadUpdated(content::DownloadItem* download) OVERRIDE;
  virtual void OnDownloadOpened(content::DownloadItem* download) OVERRIDE;

 private:
  struct OnChangedStat {
    OnChangedStat();
    ~OnChangedStat();
    int fires;
    int total;
  };

  typedef std::map<int, content::DownloadItem*> ItemMap;
  typedef std::map<int, base::DictionaryValue*> ItemJsonMap;
  typedef std::map<int, OnChangedStat*> OnChangedStatMap;

  void Init(content::DownloadManager* manager);
  void DispatchEvent(const char* event_name, base::Value* json_arg);

  Profile* profile_;
  content::DownloadManager* manager_;
  ItemMap downloads_;
  ItemJsonMap item_jsons_;
  STLValueDeleter<ItemJsonMap> delete_item_jsons_;
  OnChangedStatMap on_changed_stats_;
  STLValueDeleter<OnChangedStatMap> delete_on_changed_stats_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDownloadsEventRouter);
};
#endif  // CHROME_BROWSER_DOWNLOAD_DOWNLOAD_EXTENSION_API_H_
