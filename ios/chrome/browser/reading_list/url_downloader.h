// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_READING_LIST_URL_DOWNLOADER_H_
#define IOS_CHROME_BROWSER_READING_LIST_URL_DOWNLOADER_H_

#include <queue>

#include "base/callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "ios/chrome/browser/dom_distiller/distiller_viewer.h"

class PrefService;
class GURL;
namespace base {
class FilePath;
}

namespace dom_distiller {
class DomDistillerService;
}

// This class downloads and deletes offline versions of URLs using DOM distiller
// to fetch the page and simplify it. Only one item is downloaded or deleted at
// a time using a queue of tasks that are handled sequentially. Items (page +
// images) are saved to individual folders within an offline folder, using md5
// hashing to create unique file names. When a deletion is requested, all
// previous downloads for that URL are cancelled as they would be deleted.
class URLDownloader {
  friend class MockURLDownloader;

 public:
  // And enum indicating different download outcomes.
  enum SuccessState {
    DOWNLOAD_SUCCESS,
    DOWNLOAD_EXISTS,
    ERROR_RETRY,
    ERROR_PERMANENT
  };

  // A completion callback that takes a GURL and a bool indicating the
  // outcome and returns void.
  using SuccessCompletion = base::Callback<void(const GURL&, bool)>;

  // A download completion callback that takes, in order, the GURL that was
  // downloaded, a SuccessState indicating the outcome of the download, the
  // path to the downloaded page (relative to |OfflineRootDirectoryPath()|, and
  // the title of the url, and returns void.
  // The path to downloaded file  and title should not be used in case of
  // failure.
  using DownloadCompletion = base::Callback<void(const GURL&,
                                                 SuccessState,
                                                 const base::FilePath&,
                                                 const std::string&)>;

  // Create a URL downloader with completion callbacks for downloads and
  // deletions. The completion callbacks will be called with the original url
  // and a boolean indicating success. For downloads, if distillation was
  // successful, it will also include the distilled url and extracted title.
  URLDownloader(dom_distiller::DomDistillerService* distiller_service,
                PrefService* prefs,
                base::FilePath chrome_profile_path,
                const DownloadCompletion& download_completion,
                const SuccessCompletion& delete_completion);
  virtual ~URLDownloader();

  // Asynchronously download an offline version of the URL.
  void DownloadOfflineURL(const GURL& url);

  // Cancels the download job an offline version of the URL.
  void CancelDownloadOfflineURL(const GURL& url);

  // Asynchronously remove the offline version of the URL if it exists.
  void RemoveOfflineURL(const GURL& url);

 private:
  enum TaskType { DELETE, DOWNLOAD };
  using Task = std::pair<TaskType, GURL>;

  // Calls callback with true if an offline file exists for this URL.
  void OfflineURLExists(const GURL& url, base::Callback<void(bool)> callback);
  // Handles the next task in the queue, if no task is currently being handled.
  void HandleNextTask();
  // Callback for completed (or failed) download, handles calling
  // downloadCompletion and starting the next task.
  void DownloadCompletionHandler(const GURL& url,
                                 const std::string& title,
                                 SuccessState success);
  // Callback for completed (or failed) deletion, handles calling
  // deleteCompletion and starting the next task.
  void DeleteCompletionHandler(const GURL& url, bool success);

  // Creates the offline directory for |url|. Returns true if successful or if
  // the directory already exists.
  bool CreateOfflineURLDirectory(const GURL& url);
  // Saves the |data| for image at |imageURL| to disk, for main URL |url|;
  // puts path of saved file in |path| and returns whether save was successful.
  bool SaveImage(const GURL& url,
                 const GURL& imageURL,
                 const std::string& data,
                 std::string* image_name);
  // Saves images in |images| array to disk and replaces references in |html| to
  // local path. Returns updated html.
  std::string SaveAndReplaceImagesInHTML(
      const GURL& url,
      const std::string& html,
      const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
          images);
  // Saves |html| to disk in the correct location for |url|; returns success.
  bool SaveHTMLForURL(std::string html, const GURL& url);
  // Downloads |url|, depending on |offlineURLExists| state.
  virtual void DownloadURL(GURL url, bool offlineURLExists);
  // Saves distilled html to disk, including saving images and main file.
  SuccessState SaveDistilledHTML(
      const GURL& url,
      const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
          images,
      const std::string& html);
  // Callback for distillation completion.
  void DistillerCallback(
      const GURL& pageURL,
      const std::string& html,
      const std::vector<dom_distiller::DistillerViewerInterface::ImageInfo>&
          images,
      const std::string& title);

  dom_distiller::DomDistillerService* distiller_service_;
  PrefService* pref_service_;
  const DownloadCompletion download_completion_;
  const SuccessCompletion delete_completion_;
  std::deque<Task> tasks_;
  bool working_;
  base::FilePath base_directory_;
  std::unique_ptr<dom_distiller::DistillerViewerInterface> distiller_;
  base::CancelableTaskTracker task_tracker_;

  DISALLOW_COPY_AND_ASSIGN(URLDownloader);
};

#endif  // IOS_CHROME_BROWSER_READING_LIST_URL_DOWNLOADER_H_
