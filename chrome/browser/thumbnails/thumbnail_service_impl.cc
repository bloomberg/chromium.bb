// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/thumbnail_service_impl.h"

#include "base/command_line.h"
#include "base/memory/ref_counted_memory.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/thumbnails/content_based_thumbnailing_algorithm.h"
#include "chrome/browser/thumbnails/simple_thumbnail_crop.h"
#include "chrome/browser/thumbnails/thumbnailing_context.h"
#include "chrome/common/chrome_switches.h"
#include "url/gurl.h"

using content::BrowserThread;

namespace {

// The thumbnail size in DIP.
const int kThumbnailWidth = 212;
const int kThumbnailHeight = 132;

// True if thumbnail retargeting feature is enabled (Finch/flags).
bool IsThumbnailRetargetingEnabled() {
  if (!chrome::IsInstantExtendedAPIEnabled())
    return false;

  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableThumbnailRetargeting);
}

void AddForcedURLOnUIThread(scoped_refptr<history::TopSites> top_sites,
                            const GURL& url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (top_sites.get() != NULL)
    top_sites->AddForcedURL(url, base::Time::Now());
}

}  // namespace

namespace thumbnails {

ThumbnailServiceImpl::ThumbnailServiceImpl(Profile* profile)
    : top_sites_(profile->GetTopSites()),
      use_thumbnail_retargeting_(IsThumbnailRetargetingEnabled()) {
}

ThumbnailServiceImpl::~ThumbnailServiceImpl() {
}

bool ThumbnailServiceImpl::SetPageThumbnail(const ThumbnailingContext& context,
                                            const gfx::Image& thumbnail) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);
  if (local_ptr.get() == NULL)
    return false;

  return local_ptr->SetPageThumbnail(context.url, thumbnail, context.score);
}

bool ThumbnailServiceImpl::GetPageThumbnail(
    const GURL& url,
    bool prefix_match,
    scoped_refptr<base::RefCountedMemory>* bytes) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);
  if (local_ptr.get() == NULL)
    return false;

  return local_ptr->GetPageThumbnail(url, prefix_match, bytes);
}

void ThumbnailServiceImpl::AddForcedURL(const GURL& url) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);
  if (local_ptr.get() == NULL)
    return;

  // Adding
  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                          base::Bind(AddForcedURLOnUIThread, local_ptr, url));
}

ThumbnailingAlgorithm* ThumbnailServiceImpl::GetThumbnailingAlgorithm()
    const {
  const gfx::Size thumbnail_size(kThumbnailWidth, kThumbnailHeight);
  if (use_thumbnail_retargeting_)
    return new ContentBasedThumbnailingAlgorithm(thumbnail_size);
  return new SimpleThumbnailCrop(thumbnail_size);
}

bool ThumbnailServiceImpl::ShouldAcquirePageThumbnail(const GURL& url) {
  scoped_refptr<history::TopSites> local_ptr(top_sites_);

  if (local_ptr.get() == NULL)
    return false;

  // Skip if the given URL is not appropriate for history.
  if (!HistoryService::CanAddURL(url))
    return false;
  // Skip if the top sites list is full, and the URL is not known.
  if (local_ptr->IsNonForcedFull() && !local_ptr->IsKnownURL(url))
    return false;
  // Skip if we don't have to udpate the existing thumbnail.
  ThumbnailScore current_score;
  if (local_ptr->GetPageThumbnailScore(url, &current_score) &&
      !current_score.ShouldConsiderUpdating())
    return false;
  // Skip if we don't have to udpate the temporary thumbnail (i.e. the one
  // not yet saved).
  ThumbnailScore temporary_score;
  if (local_ptr->GetTemporaryPageThumbnailScore(url, &temporary_score) &&
      !temporary_score.ShouldConsiderUpdating())
    return false;

  return true;
}

void ThumbnailServiceImpl::ShutdownOnUIThread() {
  // Since each call uses its own scoped_refptr, we can just clear the reference
  // here by assigning null. If another call is completed, it added its own
  // reference.
  top_sites_ = NULL;
}

}  // namespace thumbnails
