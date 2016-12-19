// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_EXTERNAL_FILE_REMOVER_H_
#define IOS_CHROME_BROWSER_UI_EXTERNAL_FILE_REMOVER_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/sessions/core/tab_restore_service_observer.h"

@class BrowserViewController;
namespace sessions {
class TabRestoreService;
}

// ExternalFileRemover is responsible for removing documents received from other
// applications that are not in the list of recently closed tabs, open tabs or
// bookmarks.
class ExternalFileRemover : public sessions::TabRestoreServiceObserver {
 public:
  // Creates an ExternalFileRemover to remove external documents not referenced
  // by the specified BrowserViewController. Use Remove to initiate the removal.
  explicit ExternalFileRemover(BrowserViewController* bvc);
  ~ExternalFileRemover() override;

  // sessions::TabRestoreServiceObserver methods
  void TabRestoreServiceChanged(sessions::TabRestoreService* service) override;
  void TabRestoreServiceDestroyed(
      sessions::TabRestoreService* service) override;

  // Post a delayed task to clean up the files received from other applications.
  // |callback| is called when the clean up has finished.
  void RemoveAfterDelay(const base::TimeDelta& delay,
                        const base::Closure& callback);

 private:
  // Removes files received from other apps. If |all_files| is true, then
  // all files including files that may be referenced by tabs through restore
  // service or history. Otherwise, only the unreferenced files are removed.
  // |callback| is called when the removal finishes.
  void RemoveFiles(bool all_files, const base::Closure& callback);
  // Pointer to the tab restore service in the browser state associated with
  // |bvc_|.
  sessions::TabRestoreService* tabRestoreService_;
  // BrowserViewController used to get the referenced files. Must outlive this
  // object.
  BrowserViewController* bvc_;
  // Used to ensure |Remove()| is not run when this object is destroyed.
  base::WeakPtrFactory<ExternalFileRemover> weak_ptr_factory_;
  // Loads the |tabRestoreService_| if necessary. Removes all files received
  // from other apps if |all_files| is true. Otherwise, removes the unreferenced
  // files only. |callback| is called when the removal finishes.
  void Remove(bool all_files, const base::Closure& callback);
};

#endif  // IOS_CHROME_BROWSER_UI_EXTERNAL_FILE_REMOVER_H_
