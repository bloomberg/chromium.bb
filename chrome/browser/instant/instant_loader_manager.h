// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTANT_INSTANT_LOADER_MANAGER_H_
#define CHROME_BROWSER_INSTANT_INSTANT_LOADER_MANAGER_H_
#pragma once

#include <map>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/search_engines/template_url_id.h"

class InstantLoader;
class InstantLoaderDelegate;

// InstantLoaderManager is responsible for maintaining the InstantLoaders for
// InstantController. InstantLoaderManager keeps track of one loader for loading
// non-instant urls, and a loader per TemplateURLID for loading instant urls. A
// loader per TemplateURLID is necessitated due to not knowing in advance if a
// site really supports instant (for example, the user might have opted out even
// though it's supported).
//
// Users of InstantLoaderManager need only concern themselves with the current
// and pending loaders. The current loader is the loader that if ready is shown
// by InstantController. The pending loader is used if the current loader is
// ready and update is invoked with a different id. In this case the current
// loader is left as current (and it's preview contents stopped) and the newly
// created loader is set to pending.  Once the pending loader is ready
// MakePendingCurrent should be invoked to make the pending the current loader.
//
// InstantLoader owns all the InstantLoaders returned. You can take
// ownership of the current loader by invoking ReleaseCurrentLoader.
class InstantLoaderManager {
 public:
  explicit InstantLoaderManager(InstantLoaderDelegate* loader_delegate);
  ~InstantLoaderManager();

  // Updates the current loader. If the current loader is replaced and should be
  // deleted it is set in |old_loader|. This is done to allow the caller to
  // notify delegates before the old loader is destroyed. This returns the
  // active InstantLoader that should be used.
  InstantLoader* UpdateLoader(TemplateURLID instant_id,
                              scoped_ptr<InstantLoader>* old_loader);

  // Returns true if invoking |UpdateLoader| with |instant_id| would change the
  // active loader.
  bool WillUpateChangeActiveLoader(TemplateURLID instant_id);

  // Makes the pending loader the current loader. If ownership of the old
  // loader is to pass to the caller |old_loader| is set appropriately.
  void MakePendingCurrent(scoped_ptr<InstantLoader>* old_loader);

  // Returns the current loader and clears internal references to it.  This
  // should be used prior to destroying the InstantLoaderManager when the owner
  // of InstantLoaderManager wants to take ownership of the loader.
  InstantLoader* ReleaseCurrentLoader();

  // Destroys the specified loader.
  void DestroyLoader(InstantLoader* loader);

  // Removes references to loader.
  InstantLoader* ReleaseLoader(InstantLoader* loader);

  // If |loader| is in |instant_loaders_| it is removed.
  void RemoveLoaderFromInstant(InstantLoader* loader);

  // Returns the current loader, may be null.
  InstantLoader* current_loader() const { return current_loader_; }

  // Returns the pending loader, may be null.
  InstantLoader* pending_loader() const { return pending_loader_; }

  // The active loader is the loader that should be used for new loads. It is
  // either the pending loader or the current loader.
  InstantLoader* active_loader() const {
    return pending_loader_ ? pending_loader_ : current_loader_;
  }

  // Returns the number of instant loaders.
  // This is exposed for tests.
  size_t num_instant_loaders() const { return instant_loaders_.size(); }

 private:
  typedef std::map<TemplateURLID, InstantLoader*> Loaders;

  // Creates a loader and if |id| is non-zero registers it in instant_loaders_.
  InstantLoader* CreateLoader(TemplateURLID id);

  // Returns the loader for loading instant results with the specified id. If
  // there is no loader for the specified id a new one is created.
  InstantLoader* GetInstantLoader(TemplateURLID id);

  InstantLoaderDelegate* loader_delegate_;

  // The current loader.
  InstantLoader* current_loader_;

  // Loader we want to use as soon as ready. This is only non-null if
  // current_loader_ is ready and Update is invoked with a different template
  // url id.
  InstantLoader* pending_loader_;

  // Maps for template url id to loader used for that template url id.
  Loaders instant_loaders_;

  DISALLOW_COPY_AND_ASSIGN(InstantLoaderManager);
};

#endif  // CHROME_BROWSER_INSTANT_INSTANT_LOADER_MANAGER_H_
