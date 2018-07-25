// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"

namespace extensions {

class BookmarkAppDataRetriever;

// Class to install a BookmarkApp-based Shortcut or WebApp from a WebContents
// or WebApplicationInfo. Can only be called from the UI thread.
class BookmarkAppInstallationTask {
 public:
  enum class Result {
    kSuccess,
    kGetWebApplicationInfoFailed,
  };

  using ResultCallback = base::OnceCallback<void(Result)>;

  virtual ~BookmarkAppInstallationTask();

  void SetDataRetrieverForTesting(
      std::unique_ptr<BookmarkAppDataRetriever> data_retriever);

 protected:
  BookmarkAppInstallationTask();

  BookmarkAppDataRetriever& data_retriever() { return *data_retriever_; }

 private:
  std::unique_ptr<BookmarkAppDataRetriever> data_retriever_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallationTask);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALLATION_TASK_H_
