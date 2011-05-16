// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/download/download_tab_helper.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/download/download_request_limiter.h"
#include "chrome/browser/download/download_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper_delegate.h"
#include "chrome/browser/ui/download/download_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/render_messages.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/view_messages.h"

DownloadTabHelper::DownloadTabHelper(TabContentsWrapper* tab_contents)
    : TabContentsObserver(tab_contents->tab_contents()),
      tab_contents_wrapper_(tab_contents),
      delegate_(NULL) {
  DCHECK(tab_contents);
}

DownloadTabHelper::~DownloadTabHelper() {
}

void DownloadTabHelper::OnSavePage() {
  // If we can not save the page, try to download it.
  if (!SavePackage::IsSavableContents(tab_contents()->contents_mime_type())) {
    DownloadManager* dlm = tab_contents()->profile()->GetDownloadManager();
    const GURL& current_page_url = tab_contents()->GetURL();
    if (dlm && current_page_url.is_valid()) {
      dlm->DownloadUrl(current_page_url, GURL(), "", tab_contents());
      download_util::RecordDownloadCount(
          download_util::INITIATED_BY_SAVE_PACKAGE_FAILURE_COUNT);
    }
    return;
  }

  tab_contents()->Stop();

  // Create the save package and possibly prompt the user for the name to save
  // the page as. The user prompt is an asynchronous operation that runs on
  // another thread.
  save_package_ = new SavePackage(tab_contents_wrapper_);
  save_package_->GetSaveInfo();
}

void DownloadTabHelper::OnSaveURL(const GURL& url) {
  DownloadManager* dlm = tab_contents()->profile()->GetDownloadManager();
  dlm->DownloadUrl(url, tab_contents()->GetURL(), "", tab_contents());
}

// Used in automated testing to bypass prompting the user for file names.
// Instead, the names and paths are hard coded rather than running them through
// file name sanitation and extension / mime checking.
bool DownloadTabHelper::SavePage(const FilePath& main_file,
                                 const FilePath& dir_path,
                                 SavePackage::SavePackageType save_type) {
  // Stop the page from navigating.
  tab_contents()->Stop();

  save_package_ =
      new SavePackage(tab_contents_wrapper_, save_type, main_file, dir_path);
  return save_package_->Init();
}

bool DownloadTabHelper::CanDownload(int request_id) {
  if (delegate_)
    return delegate_->CanDownload(request_id);
  return true;
}

void DownloadTabHelper::OnStartDownload(DownloadItem* download) {
  DCHECK(download);

  BlockedContentTabHelperDelegate* blocked_content_delegate =
      tab_contents_wrapper_->blocked_content_tab_helper()->delegate();
  if (!blocked_content_delegate)
    return;

  // Download in a constrained popup is shown in the tab that opened it.
  TabContentsWrapper* tab_contents =
      blocked_content_delegate->GetConstrainingContentsWrapper(
          tab_contents_wrapper_);

  if (tab_contents && tab_contents->download_tab_helper()->delegate()) {
    tab_contents->download_tab_helper()->delegate()->OnStartDownload(
        download, tab_contents_wrapper_);
  }
}

bool DownloadTabHelper::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DownloadTabHelper, message)
    IPC_MESSAGE_HANDLER(ViewHostMsg_SaveURLAs, OnSaveURL)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void DownloadTabHelper::DidGetUserGesture() {
  DownloadRequestLimiter* limiter =
      g_browser_process->download_request_limiter();
  if (limiter)
    limiter->OnUserGesture(tab_contents());
}
