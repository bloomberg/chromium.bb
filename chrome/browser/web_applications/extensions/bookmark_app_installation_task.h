// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"

class Profile;
enum class WebappInstallSource;
struct WebApplicationInfo;

namespace content {
class WebContents;
}

namespace extensions {

class BookmarkAppDataRetriever;
class BookmarkAppHelper;
class BookmarkAppInstaller;
class Extension;

// Class to install a BookmarkApp-based Shortcut or WebApp from a WebContents
// or WebApplicationInfo. Can only be called from the UI thread.
class BookmarkAppInstallationTask {
 public:
  enum class ResultCode {
    kSuccess,
    kGetWebApplicationInfoFailed,
    kInstallationFailed,
  };

  struct Result {
    Result(ResultCode code, const std::string& app_id);
    Result(Result&&);
    ~Result();

    const ResultCode code;
    // Empty unless |code| is ResultCode::kSuccess.
    std::string app_id;

    DISALLOW_COPY_AND_ASSIGN(Result);
  };

  using ResultCallback = base::OnceCallback<void(Result)>;
  using BookmarkAppHelperFactory =
      base::RepeatingCallback<std::unique_ptr<BookmarkAppHelper>(
          Profile*,
          const WebApplicationInfo&,
          content::WebContents*,
          WebappInstallSource)>;

  // Ensures the tab helpers necessary for installing an app are present.
  static void CreateTabHelpers(content::WebContents* web_contents);

  explicit BookmarkAppInstallationTask(Profile* profile);

  virtual ~BookmarkAppInstallationTask();

  virtual void InstallWebAppOrShortcutFromWebContents(
      content::WebContents* web_contents,
      ResultCallback callback);

  void SetBookmarkAppHelperFactoryForTesting(
      BookmarkAppHelperFactory helper_factory);
  void SetDataRetrieverForTesting(
      std::unique_ptr<BookmarkAppDataRetriever> data_retriever);
  void SetInstallerForTesting(std::unique_ptr<BookmarkAppInstaller> installer);

 protected:
  BookmarkAppDataRetriever& data_retriever() { return *data_retriever_; }
  BookmarkAppInstaller& installer() { return *installer_; }

 private:
  void OnGetWebApplicationInfo(
      ResultCallback result_callback,
      content::WebContents* web_contents,
      std::unique_ptr<WebApplicationInfo> web_app_info);
  void OnInstalled(ResultCallback result_callback,
                   const Extension* extension,
                   const WebApplicationInfo& web_app_info);

  Profile* profile_;

  // We temporarily use a BookmarkAppHelper until the WebApp and WebShortcut
  // installation tasks reach feature parity with BookmarkAppHelper.
  std::unique_ptr<BookmarkAppHelper> helper_;
  BookmarkAppHelperFactory helper_factory_;

  std::unique_ptr<BookmarkAppDataRetriever> data_retriever_;
  std::unique_ptr<BookmarkAppInstaller> installer_;

  base::WeakPtrFactory<BookmarkAppInstallationTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallationTask);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_
