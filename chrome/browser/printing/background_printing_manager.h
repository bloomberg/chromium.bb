// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
#define CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_

#include <map>
#include <set>

#include "base/compiler_specific.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class RenderProcessHost;
class WebContents;
}

namespace printing {

// Manages hidden WebContents that prints documents in the background.
// The hidden WebContents are no longer part of any Browser / TabStripModel.
// The WebContents started life as a ConstrainedPrintPreview dialog.
// They get deleted when the printing finishes.
class BackgroundPrintingManager : public base::NonThreadSafe,
                                  public content::NotificationObserver {
 public:
  typedef std::set<content::WebContents*> WebContentsSet;

  BackgroundPrintingManager();
  virtual ~BackgroundPrintingManager();

  // Takes ownership of |preview_dialog| and deletes it when |preview_dialog|
  // finishes printing. This removes |preview_dialog| from its ConstrainedDialog
  // and hides it from the user.
  void OwnPrintPreviewDialog(content::WebContents* preview_dialog);

  // Returns true if |printing_contents_set_| contains |preview_dialog|.
  bool HasPrintPreviewDialog(content::WebContents* preview_dialog);

  // Let others iterate over the list of background printing contents.
  WebContentsSet::const_iterator begin();
  WebContentsSet::const_iterator end();

 private:
  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Notifications handlers.
  void OnRendererProcessClosed(content::RenderProcessHost* rph);
  void OnPrintJobReleased(content::WebContents* preview_contents);
  void OnWebContentsDestroyed(content::WebContents* preview_contents);

  // Add |preview_contents| to the pending deletion set and schedule deletion.
  void DeletePreviewContents(content::WebContents* preview_contents);

  // Check if any of the WebContentses in |set| share a RenderProcessHost
  // with |tab|, excluding |tab|.
  bool HasSharedRenderProcessHost(const WebContentsSet& set,
                                  content::WebContents* preview_contents);

  // The set of print preview WebContentses managed by
  // BackgroundPrintingManager.
  WebContentsSet printing_contents_set_;

  // The set of print preview Webcontents managed by BackgroundPrintingManager
  // that are pending deletion.
  WebContentsSet printing_contents_pending_deletion_set_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundPrintingManager);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_BACKGROUND_PRINTING_MANAGER_H_
