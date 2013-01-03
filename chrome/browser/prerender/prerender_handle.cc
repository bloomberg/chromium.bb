// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_handle.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/prerender/prerender_contents.h"

namespace prerender {

PrerenderHandle::Observer::Observer() {
}

PrerenderHandle::Observer::~Observer() {
}

PrerenderHandle::~PrerenderHandle() {
  if (prerender_data_) {
    prerender_data_->contents()->RemoveObserver(this);
  }
}

void PrerenderHandle::SetObserver(Observer* observer) {
  DCHECK_NE(static_cast<Observer*>(NULL), observer);
  observer_ = observer;
}

void PrerenderHandle::OnNavigateAway() {
  DCHECK(CalledOnValidThread());
  if (prerender_data_)
    prerender_data_->OnHandleNavigatedAway(this);
}

void PrerenderHandle::OnCancel() {
  DCHECK(CalledOnValidThread());
  if (prerender_data_)
    prerender_data_->OnHandleCanceled(this);
}

bool PrerenderHandle::IsPrerendering() const {
  DCHECK(CalledOnValidThread());
  return prerender_data_ != NULL;
}

bool PrerenderHandle::IsFinishedLoading() const {
  DCHECK(CalledOnValidThread());
  if (!prerender_data_)
    return false;
  return prerender_data_->contents()->has_finished_loading();
}

PrerenderHandle::PrerenderHandle(
    PrerenderManager::PrerenderData* prerender_data)
    : observer_(NULL),
      weak_ptr_factory_(this) {
  if (prerender_data) {
    prerender_data_ = prerender_data->AsWeakPtr();
    prerender_data->OnHandleCreated(this);
  }
}

void PrerenderHandle::AdoptPrerenderDataFrom(PrerenderHandle* other_handle) {
  DCHECK_EQ(static_cast<PrerenderManager::PrerenderData*>(NULL),
            prerender_data_);
  if (other_handle->prerender_data_ &&
      other_handle->prerender_data_->contents()) {
    other_handle->prerender_data_->contents()->RemoveObserver(other_handle);
  }

  prerender_data_ = other_handle->prerender_data_;
  other_handle->prerender_data_.reset();

  if (prerender_data_) {
    DCHECK_NE(static_cast<PrerenderContents*>(NULL),
              prerender_data_->contents());
    prerender_data_->contents()->AddObserver(this);
    // We are joining a prerender that has already started so we fire off an
    // extra start event at ourselves.
    OnPrerenderStart(prerender_data_->contents());
  }
}

void PrerenderHandle::OnPrerenderStart(PrerenderContents* prerender_contents) {
  DCHECK(CalledOnValidThread());
  DCHECK(prerender_data_);
  DCHECK_EQ(prerender_data_->contents(), prerender_contents);
  if (observer_)
    observer_->OnPrerenderStart(this);
}

void PrerenderHandle::OnPrerenderStopLoading(
    PrerenderContents* prerender_contents) {
  DCHECK(CalledOnValidThread());
  DCHECK(prerender_data_);
  DCHECK_EQ(prerender_data_->contents(), prerender_contents);
  if (observer_)
    observer_->OnPrerenderStopLoading(this);
}

void PrerenderHandle::OnPrerenderStop(PrerenderContents* prerender_contents) {
  DCHECK(CalledOnValidThread());
  if (observer_)
    observer_->OnPrerenderStop(this);
}

void PrerenderHandle::OnPrerenderAddAlias(PrerenderContents* prerender_contents,
                                          const GURL& alias_url) {
  DCHECK(CalledOnValidThread());
  DCHECK(prerender_data_);
  DCHECK_EQ(prerender_data_->contents(), prerender_contents);
  if (observer_)
    observer_->OnPrerenderAddAlias(this, alias_url);
}

void PrerenderHandle::OnPrerenderCreatedMatchCompleteReplacement(
    PrerenderContents* contents, PrerenderContents* replacement) {
  DCHECK(CalledOnValidThread());

  // This should occur in the middle of the surgery on the PrerenderData, and
  // so we expect to not have our new contents in our PrerenderData yet. The
  // switch occurs in
  // PrerenderManager::PrerenderData::MakeIntoMatchCompleteReplacement, so
  // this method only needs to switch observing.

  contents->RemoveObserver(this);
  replacement->AddObserver(this);
}

}  // namespace prerender
