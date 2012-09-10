// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/drag_download_file.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/browser/download_url_parameters.h"
#include "net/base/file_stream.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DownloadItem;
using content::DownloadManager;
using content::DownloadUrlParameters;
using content::WebContents;

DragDownloadFile::DragDownloadFile(
    const FilePath& file_name_or_path,
    linked_ptr<net::FileStream> file_stream,
    const GURL& url,
    const content::Referrer& referrer,
    const std::string& referrer_encoding,
    WebContents* web_contents)
    : file_stream_(file_stream),
      url_(url),
      referrer_(referrer),
      referrer_encoding_(referrer_encoding),
      web_contents_(web_contents),
      drag_message_loop_(MessageLoop::current()),
      is_started_(false),
      is_successful_(false),
#if defined(OS_WIN)
      is_running_nested_message_loop_(false),
#endif
      download_manager_(NULL),
      download_manager_observer_added_(false),
      download_item_(NULL) {
#if defined(OS_WIN)
  DCHECK(!file_name_or_path.empty() && !file_stream.get());
  file_name_ = file_name_or_path;
#elif defined(OS_POSIX)
  DCHECK(!file_name_or_path.empty() && file_stream.get());
  file_path_ = file_name_or_path;
#endif
}

DragDownloadFile::~DragDownloadFile() {
  AssertCurrentlyOnDragThread();

  // Since the target application can still hold and use the dragged file,
  // we do not know the time that it can be safely deleted. To solve this
  // problem, we schedule it to be removed after the system is restarted.
#if defined(OS_WIN)
  if (!temp_dir_path_.empty()) {
    if (!file_path_.empty())
      file_util::DeleteAfterReboot(file_path_);
    file_util::DeleteAfterReboot(temp_dir_path_);
  }
#endif

  RemoveObservers();
}

void DragDownloadFile::RemoveObservers() {
  if (download_item_) {
    download_item_->RemoveObserver(this);
    download_item_ = NULL;
  }

  if (download_manager_observer_added_) {
    download_manager_observer_added_ = false;
    download_manager_->RemoveObserver(this);
  }
}

bool DragDownloadFile::Start(ui::DownloadFileObserver* observer) {
  AssertCurrentlyOnDragThread();

  if (is_started_)
    return true;
  is_started_ = true;

  DCHECK(!observer_.get());
  observer_ = observer;

  if (!file_stream_.get()) {
    // Create a temporary directory to save the temporary download file. We do
    // not want to use the default download directory since we do not want the
    // twisted file name shown in the download shelf if the file with the same
    // name already exists.
    if (!file_util::CreateNewTempDirectory(FILE_PATH_LITERAL("chrome"),
                                           &temp_dir_path_))
      return false;

    file_path_ = temp_dir_path_.Append(file_name_);
  }

  InitiateDownload();

  // On Windows, we need to wait till the download file is completed.
#if defined(OS_WIN)
  StartNestedMessageLoop();
#endif

  return is_successful_;
}

void DragDownloadFile::Stop() {
}

void DragDownloadFile::InitiateDownload() {
#if defined(OS_WIN)
  // DownloadManager could only be invoked from the UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&DragDownloadFile::InitiateDownload, this));
    return;
  }
#endif

  download_manager_ = BrowserContext::GetDownloadManager(
      web_contents_->GetBrowserContext());
  download_manager_observer_added_ = true;
  download_manager_->AddObserver(this);

  content::DownloadSaveInfo save_info;
  save_info.file_path = file_path_;
  save_info.file_stream = file_stream_;

  download_stats::RecordDownloadSource(
      download_stats::INITIATED_BY_DRAG_N_DROP);
  scoped_ptr<DownloadUrlParameters> params(
      DownloadUrlParameters::FromWebContents(web_contents_, url_, save_info));
  params->set_referrer(referrer_);
  params->set_referrer_encoding(referrer_encoding_);
  download_manager_->DownloadUrl(params.Pass());
}

void DragDownloadFile::DownloadCompleted(bool is_successful) {
#if defined(OS_WIN)
  // If not in drag-and-drop thread, defer the running to it.
  if (drag_message_loop_ != MessageLoop::current()) {
    drag_message_loop_->PostTask(
        FROM_HERE,
        base::Bind(&DragDownloadFile::DownloadCompleted, this, is_successful));
    return;
  }
#endif

  is_successful_ = is_successful;

  // Call the observer.
  DCHECK(observer_);
  if (is_successful)
    observer_->OnDownloadCompleted(file_path_);
  else
    observer_->OnDownloadAborted();

  // Release the observer since we do not need it any more.
  observer_ = NULL;

  // On Windows, we need to stop the waiting.
#if defined(OS_WIN)
  QuitNestedMessageLoop();
#endif
}

void DragDownloadFile::ModelChanged(DownloadManager* manager) {
  AssertCurrentlyOnUIThread();
  DCHECK_EQ(manager, download_manager_);

  if (download_item_)
    return;

  std::vector<DownloadItem*> downloads;
  download_manager_->GetAllDownloads(&downloads);
  for (std::vector<DownloadItem*>::const_iterator i = downloads.begin();
       i != downloads.end(); ++i) {
    DownloadItem* item = *i;
    if (item->IsTemporary() &&
        item->GetOriginalUrl() == url_ &&
        file_path_.DirName() == item->GetTargetFilePath().DirName()) {
      download_item_ = item;
      download_item_->AddObserver(this);
      break;
    }
  }
}

void DragDownloadFile::OnDownloadUpdated(content::DownloadItem* download) {
  AssertCurrentlyOnUIThread();
  if (download->IsCancelled()) {
    RemoveObservers();
    DownloadCompleted(false);
  } else if (download->IsComplete()) {
    RemoveObservers();
    DownloadCompleted(true);
  }
  // Ignore other states.
}

// If the download completes or is cancelled, then OnDownloadUpdated() will
// handle it and RemoveObserver() so that OnDownloadDestroyed is never called.
// OnDownloadDestroyed is only called if OnDownloadUpdated() does not detect
// completion or cancellation (in which cases it removes this observer).
// TODO(benjhayden): Try to change this to NOTREACHED()?
void DragDownloadFile::OnDownloadDestroyed(content::DownloadItem* download) {
  AssertCurrentlyOnUIThread();
  RemoveObservers();
  DownloadCompleted(false);
}

void DragDownloadFile::AssertCurrentlyOnDragThread() {
  // Only do the check on Windows where two threads are involved.
#if defined(OS_WIN)
  DCHECK(drag_message_loop_ == MessageLoop::current());
#endif
}

void DragDownloadFile::AssertCurrentlyOnUIThread() {
  // Only do the check on Windows where two threads are involved.
#if defined(OS_WIN)
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
#endif
}

#if defined(OS_WIN)
void DragDownloadFile::StartNestedMessageLoop() {
  AssertCurrentlyOnDragThread();

  MessageLoop::ScopedNestableTaskAllower allow(MessageLoop::current());
  is_running_nested_message_loop_ = true;
  MessageLoop::current()->Run();
  DCHECK(!is_running_nested_message_loop_);
}

void DragDownloadFile::QuitNestedMessageLoop() {
  AssertCurrentlyOnDragThread();

  if (is_running_nested_message_loop_) {
    is_running_nested_message_loop_ = false;
    MessageLoop::current()->Quit();
  }
}
#endif
