// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_loader_manager.h"

#include "base/logging.h"
#include "chrome/browser/instant/instant_loader.h"
#include "chrome/browser/instant/instant_loader_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/tab_contents/tab_contents.h"

InstantLoaderManager::InstantLoaderManager(
    InstantLoaderDelegate* loader_delegate)
    : loader_delegate_(loader_delegate),
      current_loader_(NULL),
      pending_loader_(NULL) {
}

InstantLoaderManager::~InstantLoaderManager() {
  for (Loaders::iterator i = instant_loaders_.begin();
       i != instant_loaders_.end(); ++i) {
    if (i->second == current_loader_)
      current_loader_ = NULL;
    if (i->second == pending_loader_)
      pending_loader_ = NULL;
    delete i->second;
  }
  instant_loaders_.clear();

  delete current_loader_;
  delete pending_loader_;
}

InstantLoader* InstantLoaderManager::UpdateLoader(
    TemplateURLID instant_id,
    scoped_ptr<InstantLoader>* old_loader) {
  InstantLoader* old_current_loader = current_loader_;
  InstantLoader* old_pending_loader = pending_loader_;

  // Determine the new loader.
  InstantLoader* loader = NULL;
  if (instant_id) {
    loader = GetInstantLoader(instant_id);
  } else {
    if (current_loader_ && !current_loader_->template_url_id())
      loader = current_loader_;
    else if (pending_loader_ && !pending_loader_->template_url_id())
      loader = pending_loader_;
    else
      loader = CreateLoader(0);
  }

  if (loader->ready()) {
    // The loader is ready, make it the current loader no matter what.
    current_loader_ = loader;
    pending_loader_ = NULL;
  } else {
    // The loader isn't ready make it the current only if the current isn't
    // ready. If the current is ready, then stop the current and make the new
    // loader pending.
    if (!current_loader_ || !current_loader_->ready()) {
      current_loader_ = loader;
      DCHECK(!pending_loader_);
    } else {
      // preview_contents() may be null for tests.
      if (!current_loader_->template_url_id() &&
          current_loader_->preview_contents()) {
        current_loader_->preview_contents()->tab_contents()->Stop();
      }
      pending_loader_ = loader;
    }
  }

  if (current_loader_ != old_current_loader && old_current_loader &&
      !old_current_loader->template_url_id()) {
    old_loader->reset(old_current_loader);
  }
  if (pending_loader_ != old_pending_loader && old_pending_loader &&
      !old_pending_loader->template_url_id() &&
      old_pending_loader != current_loader_) {
    DCHECK(!old_loader->get());
    old_loader->reset(old_pending_loader);
  }

  return active_loader();
}

bool InstantLoaderManager::WillUpateChangeActiveLoader(
    TemplateURLID instant_id) {
  return !active_loader() || active_loader()->template_url_id() != instant_id;
}

void InstantLoaderManager::MakePendingCurrent(
    scoped_ptr<InstantLoader>* old_loader) {
  DCHECK(current_loader_);
  DCHECK(pending_loader_);

  if (!current_loader_->template_url_id())
    old_loader->reset(current_loader_);

  current_loader_ = pending_loader_;
  pending_loader_ = NULL;
}

InstantLoader* InstantLoaderManager::ReleaseCurrentLoader() {
  DCHECK(current_loader_);
  InstantLoader* loader = current_loader_;
  RemoveLoaderFromInstant(current_loader_);
  current_loader_ = NULL;
  return loader;
}

void InstantLoaderManager::DestroyLoader(InstantLoader* loader) {
  delete ReleaseLoader(loader);
}

InstantLoader* InstantLoaderManager::ReleaseLoader(InstantLoader* loader) {
  DCHECK(loader == current_loader_ || loader == pending_loader_ ||
         (loader->template_url_id() &&
          instant_loaders_.find(loader->template_url_id()) !=
          instant_loaders_.end()));

  if (current_loader_ == loader)
    current_loader_ = pending_loader_;

  if (pending_loader_ == loader)
    pending_loader_ = NULL;

  RemoveLoaderFromInstant(loader);

  return loader;
}

void InstantLoaderManager::RemoveLoaderFromInstant(InstantLoader* loader) {
  if (!loader->template_url_id())
    return;

  Loaders::iterator i = instant_loaders_.find(loader->template_url_id());
  DCHECK(i != instant_loaders_.end());
  instant_loaders_.erase(i);
}

InstantLoader* InstantLoaderManager::CreateLoader(TemplateURLID id) {
  InstantLoader* loader = new InstantLoader(loader_delegate_, id);
  if (id)
    instant_loaders_[id] = loader;
  return loader;
}

InstantLoader* InstantLoaderManager::GetInstantLoader(TemplateURLID id) {
  Loaders::iterator i = instant_loaders_.find(id);
  return i == instant_loaders_.end() ? CreateLoader(id) : i->second;
}
