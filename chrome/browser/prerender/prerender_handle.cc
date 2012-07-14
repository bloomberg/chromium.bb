// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_handle.h"

#include <algorithm>

#include "chrome/browser/prerender/prerender_contents.h"

namespace prerender {

PrerenderHandle::~PrerenderHandle() {
  DCHECK(!IsValid());
  // This shouldn't occur, but we also shouldn't leak if it does.
  if (IsValid())
    OnCancel();
}

void PrerenderHandle::OnNavigateAway() {
  DCHECK(CalledOnValidThread());
  if (!IsValid())
    return;
  prerender_data_->OnNavigateAwayByHandle();
  prerender_data_.reset();
}

void PrerenderHandle::OnCancel() {
  DCHECK(CalledOnValidThread());
  if (!IsValid())
    return;
  prerender_data_->OnCancelByHandle();
  prerender_data_.reset();
}

bool PrerenderHandle::IsValid() const {
  return prerender_data_ != NULL;
}

bool PrerenderHandle::IsPending() const {
  DCHECK(CalledOnValidThread());
  return prerender_data_ && !prerender_data_->contents();
}

bool PrerenderHandle::IsPrerendering() const {
  DCHECK(CalledOnValidThread());
  return prerender_data_ && prerender_data_->contents();
}

bool PrerenderHandle::IsFinishedLoading() const {
  DCHECK(CalledOnValidThread());
  if (!prerender_data_ || IsPending())
    return false;
  return prerender_data_->contents()->has_finished_loading();
}

PrerenderHandle::PrerenderHandle(
    PrerenderManager::PrerenderData* prerender_data)
    : prerender_data_(prerender_data->AsWeakPtr()),
      weak_ptr_factory_(this) {
  prerender_data->OnNewHandle();
}

void PrerenderHandle::SwapPrerenderDataWith(
    PrerenderHandle* other_prerender_handle) {
  DCHECK(CalledOnValidThread());
  DCHECK(other_prerender_handle);
  std::swap(prerender_data_, other_prerender_handle->prerender_data_);
}

}  // namespace prerender
