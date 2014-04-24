// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_handle.h"

#include <algorithm>

#include "base/logging.h"
#include "chrome/browser/prerender/prerender_contents.h"
#include "content/public/browser/web_contents.h"

namespace prerender {

PrerenderHandle::Observer::Observer() {
}

PrerenderHandle::Observer::~Observer() {
}

PrerenderHandle::~PrerenderHandle() {
  if (prerender_data_.get()) {
    prerender_data_->contents()->RemoveObserver(this);
  }
}

void PrerenderHandle::SetObserver(Observer* observer) {
  DCHECK_NE(static_cast<Observer*>(NULL), observer);
  observer_ = observer;
}

void PrerenderHandle::OnNavigateAway() {
  DCHECK(CalledOnValidThread());
  if (prerender_data_.get())
    prerender_data_->OnHandleNavigatedAway(this);
}

void PrerenderHandle::OnCancel() {
  DCHECK(CalledOnValidThread());
  if (prerender_data_.get())
    prerender_data_->OnHandleCanceled(this);
}

bool PrerenderHandle::IsPrerendering() const {
  DCHECK(CalledOnValidThread());
  return prerender_data_.get() != NULL;
}

bool PrerenderHandle::IsFinishedLoading() const {
  DCHECK(CalledOnValidThread());
  if (!prerender_data_.get())
    return false;
  return prerender_data_->contents()->has_finished_loading();
}

bool PrerenderHandle::IsAbandoned() const {
  DCHECK(CalledOnValidThread());
  return prerender_data_ && !prerender_data_->abandon_time().is_null();
}

PrerenderContents* PrerenderHandle::contents() const {
  DCHECK(CalledOnValidThread());
  return prerender_data_ ? prerender_data_->contents() : NULL;
}

bool PrerenderHandle::Matches(
    const GURL& url,
    const content::SessionStorageNamespace* session_storage_namespace) const {
  DCHECK(CalledOnValidThread());
  if (!prerender_data_.get())
    return false;
  return prerender_data_->contents()->Matches(url, session_storage_namespace);
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

void PrerenderHandle::OnPrerenderStart(PrerenderContents* prerender_contents) {
  DCHECK(CalledOnValidThread());
  DCHECK(prerender_data_.get());
  DCHECK_EQ(prerender_data_->contents(), prerender_contents);
  if (observer_)
    observer_->OnPrerenderStart(this);
}

void PrerenderHandle::OnPrerenderStopLoading(
    PrerenderContents* prerender_contents) {
  DCHECK(CalledOnValidThread());
  DCHECK(prerender_data_.get());
  DCHECK_EQ(prerender_data_->contents(), prerender_contents);
  if (observer_)
    observer_->OnPrerenderStopLoading(this);
}

void PrerenderHandle::OnPrerenderDomContentLoaded(
    PrerenderContents* prerender_contents) {
  DCHECK(CalledOnValidThread());
  DCHECK(prerender_data_.get());
  DCHECK_EQ(prerender_data_->contents(), prerender_contents);
  if (observer_)
    observer_->OnPrerenderDomContentLoaded(this);
}

void PrerenderHandle::OnPrerenderStop(PrerenderContents* prerender_contents) {
  DCHECK(CalledOnValidThread());
  if (observer_)
    observer_->OnPrerenderStop(this);
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
  if (observer_)
    observer_->OnPrerenderCreatedMatchCompleteReplacement(this);
}

bool PrerenderHandle::RepresentingSamePrerenderAs(
    PrerenderHandle* other) const {
  return other && other->prerender_data_.get() && prerender_data_.get()
      && prerender_data_.get() == other->prerender_data_.get();
}

content::SessionStorageNamespace*
PrerenderHandle::GetSessionStorageNamespace() const {
  if (!prerender_data_.get())
    return NULL;
  return prerender_data_->contents()->GetSessionStorageNamespace();
}

int PrerenderHandle::GetChildId() const {
  if (!prerender_data_.get())
    return -1;
  return prerender_data_->contents()->child_id();
}

}  // namespace prerender
