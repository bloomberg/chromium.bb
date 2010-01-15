// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/drag_download_file_win.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"

DragDownloadFile::DragDownloadFile(
    const GURL& url,
    const GURL& referrer,
    const std::string& referrer_encoding,
    TabContents* tab_contents)
    : url_(url),
      referrer_(referrer),
      referrer_encoding_(referrer_encoding),
      tab_contents_(tab_contents),
      drag_message_loop_(MessageLoop::current()),
      is_started_(false),
      is_running_nested_message_loop_(false),
      initiate_download_result_(false),
      format_(0),
      download_manager_(NULL),
      download_item_observer_added_(false) {
}

DragDownloadFile::~DragDownloadFile() {
  DCHECK(drag_message_loop_ == MessageLoop::current());

  // Since the target application can still hold and use the dragged file,
  // we do not know the time that it can be safely deleted. To solve this
  // problem, we schedule it to be removed after the system is restarted.
#if defined(OS_WIN)
  if (!dir_path_.empty()) {
    if (!file_path_.empty())
      file_util::DeleteAfterReboot(file_path_);
    file_util::DeleteAfterReboot(dir_path_);
  }
#endif

  if (download_manager_)
    download_manager_->RemoveObserver(this);
}

bool DragDownloadFile::Start(OSExchangeData::DownloadFileObserver* observer,
                             int format) {
  DCHECK(drag_message_loop_ == MessageLoop::current());

  if (is_started_)
    return true;
  is_started_ = true;

  DCHECK(!observer_.get());
  observer_ = observer;
  format_ = format;

  if (!InitiateDownload())
    return false;

  // Wait till the download is fully initiated.
  StartNestedMessageLoop();

  return initiate_download_result_;
}

void DragDownloadFile::Stop() {
}

bool DragDownloadFile::InitiateDownload() {
  DCHECK(drag_message_loop_ == MessageLoop::current());

  // Create a temporary directory to save the temporary download file. We do
  // not want to use the default download directory since we do not want the
  // twisted file name shown in the download shelf if the file with the same
  // name already exists.
  if (!file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome"),
                                         &dir_path_))
    return false;

  // DownloadManager could only be invoked from the UI thread.
  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(this,
                        &DragDownloadFile::OnInitiateDownload,
                        dir_path_));

  return true;
}

void DragDownloadFile::OnInitiateDownload(const FilePath& dir_path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  download_manager_ = tab_contents_->profile()->GetDownloadManager();
  download_manager_->AddObserver(this);

  // Start the download.
  download_manager_->DownloadUrlToFile(url_,
                                       referrer_,
                                       referrer_encoding_,
                                       dir_path,
                                       tab_contents_);
}

void DragDownloadFile::InitiateDownloadSucceeded(
    const std::vector<OSExchangeData::DownloadFileInfo*>& downloads) {
  DCHECK(drag_message_loop_ == MessageLoop::current());

  // Notify the drag-and-drop observer about the file info.
  DCHECK(observer_);
  observer_->OnDataReady(format_, downloads);

  InitiateDownloadCompleted(true);
}

void DragDownloadFile::InitiateDownloadFailed() {
  DCHECK(drag_message_loop_ == MessageLoop::current());

  InitiateDownloadCompleted(false);
}

void DragDownloadFile::InitiateDownloadCompleted(bool result) {
  DCHECK(drag_message_loop_ == MessageLoop::current());

  // Release the observer since we do not need it any more.
  observer_ = NULL;

  initiate_download_result_ = result;
  QuitNestedMessageLoopIfNeeded();
}

void DragDownloadFile::StartNestedMessageLoop() {
  DCHECK(drag_message_loop_ == MessageLoop::current());

  bool old_state = MessageLoop::current()->NestableTasksAllowed();
  MessageLoop::current()->SetNestableTasksAllowed(true);
  is_running_nested_message_loop_ = true;
  MessageLoop::current()->Run();
  MessageLoop::current()->SetNestableTasksAllowed(old_state);
}

void DragDownloadFile::QuitNestedMessageLoopIfNeeded() {
  DCHECK(drag_message_loop_ == MessageLoop::current());

  if (is_running_nested_message_loop_) {
    is_running_nested_message_loop_ = false;
    MessageLoop::current()->Quit();
  }
}

void DragDownloadFile::ModelChanged() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  download_manager_->GetTemporaryDownloads(this, dir_path_);
}

void DragDownloadFile::SetDownloads(std::vector<DownloadItem*>& downloads) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  std::vector<DownloadItem*>::const_iterator it = downloads.begin();
  for (; it != downloads.end(); ++it) {
    if (!download_item_observer_added_ && (*it)->url() == url_) {
      download_item_observer_added_ = true;
      (*it)->AddObserver(this);
    }
  }
}

void DragDownloadFile::OnDownloadUpdated(DownloadItem* download) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  if (download->state() == DownloadItem::CANCELLED) {
    download->RemoveObserver(this);
    download_manager_->RemoveObserver(this);

    drag_message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this,
                          &DragDownloadFile::DownloadCancelled));
  }
}

void DragDownloadFile::OnDownloadFileCompleted(DownloadItem* download) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(download->state() == DownloadItem::COMPLETE);

  download->RemoveObserver(this);
  download_manager_->RemoveObserver(this);

  drag_message_loop_->PostTask(
      FROM_HERE,
      NewRunnableMethod(this,
                        &DragDownloadFile::DownloadCompleted,
                        download->full_path()));
}

void DragDownloadFile::DownloadCancelled() {
  DCHECK(drag_message_loop_ == MessageLoop::current());

  InitiateDownloadFailed();
}

void DragDownloadFile::DownloadCompleted(const FilePath& file_path) {
  DCHECK(drag_message_loop_ == MessageLoop::current());

  file_path_ = file_path;

  // The download has been successfully initiated.
  std::vector<OSExchangeData::DownloadFileInfo*> downloads;
  downloads.push_back(
      new OSExchangeData::DownloadFileInfo(file_path, 0, NULL));
  InitiateDownloadSucceeded(downloads);
}
