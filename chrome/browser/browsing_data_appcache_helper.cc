// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data_appcache_helper.h"

#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "webkit/appcache/appcache_database.h"
#include "webkit/appcache/appcache_storage.h"

using appcache::AppCacheDatabase;

BrowsingDataAppCacheHelper::BrowsingDataAppCacheHelper(Profile* profile)
    : request_context_getter_(profile->GetRequestContext()),
      is_fetching_(false) {
}

void BrowsingDataAppCacheHelper::StartFetching(Callback0::Type* callback) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    DCHECK(!is_fetching_);
    DCHECK(callback);
    is_fetching_ = true;
    info_collection_ = new appcache::AppCacheInfoCollection;
    completion_callback_.reset(callback);
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, NewRunnableMethod(
        this, &BrowsingDataAppCacheHelper::StartFetching, callback));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  appcache_info_callback_ =
      new net::CancelableCompletionCallback<BrowsingDataAppCacheHelper>(
          this, &BrowsingDataAppCacheHelper::OnFetchComplete);
  GetAppCacheService()->GetAllAppCacheInfo(info_collection_,
                                           appcache_info_callback_);
}

void BrowsingDataAppCacheHelper::CancelNotification() {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    completion_callback_.reset();
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, NewRunnableMethod(
        this, &BrowsingDataAppCacheHelper::CancelNotification));
    return;
  }

  if (appcache_info_callback_)
    appcache_info_callback_.release()->Cancel();
}

void BrowsingDataAppCacheHelper::DeleteAppCacheGroup(
    const GURL& manifest_url) {
  if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE, NewRunnableMethod(
        this, &BrowsingDataAppCacheHelper::DeleteAppCacheGroup,
        manifest_url));
    return;
  }
  GetAppCacheService()->DeleteAppCacheGroup(manifest_url, NULL);
}

BrowsingDataAppCacheHelper::~BrowsingDataAppCacheHelper() {}

void BrowsingDataAppCacheHelper::OnFetchComplete(int rv) {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    // Filter out appache info entries for extensions. Extension state is not
    // considered browsing data.
    typedef std::map<GURL, appcache::AppCacheInfoVector> InfoByOrigin;
    InfoByOrigin& origin_map = info_collection_->infos_by_origin;
    for (InfoByOrigin::iterator origin = origin_map.begin();
         origin != origin_map.end();) {
      InfoByOrigin::iterator current = origin;
      ++origin;
      if (current->first.SchemeIs(chrome::kExtensionScheme))
        origin_map.erase(current);
    }

    appcache_info_callback_ = NULL;
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, NewRunnableMethod(
        this, &BrowsingDataAppCacheHelper::OnFetchComplete, rv));
    return;
  }

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(is_fetching_);
  is_fetching_ = false;
  if (completion_callback_ != NULL) {
    completion_callback_->Run();
    completion_callback_.reset();
  }
}

ChromeAppCacheService* BrowsingDataAppCacheHelper::GetAppCacheService() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  ChromeURLRequestContext* request_context =
      reinterpret_cast<ChromeURLRequestContext*>(
          request_context_getter_->GetURLRequestContext());
  return request_context ? request_context->appcache_service()
                         : NULL;
}

CannedBrowsingDataAppCacheHelper::CannedBrowsingDataAppCacheHelper(
    Profile* profile)
    : BrowsingDataAppCacheHelper(profile),
      profile_(profile) {
  info_collection_ = new appcache::AppCacheInfoCollection;
}

CannedBrowsingDataAppCacheHelper* CannedBrowsingDataAppCacheHelper::Clone() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CannedBrowsingDataAppCacheHelper* clone =
      new CannedBrowsingDataAppCacheHelper(profile_);

  clone->info_collection_->infos_by_origin = info_collection_->infos_by_origin;
  return clone;
}

void CannedBrowsingDataAppCacheHelper::AddAppCache(const GURL& manifest_url) {
  typedef std::map<GURL, appcache::AppCacheInfoVector> InfoByOrigin;
  InfoByOrigin& origin_map = info_collection_->infos_by_origin;
  appcache::AppCacheInfoVector& appcache_infos_ =
      origin_map[manifest_url.GetOrigin()];

  for (appcache::AppCacheInfoVector::iterator
       appcache = appcache_infos_.begin(); appcache != appcache_infos_.end();
       ++appcache) {
    if (appcache->manifest_url == manifest_url)
      return;
  }

  appcache::AppCacheInfo info;
  info.manifest_url = manifest_url;
  appcache_infos_.push_back(info);
}

void CannedBrowsingDataAppCacheHelper::Reset() {
  info_collection_->infos_by_origin.clear();
}

bool CannedBrowsingDataAppCacheHelper::empty() const {
  return info_collection_->infos_by_origin.empty();
}

void CannedBrowsingDataAppCacheHelper::StartFetching(
    Callback0::Type* completion_callback) {
  completion_callback->Run();
  delete completion_callback;
}

CannedBrowsingDataAppCacheHelper::~CannedBrowsingDataAppCacheHelper() {}
