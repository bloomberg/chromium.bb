// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOADS_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOADS_API_H_

#include <string>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/download/all_download_item_notifier.h"
#include "chrome/browser/extensions/event_router.h"
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
extern const char kInvalidFilenameError[];
extern const char kInvalidFilterError[];
extern const char kInvalidOperationError[];
extern const char kInvalidOrderByError[];
extern const char kInvalidQueryLimit[];
extern const char kInvalidStateError[];
extern const char kInvalidURLError[];
extern const char kNotImplementedError[];
extern const char kTooManyListenersError[];

}  // namespace download_extension_errors


class DownloadsDownloadFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.download", DOWNLOADS_DOWNLOAD)
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
  DECLARE_EXTENSION_FUNCTION("downloads.search", DOWNLOADS_SEARCH)
  DownloadsSearchFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsSearchFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsSearchFunction);
};

class DownloadsPauseFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.pause", DOWNLOADS_PAUSE)
  DownloadsPauseFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsPauseFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsPauseFunction);
};

class DownloadsResumeFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.resume", DOWNLOADS_RESUME)
  DownloadsResumeFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsResumeFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsResumeFunction);
};

class DownloadsCancelFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.cancel", DOWNLOADS_CANCEL)
  DownloadsCancelFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsCancelFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsCancelFunction);
};

class DownloadsEraseFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.erase", DOWNLOADS_ERASE)
  DownloadsEraseFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsEraseFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsEraseFunction);
};

class DownloadsAcceptDangerFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.acceptDanger", DOWNLOADS_ACCEPTDANGER)
  DownloadsAcceptDangerFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsAcceptDangerFunction();
  void DangerPromptCallback(bool accept, int download_id);

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsAcceptDangerFunction);
};

class DownloadsShowFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.show", DOWNLOADS_SHOW)
  DownloadsShowFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsShowFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsShowFunction);
};

class DownloadsOpenFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.open", DOWNLOADS_OPEN)
  DownloadsOpenFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsOpenFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsOpenFunction);
};

class DownloadsDragFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.drag", DOWNLOADS_DRAG)
  DownloadsDragFunction();
  virtual bool RunImpl() OVERRIDE;

 protected:
  virtual ~DownloadsDragFunction();

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadsDragFunction);
};

class DownloadsGetFileIconFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("downloads.getFileIcon", DOWNLOADS_GETFILEICON)
  DownloadsGetFileIconFunction();
  virtual bool RunImpl() OVERRIDE;
  void SetIconExtractorForTesting(DownloadFileIconExtractor* extractor);

 protected:
  virtual ~DownloadsGetFileIconFunction();

 private:
  void OnIconURLExtracted(const std::string& url);
  base::FilePath path_;
  scoped_ptr<DownloadFileIconExtractor> icon_extractor_;
  DISALLOW_COPY_AND_ASSIGN(DownloadsGetFileIconFunction);
};

// Observes a single DownloadManager and many DownloadItems and dispatches
// onCreated and onErased events.
class ExtensionDownloadsEventRouter : public extensions::EventRouter::Observer,
                                      public AllDownloadItemNotifier::Observer {
 public:
  typedef base::Callback<void(const base::FilePath& changed_filename,
                              bool overwrite)> FilenameChangedCallback;

  // A downloads.onDeterminingFilename listener has returned. If the extension
  // wishes to override the download's filename, then |filename| will be
  // non-empty. |filename| will be interpreted as a relative path, appended to
  // the default downloads directory. If the extension wishes to overwrite any
  // existing files, then |overwrite| will be true. Returns true on success,
  // false otherwise.
  static bool DetermineFilename(
      Profile* profile,
      bool include_incognito,
      const std::string& ext_id,
      int download_id,
      const base::FilePath& filename,
      bool overwrite,
      std::string* error);

  explicit ExtensionDownloadsEventRouter(
      Profile* profile, content::DownloadManager* manager);
  virtual ~ExtensionDownloadsEventRouter();

  // Called by ChromeDownloadManagerDelegate during the filename determination
  // process, allows extensions to change the item's target filename. If no
  // extension wants to change the target filename, then |no_change| will be
  // called and the filename determination process will continue as normal. If
  // an extension wants to change the target filename, then |change| will be
  // called with the new filename and a flag indicating whether the new file
  // should overwrite any old files of the same name.
  void OnDeterminingFilename(
      content::DownloadItem* item,
      const base::FilePath& suggested_path,
      const base::Closure& no_change,
      const FilenameChangedCallback& change);

  // AllDownloadItemNotifier::Observer
  virtual void OnDownloadCreated(
      content::DownloadManager* manager,
      content::DownloadItem* download_item) OVERRIDE;
  virtual void OnDownloadUpdated(
      content::DownloadManager* manager,
      content::DownloadItem* download_item) OVERRIDE;
  virtual void OnDownloadRemoved(
      content::DownloadManager* manager,
      content::DownloadItem* download_item) OVERRIDE;

  // extensions::EventRouter::Observer
  virtual void OnListenerRemoved(
      const extensions::EventListenerInfo& details) OVERRIDE;

  // Used for testing.
  struct DownloadsNotificationSource {
    std::string event_name;
    Profile* profile;
  };

 private:
  void DispatchEvent(
      const char* event_name,
      bool include_incognito,
      const extensions::Event::WillDispatchCallback& will_dispatch_callback,
      base::Value* json_arg);

  Profile* profile_;
  AllDownloadItemNotifier notifier_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDownloadsEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOWNLOADS_DOWNLOADS_API_H_
