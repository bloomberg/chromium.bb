// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOADS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOADS_API_H_

#include <map>
#include <set>
#include <string>

#include "base/file_path.h"
#include "base/memory/singleton.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/download/all_download_item_notifier.h"
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

// Functions in the chrome.downloads namespace facilitate
// controlling downloads from extensions. See the full API doc at
// http://goo.gl/6hO1n

namespace download_extension_errors {

// Errors that can be returned through chrome.runtime.lastError.message.
extern const char kGenericError[];
extern const char kIconNotFoundError[];
extern const char kInvalidDangerTypeError[];
extern const char kInvalidFilterError[];
extern const char kInvalidOperationError[];
extern const char kInvalidOrderByError[];
extern const char kInvalidQueryLimit[];
extern const char kInvalidStateError[];
extern const char kInvalidURLError[];
extern const char kNotImplementedError[];

}  // namespace download_extension_errors


class DownloadsDownloadFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.download");
  DownloadsDownloadFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsDownloadFunction();

 private:
  void OnStarted(content::DownloadItem* item, net::Error error);

  DISALLOW_COPY_AND_ASSIGN(DownloadsDownloadFunction);
};

class DownloadsSearchFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.search");
  DownloadsSearchFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsSearchFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsSearchFunction);
};

class DownloadsPauseFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.pause");
  DownloadsPauseFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsPauseFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsPauseFunction);
};

class DownloadsResumeFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.resume");
  DownloadsResumeFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsResumeFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsResumeFunction);
};

class DownloadsCancelFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.cancel");
  DownloadsCancelFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsCancelFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsCancelFunction);
};

class DownloadsEraseFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.erase");
  DownloadsEraseFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsEraseFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsEraseFunction);
};

class DownloadsSetDestinationFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.setDestination");
  DownloadsSetDestinationFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsSetDestinationFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsSetDestinationFunction);
};

class DownloadsAcceptDangerFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.acceptDanger");
  DownloadsAcceptDangerFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsAcceptDangerFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsAcceptDangerFunction);
};

class DownloadsShowFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.show");
  DownloadsShowFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsShowFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsShowFunction);
};

class DownloadsOpenFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.open");
  DownloadsOpenFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsOpenFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsOpenFunction);
};

class DownloadsDragFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.drag");
  DownloadsDragFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsDragFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsDragFunction);
};

class DownloadsGetFileIconFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("downloads.getFileIcon");
  DownloadsGetFileIconFunction();
  virtual bool RunImpl() OVERRIDE;
  void SetIconExtractorForTesting(DownloadFileIconExtractor* extractor);

 protected:
  virtual ~DownloadsGetFileIconFunction();

 private:
  void OnIconURLExtracted(const std::string& url);
  FilePath path_;
  scoped_ptr<DownloadFileIconExtractor> icon_extractor_;
  DISALLOW_COPY_AND_ASSIGN(DownloadsGetFileIconFunction);
};

// Observes a single DownloadManager and many DownloadItems and dispatches
// onCreated and onErased events.
class ExtensionDownloadsEventRouter
  : public AllDownloadItemNotifier::Observer {
 public:
  explicit ExtensionDownloadsEventRouter(
      Profile* profile, content::DownloadManager* manager);
  virtual ~ExtensionDownloadsEventRouter();

  // AllDownloadItemNotifier::Observer interface
  virtual void OnDownloadCreated(
      content::DownloadManager* manager,
      content::DownloadItem* download_item) OVERRIDE;
  virtual void OnDownloadUpdated(
      content::DownloadManager* manager,
      content::DownloadItem* download_item) OVERRIDE;
  virtual void OnDownloadRemoved(
      content::DownloadManager* manager,
      content::DownloadItem* download_item) OVERRIDE;

  // Used for testing.
  struct DownloadsNotificationSource {
    std::string event_name;
    Profile* profile;
  };

 private:
  void DispatchEvent(const char* event_name, base::Value* json_arg);

  Profile* profile_;
  AllDownloadItemNotifier notifier_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDownloadsEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOADS_API_H_
