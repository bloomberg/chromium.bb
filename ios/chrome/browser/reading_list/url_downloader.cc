// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/url_downloader.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/md5.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/dom_distiller/distiller_viewer.h"
#include "ios/web/public/web_thread.h"
#include "url/gurl.h"

namespace {
char const kOfflineDirectory[] = "Offline";

// TODO(crbug.com/629771): Handle errors & retrying of failed saves, including
// distillation failure.

}  // namespace

// URLDownloader

URLDownloader::URLDownloader(
    dom_distiller::DomDistillerService* distiller_service,
    PrefService* prefs,
    base::FilePath chrome_profile_path,
    const SuccessCompletion& download_completion,
    const SuccessCompletion& delete_completion)
    : distiller_service_(distiller_service),
      pref_service_(prefs),
      download_completion_(download_completion),
      delete_completion_(delete_completion),
      working_(false),
      base_directory_(chrome_profile_path),
      task_tracker_() {}

URLDownloader::~URLDownloader() {
  task_tracker_.TryCancelAll();
}

void URLDownloader::OfflineURLExists(const GURL& url,
                                     base::Callback<void(bool)> callback) {
  task_tracker_.PostTaskAndReplyWithResult(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE).get(),
      FROM_HERE, base::Bind(&base::PathExists, OfflineURLPagePath(url)),
      callback);
}

void URLDownloader::RemoveOfflineURL(const GURL& url) {
  // Remove all download tasks for this url as it would be pointless work.
  tasks_.erase(
      std::remove(tasks_.begin(), tasks_.end(), std::make_pair(DOWNLOAD, url)),
      tasks_.end());
  tasks_.push_back(std::make_pair(DELETE, url));
  HandleNextTask();
}

void URLDownloader::DownloadOfflineURL(const GURL& url) {
  if (std::find(tasks_.begin(), tasks_.end(), std::make_pair(DOWNLOAD, url)) ==
      tasks_.end()) {
    tasks_.push_back(std::make_pair(DOWNLOAD, url));
    HandleNextTask();
  }
}

void URLDownloader::DownloadCompletionHandler(const GURL& url, bool success) {
  DCHECK(working_);
  download_completion_.Run(url, success);
  distiller_.reset();
  working_ = false;
  HandleNextTask();
}

void URLDownloader::DeleteCompletionHandler(const GURL& url, bool success) {
  DCHECK(working_);
  delete_completion_.Run(url, success);
  working_ = false;
  HandleNextTask();
}

void URLDownloader::HandleNextTask() {
  if (working_ || tasks_.empty()) {
    return;
  }
  working_ = true;

  Task task = tasks_.front();
  tasks_.pop_front();
  GURL url = task.second;

  if (task.first == DELETE) {
    task_tracker_.PostTaskAndReplyWithResult(
        web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE).get(),
        FROM_HERE,
        base::Bind(&base::DeleteFile, OfflineURLDirectoryPath(url), true),
        base::Bind(&URLDownloader::DeleteCompletionHandler,
                   base::Unretained(this), url));
  } else if (task.first == DOWNLOAD) {
    DCHECK(!distiller_);
    OfflineURLExists(url, base::Bind(&URLDownloader::DownloadURL,
                                     base::Unretained(this), url));
  }
}

void URLDownloader::DownloadURL(GURL url, bool offlineURLExists) {
  if (offlineURLExists) {
    DownloadCompletionHandler(url, false);
    return;
  }

  distiller_.reset(new dom_distiller::DistillerViewerImpl(
      distiller_service_, pref_service_, url,
      base::Bind(&URLDownloader::DistillerCallback, base::Unretained(this))));
}

void URLDownloader::DistillerCallback(
    const GURL& pageURL,
    const std::string& html,
    const std::vector<dom_distiller::DistillerViewer::ImageInfo>& images) {
  std::vector<dom_distiller::DistillerViewer::ImageInfo> imagesBlock = images;
  std::string blockHTML = html;
  task_tracker_.PostTaskAndReplyWithResult(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE).get(),
      FROM_HERE,
      base::Bind(&URLDownloader::SaveDistilledHTML, base::Unretained(this),
                 pageURL, imagesBlock, blockHTML),
      base::Bind(&URLDownloader::DownloadCompletionHandler,
                 base::Unretained(this), pageURL));
}

bool URLDownloader::SaveDistilledHTML(
    const GURL& url,
    std::vector<dom_distiller::DistillerViewer::ImageInfo> images,
    std::string html) {
  if (CreateOfflineURLDirectory(url)) {
    return SaveHTMLForURL(SaveAndReplaceImagesInHTML(url, html, images), url);
  }
  return false;
}

base::FilePath URLDownloader::OfflineDirectoryPath() {
  return base_directory_.Append(kOfflineDirectory);
}

base::FilePath URLDownloader::OfflineURLDirectoryPath(const GURL& url) {
  std::string hash = base::MD5String(url.spec());
  return OfflineDirectoryPath().Append(hash);
}

base::FilePath URLDownloader::OfflineURLPagePath(const GURL& url) {
  return OfflineURLDirectoryPath(url).Append("page.html");
}

bool URLDownloader::CreateOfflineURLDirectory(const GURL& url) {
  base::FilePath path = OfflineURLDirectoryPath(url);
  if (!DirectoryExists(path)) {
    return CreateDirectoryAndGetError(path, nil);
  }
  return true;
}

std::string URLDownloader::SaveImage(const GURL& url,
                                     const GURL& imageURL,
                                     const std::string& data) {
  base::FilePath path =
      OfflineURLDirectoryPath(url).Append(base::MD5String(imageURL.spec()));
  if (!base::PathExists(path)) {
    base::WriteFile(path, data.c_str(), data.length());
  }
  return path.AsUTF8Unsafe();
}

// TODO(crbug.com/625621) DomDistiller doesn't correctly handle srcset
// attributes in img tags.
std::string URLDownloader::SaveAndReplaceImagesInHTML(
    const GURL& url,
    const std::string& html,
    const std::vector<dom_distiller::DistillerViewer::ImageInfo>& images) {
  std::string mutableHTML = html;
  for (size_t i = 0; i < images.size(); i++) {
    const std::string& localImagePath =
        SaveImage(url, images[i].url, images[i].data);
    const std::string& imageURL = images[i].url.spec();
    size_t imageURLSize = imageURL.size();
    size_t pos = mutableHTML.find(imageURL, 0);
    while (pos != std::string::npos) {
      mutableHTML.replace(pos, imageURLSize, localImagePath);
      pos = mutableHTML.find(imageURL, pos + imageURLSize);
    }
  }
  return mutableHTML;
}

bool URLDownloader::SaveHTMLForURL(std::string html, const GURL& url) {
  base::FilePath path = OfflineURLPagePath(url);
  return base::WriteFile(path, html.c_str(), html.length()) < 0;
}