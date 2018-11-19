// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnail_service_impl.h"

#include "base/feature_list.h"
#include "base/memory/ref_counted_memory.h"
#include "base/task/post_task.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_utils.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

void AddForcedURLOnUIThread(scoped_refptr<history::TopSites> top_sites,
                            const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (top_sites)
    top_sites->AddForcedURL(url, base::Time::Now());
}

}  // namespace

namespace thumbnails {

ThumbnailServiceImpl::ThumbnailServiceImpl(Profile* profile)
    : top_sites_(TopSitesFactory::GetForProfile(profile)) {}

ThumbnailServiceImpl::~ThumbnailServiceImpl() {
}

bool ThumbnailServiceImpl::SetPageThumbnail(const ThumbnailingContext& context,
                                            const gfx::Image& thumbnail) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);
  if (!local_ptr)
    return false;

  return local_ptr->SetPageThumbnail(context.url, thumbnail, context.score);
}

bool ThumbnailServiceImpl::GetPageThumbnail(
    const GURL& url,
    bool prefix_match,
    scoped_refptr<base::RefCountedMemory>* bytes) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);
  if (!local_ptr)
    return false;

  return local_ptr->GetPageThumbnail(url, prefix_match, bytes);
}

void ThumbnailServiceImpl::AddForcedURL(const GURL& url) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);
  if (!local_ptr)
    return;

  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::Bind(AddForcedURLOnUIThread, local_ptr, url));
}

bool ThumbnailServiceImpl::ShouldAcquirePageThumbnail(
    const GURL& url,
    ui::PageTransition transition) {
  // Disable thumbnail capture (see https://crbug.com/893362).
  return false;
}

void ThumbnailServiceImpl::ShutdownOnUIThread() {
  // Since each call uses its own scoped_refptr, we can just clear the reference
  // here by assigning null. If another call is completed, it added its own
  // reference.
  top_sites_ = NULL;
}

}  // namespace thumbnails
