// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installation_task.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_data_retriever.h"
#include "chrome/browser/web_applications/extensions/bookmark_app_installer.h"

namespace extensions {

BookmarkAppInstallationTask::Result::Result(ResultCode code,
                                            const std::string& app_id)
    : code(code), app_id(app_id) {
  DCHECK_EQ(code == ResultCode::kSuccess, !app_id.empty());
}

BookmarkAppInstallationTask::Result::Result(Result&&) = default;

BookmarkAppInstallationTask::Result::~Result() = default;

BookmarkAppInstallationTask::~BookmarkAppInstallationTask() = default;

void BookmarkAppInstallationTask::SetDataRetrieverForTesting(
    std::unique_ptr<BookmarkAppDataRetriever> data_retriever) {
  data_retriever_ = std::move(data_retriever);
}

void BookmarkAppInstallationTask::SetInstallerForTesting(
    std::unique_ptr<BookmarkAppInstaller> installer) {
  installer_ = std::move(installer);
}

BookmarkAppInstallationTask::BookmarkAppInstallationTask(Profile* profile)
    : data_retriever_(std::make_unique<BookmarkAppDataRetriever>()),
      installer_(std::make_unique<BookmarkAppInstaller>(profile)) {}

}  // namespace extensions
