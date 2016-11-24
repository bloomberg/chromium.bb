// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/url_downloader.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ptr_util.h"
#include "base/path_service.h"
#include "components/reading_list/ios/offline_url_utils.h"
#include "ios/chrome/browser/chrome_paths.h"
#include "ios/chrome/browser/dom_distiller/distiller_viewer.h"
#include "ios/web/public/web_thread.h"
#include "net/base/escape.h"
#include "url/gurl.h"

// URLDownloader

URLDownloader::URLDownloader(
    dom_distiller::DomDistillerService* distiller_service,
    PrefService* prefs,
    base::FilePath chrome_profile_path,
    const DownloadCompletion& download_completion,
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
      FROM_HERE,
      base::Bind(&base::PathExists,
                 reading_list::OfflinePageAbsolutePath(base_directory_, url)),
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

void URLDownloader::DownloadCompletionHandler(const GURL& url,
                                              const std::string& title,
                                              SuccessState success) {
  DCHECK(working_);

  auto post_delete = base::Bind(
      [](URLDownloader* _this, const GURL& url, const std::string& title,
         SuccessState success) {
        _this->download_completion_.Run(
            url, success, reading_list::OfflinePagePath(url), title);
        _this->distiller_.reset();
        _this->working_ = false;
        _this->HandleNextTask();
      },
      base::Unretained(this), url, title, success);

  // If downloading failed, clean up any partial download.
  if (success == ERROR_RETRY || success == ERROR_PERMANENT) {
    task_tracker_.PostTaskAndReply(
        web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE).get(),
        FROM_HERE, base::Bind(
                       [](const base::FilePath& offline_directory_path) {
                         base::DeleteFile(offline_directory_path, true);
                       },
                       reading_list::OfflineURLDirectoryAbsolutePath(
                           base_directory_, url)),
        post_delete);
  } else {
    post_delete.Run();
  }
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
        FROM_HERE, base::Bind(&base::DeleteFile,
                              reading_list::OfflineURLDirectoryAbsolutePath(
                                  base_directory_, url),
                              true),
        base::Bind(&URLDownloader::DeleteCompletionHandler,
                   base::Unretained(this), url));
  } else if (task.first == DOWNLOAD) {
    DCHECK(!distiller_);
    OfflineURLExists(url, base::Bind(&URLDownloader::DownloadURL,
                                     base::Unretained(this), url));
  }
}

void URLDownloader::DownloadURL(GURL url, bool offline_url_exists) {
  if (offline_url_exists) {
    DownloadCompletionHandler(url, std::string(), DOWNLOAD_EXISTS);
    return;
  }

  distiller_.reset(new dom_distiller::DistillerViewer(
      distiller_service_, pref_service_, url,
      base::Bind(&URLDownloader::DistillerCallback, base::Unretained(this))));
}

void URLDownloader::DistillerCallback(
    const GURL& page_url,
    const std::string& html,
    const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
        images,
    const std::string& title) {
  if (html.empty()) {
    DownloadCompletionHandler(page_url, std::string(), ERROR_RETRY);
    return;
  }

  std::vector<dom_distiller::DistillerViewer::ImageInfo> images_block = images;
  std::string block_html = html;
  task_tracker_.PostTaskAndReplyWithResult(
      web::WebThread::GetTaskRunnerForThread(web::WebThread::FILE).get(),
      FROM_HERE,
      base::Bind(&URLDownloader::SaveDistilledHTML, base::Unretained(this),
                 page_url, images_block, block_html),
      base::Bind(&URLDownloader::DownloadCompletionHandler,
                 base::Unretained(this), page_url, title));
}

URLDownloader::SuccessState URLDownloader::SaveDistilledHTML(
    const GURL& url,
    const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
        images,
    const std::string& html) {
  if (CreateOfflineURLDirectory(url)) {
    return SaveHTMLForURL(SaveAndReplaceImagesInHTML(url, html, images), url)
               ? DOWNLOAD_SUCCESS
               : ERROR_PERMANENT;
  }
  return ERROR_PERMANENT;
}

bool URLDownloader::CreateOfflineURLDirectory(const GURL& url) {
  base::FilePath path =
      reading_list::OfflineURLDirectoryAbsolutePath(base_directory_, url);
  if (!DirectoryExists(path)) {
    return CreateDirectoryAndGetError(path, nil);
  }
  return true;
}

bool URLDownloader::SaveImage(const GURL& url,
                              const GURL& image_url,
                              const std::string& data,
                              std::string* image_name) {
  std::string image_hash = base::MD5String(image_url.spec());
  *image_name = image_hash;
  base::FilePath path =
      reading_list::OfflineURLDirectoryAbsolutePath(base_directory_, url)
          .Append(image_hash);
  if (!base::PathExists(path)) {
    return base::WriteFile(path, data.c_str(), data.length()) > 0;
  }
  return true;
}

std::string URLDownloader::SaveAndReplaceImagesInHTML(
    const GURL& url,
    const std::string& html,
    const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
        images) {
  std::string mutable_html = html;
  for (size_t i = 0; i < images.size(); i++) {
    base::FilePath local_image_path;
    std::string local_image_name;
    if (!SaveImage(url, images[i].url, images[i].data, &local_image_name)) {
      return std::string();
    }
    std::string image_url = net::EscapeForHTML(images[i].url.spec());
    size_t image_url_size = image_url.size();
    size_t pos = mutable_html.find(image_url, 0);
    while (pos != std::string::npos) {
      mutable_html.replace(pos, image_url_size, local_image_name);
      pos = mutable_html.find(image_url, pos + local_image_name.size());
    }
  }
  return mutable_html;
}

bool URLDownloader::SaveHTMLForURL(std::string html, const GURL& url) {
  if (html.empty()) {
    return false;
  }
  base::FilePath path =
      reading_list::OfflinePageAbsolutePath(base_directory_, url);
  return base::WriteFile(path, html.c_str(), html.length()) > 0;
}
