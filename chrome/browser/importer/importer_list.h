// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMPORTER_IMPORTER_LIST_H_
#define CHROME_BROWSER_IMPORTER_IMPORTER_LIST_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "content/browser/browser_thread.h"

namespace importer {
struct SourceProfile;
}

class ImporterList : public base::RefCountedThreadSafe<ImporterList> {
 public:
  // Any class calling DetectSourceProfiles() must implement this interface in
  // order to be called back when the source profiles are loaded.
  class Observer {
   public:
    virtual void OnSourceProfilesLoaded() = 0;

   protected:
    virtual ~Observer() {}
  };

  ImporterList();

  // Detects the installed browsers and their associated profiles, then stores
  // their information in a list. It returns the list of description of all
  // profiles. Calls into DetectSourceProfilesWorker() on the FILE thread to do
  // the real work of detecting source profiles. |observer| must be non-NULL.
  void DetectSourceProfiles(Observer* observer);

  // Sets the observer of this object. When the current observer is destroyed,
  // this method should be called with a NULL |observer| so it is not notified
  // after destruction.
  void SetObserver(Observer* observer);

  // DEPRECATED: This method is synchronous and performs file operations which
  // may end up blocking the current thread, which is usually the UI thread.
  void DetectSourceProfilesHack();

  // Returns the number of different source profiles you can import from.
  size_t count() const { return source_profiles_.size(); }

  // Returns the SourceProfile at |index|. The profiles are ordered such that
  // the profile at index 0 is the likely default browser. The SourceProfile
  // should be passed to ImporterHost::StartImportSettings().
  const importer::SourceProfile& GetSourceProfileAt(size_t index) const;

  // Returns the SourceProfile for the given |importer_type|.
  const importer::SourceProfile& GetSourceProfileForImporterType(
      int importer_type) const;

 private:
  friend class base::RefCountedThreadSafe<ImporterList>;

  ~ImporterList();

  // The worker method for DetectSourceProfiles(). Must be called on the FILE
  // thread.
  void DetectSourceProfilesWorker();

  // Called by DetectSourceProfilesWorker() on the source thread. This method
  // notifies |observer_| that the source profiles are loaded. |profiles| is
  // the vector of loaded profiles.
  void SourceProfilesLoaded(
      const std::vector<importer::SourceProfile*>& profiles);

  // The list of profiles with the default one first.
  ScopedVector<importer::SourceProfile> source_profiles_;

  // The ID of the thread DetectSourceProfiles() is called on. Only valid after
  // DetectSourceProfiles() is called and until SourceProfilesLoaded() has
  // returned.
  BrowserThread::ID source_thread_id_;

  // Weak reference. Only valid after DetectSourceProfiles() is called and until
  // SourceProfilesLoaded() has returned.
  Observer* observer_;

  // True if |observer_| is set during the lifetime of source profile detection.
  // This hack is necessary in order to not use |observer_| != NULL as a method
  // of determining whether this object is being observed or not.
  // TODO(jhawkins): Remove once DetectSourceProfilesHack() is removed.
  bool is_observed_;

  // True if source profiles are loaded.
  bool source_profiles_loaded_;

  DISALLOW_COPY_AND_ASSIGN(ImporterList);
};

#endif  // CHROME_BROWSER_IMPORTER_IMPORTER_LIST_H_
