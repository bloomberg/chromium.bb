// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_LOADER_MANAGER_H_
#define CHROME_BROWSER_TAB_CONTENTS_LOADER_MANAGER_H_
#pragma once

#include <map>

#include "base/scoped_ptr.h"
#include "chrome/browser/search_engines/template_url_id.h"

class MatchPreviewLoader;
class MatchPreviewLoaderDelegate;

// LoaderManager is responsible for maintaining the MatchPreviewLoaders for
// MatchPreview. LoaderManager keeps track of one loader for loading non-instant
// urls, and a loader per TemplateURLID for loading instant urls. A loader per
// TemplateURLID is necessitated due to not knowing in advance if a site
// really supports instant (for example, the user might have opted out even
// though it's supported).
//
// Users of LoaderManager need only concern themselves with the current and
// pending loaders. The current loader is the loader that if ready is shown by
// MatchPreview. The pending loader is used if the current loader is ready and
// update is invoked with a different id. In this case the current loader is
// left as current (and it's preview contents stopped) and the newly created
// loader is set to pending.  Once the pending loader is ready
// MakePendingCurrent should be invoked to make the pending the current loader.
//
// MatchPreviewLoader owns all the MatchPreviewLoaders returned. You can take
// ownership of the current loader by invoking ReleaseCurrentLoader.
class LoaderManager {
 public:
  explicit LoaderManager(MatchPreviewLoaderDelegate* loader_delegate);
  ~LoaderManager();

  // Updates the current loader. If the current loader is replaced and should be
  // deleted it is set in |old_loader|. This is done to allow the caller to
  // notify delegates before the old loader is destroyed. This returns the
  // active MatchPreviewLoader that should be used.
  MatchPreviewLoader* UpdateLoader(TemplateURLID instant_id,
                                   scoped_ptr<MatchPreviewLoader>* old_loader);

  // Makes the pending loader the current loader. If ownership of the old
  // loader is to pass to the caller |old_loader| is set appropriately.
  void MakePendingCurrent(scoped_ptr<MatchPreviewLoader>* old_loader);

  // Returns the current loader and clears internal references to it.  This
  // should be used prior to destroying the LoaderManager when the owner of
  // LoaderManager wants to take ownership of the loader.
  MatchPreviewLoader* ReleaseCurrentLoader();

  // Returns the current loader, may be null.
  MatchPreviewLoader* current_loader() const { return current_loader_; }

  // Returns the pending loader, may be null.
  MatchPreviewLoader* pending_loader() const { return pending_loader_; }

  // The active loader is the loader that should be used for new loads. It is
  // either the pending loader or the current loader.
  MatchPreviewLoader* active_loader() const {
    return pending_loader_ ? pending_loader_ : current_loader_;
  }

  // Returns the number of instant loaders.
  // This is exposed for tests.
  size_t num_instant_loaders() const { return instant_loaders_.size(); }

 private:
  typedef std::map<TemplateURLID, MatchPreviewLoader*> Loaders;

  // Creates a loader and if |id| is non-zero registers it in instant_loaders_.
  MatchPreviewLoader* CreateLoader(TemplateURLID id);

  // Returns the loader for loading instant results with the specified id. If
  // there is no loader for the specified id a new one is created.
  MatchPreviewLoader* GetInstantLoader(TemplateURLID id);

  MatchPreviewLoaderDelegate* loader_delegate_;

  // The current loader.
  MatchPreviewLoader* current_loader_;

  // Loader we want to use as soon as ready. This is only non-null if
  // current_loader_ is ready and Update is invoked with a different template
  // url id.
  MatchPreviewLoader* pending_loader_;

  // Maps for template url id to loader used for that template url id.
  Loaders instant_loaders_;

  DISALLOW_COPY_AND_ASSIGN(LoaderManager);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_LOADER_MANAGER_H_
