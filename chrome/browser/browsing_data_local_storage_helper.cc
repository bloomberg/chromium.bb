// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_local_storage_helper.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/in_process_webkit/webkit_context.h"
#include "chrome/browser/profile.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebString.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webkit_glue.h"

BrowsingDataLocalStorageHelper::BrowsingDataLocalStorageHelper(
    Profile* profile)
    : profile_(profile),
      completion_callback_(NULL),
      is_fetching_(false) {
  DCHECK(profile_);
}

BrowsingDataLocalStorageHelper::~BrowsingDataLocalStorageHelper() {
}

void BrowsingDataLocalStorageHelper::StartFetching(
    Callback1<const std::vector<LocalStorageInfo>& >::Type* callback) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(!is_fetching_);
  DCHECK(callback);
  is_fetching_ = true;
  completion_callback_.reset(callback);
  ChromeThread::PostTask(
      ChromeThread::WEBKIT, FROM_HERE,
      NewRunnableMethod(
          this,
          &BrowsingDataLocalStorageHelper::
              FetchLocalStorageInfoInWebKitThread));
}

void BrowsingDataLocalStorageHelper::CancelNotification() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  completion_callback_.reset(NULL);
}

void BrowsingDataLocalStorageHelper::DeleteLocalStorageFile(
    const FilePath& file_path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  ChromeThread::PostTask(
      ChromeThread::WEBKIT, FROM_HERE,
       NewRunnableMethod(
           this,
           &BrowsingDataLocalStorageHelper::
              DeleteLocalStorageFileInWebKitThread,
           file_path));
}

void BrowsingDataLocalStorageHelper::DeleteAllLocalStorageFiles() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  ChromeThread::PostTask(
      ChromeThread::WEBKIT, FROM_HERE,
      NewRunnableMethod(
          this,
          &BrowsingDataLocalStorageHelper::
              DeleteAllLocalStorageFilesInWebKitThread));
}

void BrowsingDataLocalStorageHelper::FetchLocalStorageInfoInWebKitThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  file_util::FileEnumerator file_enumerator(
      profile_->GetWebKitContext()->data_path().Append(
          DOMStorageContext::kLocalStorageDirectory),
      false, file_util::FileEnumerator::FILES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == DOMStorageContext::kLocalStorageExtension) {
      scoped_ptr<WebKit::WebSecurityOrigin> web_security_origin(
          WebKit::WebSecurityOrigin::createFromDatabaseIdentifier(
              webkit_glue::FilePathToWebString(file_path.BaseName())));
      file_util::FileInfo file_info;
      bool ret = file_util::GetFileInfo(file_path, &file_info);
      if (ret) {
        local_storage_info_.push_back(LocalStorageInfo(
            webkit_glue::WebStringToStdString(web_security_origin->protocol()),
            webkit_glue::WebStringToStdString(web_security_origin->host()),
            web_security_origin->port(),
            webkit_glue::WebStringToStdString(
                web_security_origin->databaseIdentifier()),
            webkit_glue::WebStringToStdString(
                web_security_origin->toString()),
            file_path,
            file_info.size,
            file_info.last_modified));
      }
    }
  }

  ChromeThread::PostTask(
      ChromeThread::UI, FROM_HERE,
      NewRunnableMethod(
          this, &BrowsingDataLocalStorageHelper::NotifyInUIThread));
}

void BrowsingDataLocalStorageHelper::NotifyInUIThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));
  DCHECK(is_fetching_);
  // Note: completion_callback_ mutates only in the UI thread, so it's safe to
  // test it here.
  if (completion_callback_ != NULL)
    completion_callback_->Run(local_storage_info_);
  is_fetching_ = false;
}

void BrowsingDataLocalStorageHelper::DeleteLocalStorageFileInWebKitThread(
    const FilePath& file_path) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  profile_->GetWebKitContext()->dom_storage_context()->DeleteLocalStorageFile(
      file_path);
}

void
    BrowsingDataLocalStorageHelper::DeleteAllLocalStorageFilesInWebKitThread() {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::WEBKIT));
  profile_->GetWebKitContext()->dom_storage_context()
      ->DeleteAllLocalStorageFiles();
}
