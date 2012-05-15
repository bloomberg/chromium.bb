// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_local_storage_helper.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/glue/webkit_glue.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;
using WebKit::WebSecurityOrigin;

BrowsingDataLocalStorageHelper::LocalStorageInfo::LocalStorageInfo(
    const std::string& protocol,
    const std::string& host,
    unsigned short port,
    const std::string& database_identifier,
    const std::string& origin,
    const FilePath& file_path,
    int64 size,
    base::Time last_modified)
    : protocol(protocol),
      host(host),
      port(port),
      database_identifier(database_identifier),
      origin(origin),
      file_path(file_path),
      size(size),
      last_modified(last_modified) {
      }

BrowsingDataLocalStorageHelper::LocalStorageInfo::~LocalStorageInfo() {}

BrowsingDataLocalStorageHelper::BrowsingDataLocalStorageHelper(
    Profile* profile)
    : dom_storage_context_(BrowserContext::GetDOMStorageContext(profile)),
      is_fetching_(false) {
  DCHECK(dom_storage_context_);
}

BrowsingDataLocalStorageHelper::~BrowsingDataLocalStorageHelper() {
}

void BrowsingDataLocalStorageHelper::StartFetching(
    const base::Callback<void(const std::list<LocalStorageInfo>&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK_EQ(false, callback.is_null());

  is_fetching_ = true;
  completion_callback_ = callback;
  dom_storage_context_->GetAllStorageFiles(
      base::Bind(
          &BrowsingDataLocalStorageHelper::GetAllStorageFilesCallback, this));
}

void BrowsingDataLocalStorageHelper::DeleteLocalStorageFile(
    const FilePath& file_path) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  dom_storage_context_->DeleteLocalStorageFile(file_path);
}

void BrowsingDataLocalStorageHelper::GetAllStorageFilesCallback(
    const std::vector<FilePath>& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &BrowsingDataLocalStorageHelper::FetchLocalStorageInfo,
          this, files));
}

void BrowsingDataLocalStorageHelper::FetchLocalStorageInfo(
    const std::vector<FilePath>& files) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  for (size_t i = 0; i < files.size(); ++i) {
    FilePath file_path = files[i];
    WebSecurityOrigin web_security_origin =
        WebSecurityOrigin::createFromDatabaseIdentifier(
            webkit_glue::FilePathToWebString(file_path.BaseName()));
    if (!BrowsingDataHelper::IsValidScheme(web_security_origin.protocol()))
      continue;  // Non-websafe state is not considered browsing data.

    base::PlatformFileInfo file_info;
    bool ret = file_util::GetFileInfo(file_path, &file_info);
    if (ret) {
      local_storage_info_.push_back(LocalStorageInfo(
          web_security_origin.protocol().utf8(),
          web_security_origin.host().utf8(),
          web_security_origin.port(),
          web_security_origin.databaseIdentifier().utf8(),
          web_security_origin.toString().utf8(),
          file_path,
          file_info.size,
          file_info.last_modified));
    }
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&BrowsingDataLocalStorageHelper::NotifyInUIThread, this));
}

void BrowsingDataLocalStorageHelper::NotifyInUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);
  // Note: completion_callback_ mutates only in the UI thread, so it's safe to
  // test it here.
  completion_callback_.Run(local_storage_info_);
  completion_callback_.Reset();
  is_fetching_ = false;
}

//---------------------------------------------------------

CannedBrowsingDataLocalStorageHelper::CannedBrowsingDataLocalStorageHelper(
    Profile* profile)
    : BrowsingDataLocalStorageHelper(profile),
      profile_(profile) {
}

CannedBrowsingDataLocalStorageHelper*
CannedBrowsingDataLocalStorageHelper::Clone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CannedBrowsingDataLocalStorageHelper* clone =
      new CannedBrowsingDataLocalStorageHelper(profile_);

  clone->pending_local_storage_info_ = pending_local_storage_info_;
  return clone;
}

void CannedBrowsingDataLocalStorageHelper::AddLocalStorage(
    const GURL& origin) {
  if (BrowsingDataHelper::HasValidScheme(origin))
    pending_local_storage_info_.insert(origin);
}

void CannedBrowsingDataLocalStorageHelper::Reset() {
  pending_local_storage_info_.clear();
}

bool CannedBrowsingDataLocalStorageHelper::empty() const {
  return pending_local_storage_info_.empty();
}

size_t CannedBrowsingDataLocalStorageHelper::GetLocalStorageCount() const {
  return pending_local_storage_info_.size();
}

const std::set<GURL>&
CannedBrowsingDataLocalStorageHelper::GetLocalStorageInfo() const {
  return pending_local_storage_info_;
}

void CannedBrowsingDataLocalStorageHelper::StartFetching(
    const base::Callback<void(const std::list<LocalStorageInfo>&)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!is_fetching_);
  DCHECK_EQ(false, callback.is_null());

  is_fetching_ = true;
  completion_callback_ = callback;

  // We post a task to emulate async fetching behavior.
  MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&CannedBrowsingDataLocalStorageHelper::
          ConvertPendingInfo, this));
}

CannedBrowsingDataLocalStorageHelper::~CannedBrowsingDataLocalStorageHelper() {}

void CannedBrowsingDataLocalStorageHelper::ConvertPendingInfo() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  local_storage_info_.clear();
  for (std::set<GURL>::iterator info = pending_local_storage_info_.begin();
       info != pending_local_storage_info_.end(); ++info) {
    WebSecurityOrigin web_security_origin =
        WebSecurityOrigin::createFromString(
            UTF8ToUTF16(info->spec()));
    std::string security_origin(web_security_origin.toString().utf8());

    local_storage_info_.push_back(LocalStorageInfo(
        web_security_origin.protocol().utf8(),
        web_security_origin.host().utf8(),
        web_security_origin.port(),
        web_security_origin.databaseIdentifier().utf8(),
        security_origin,
        dom_storage_context_->
            GetFilePath(web_security_origin.databaseIdentifier()),
        0,
        base::Time()));
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CannedBrowsingDataLocalStorageHelper::NotifyInUIThread,
                 this));
}
