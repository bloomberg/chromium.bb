// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"

class Profile;

namespace extensions {

class BookmarkAppDataRetriever;
class BookmarkAppInstaller;

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

  virtual ~BookmarkAppInstallationTask();

  void SetDataRetrieverForTesting(
      std::unique_ptr<BookmarkAppDataRetriever> data_retriever);
  void SetInstallerForTesting(std::unique_ptr<BookmarkAppInstaller> installer);

 protected:
  explicit BookmarkAppInstallationTask(Profile* profile);

  BookmarkAppDataRetriever& data_retriever() { return *data_retriever_; }
  BookmarkAppInstaller& installer() { return *installer_; }

 private:
  std::unique_ptr<BookmarkAppDataRetriever> data_retriever_;
  std::unique_ptr<BookmarkAppInstaller> installer_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallationTask);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_
