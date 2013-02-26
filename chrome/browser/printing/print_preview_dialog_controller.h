// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_

#include <map>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/sessions/session_id.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class GURL;

namespace content {
struct LoadCommittedDetails;
class RenderProcessHost;
class WebContents;
}

namespace printing {

// For print preview, a print preview (PP) tab is linked with the initiator tab
// that initiated the printing operation. If the tab initiates a second
// printing operation while the first print preview tab is still open, that PP
// tab is focused/activated. There may be more than one PP tab open. There is a
// 1:1 relationship between PP tabs and initiating tabs. This class manages PP
// tabs and initiator tabs.
//
// THE ABOVE COMMENT IS OBSOLETE
//
// This class needs to be renamed. It no longer controls tabs. All tests need to
// be reevaluated as to whether they're useful. The comments, both here and in
// the tests, need to be fixed so that they don't lie.
// http://crbug.com/163671
class PrintPreviewDialogController
    : public base::RefCounted<PrintPreviewDialogController>,
      public content::NotificationObserver {
 public:
  PrintPreviewDialogController();

  static PrintPreviewDialogController* GetInstance();

  // Initiate print preview for |initiator_tab|.
  // Call this instead of GetOrCreatePreviewDialog().
  static void PrintPreview(content::WebContents* initiator_tab);

  // Get/Create the print preview dialog for |initiator_tab|.
  // Exposed for unit tests.
  content::WebContents* GetOrCreatePreviewDialog(
      content::WebContents* initiator_tab);

  // Returns the preview dialog for |contents|.
  // Returns |contents| if |contents| is a preview dialog.
  // Returns NULL if no preview dialog exists for |contents|.
  content::WebContents* GetPrintPreviewForContents(
      content::WebContents* contents) const;

  // Returns initiator tab for |preview_tab|.
  // Returns NULL if no initiator tab exists for |preview_tab|.
  content::WebContents* GetInitiatorTab(content::WebContents* preview_tab);

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Returns true if |contents| is a print preview dialog.
  static bool IsPrintPreviewDialog(content::WebContents* contents);

  // Returns true if |url| is a print preview url.
  static bool IsPrintPreviewURL(const GURL& url);

  // Erase the initiator tab info associated with |preview_tab|.
  void EraseInitiatorTabInfo(content::WebContents* preview_tab);

  bool is_creating_print_preview_dialog() const {
    return is_creating_print_preview_dialog_;
  }

  void set_print_preview_tab_created_callback_for_testing(
      const base::Closure& callback) {
    print_preview_tab_created_callback_ = callback;
  }

 private:
  friend class base::RefCounted<PrintPreviewDialogController>;

  // 1:1 relationship between initiator tab and print preview tab.
  // Key: Preview tab.
  // Value: Initiator tab.
  typedef std::map<content::WebContents*, content::WebContents*>
      PrintPreviewTabMap;

  virtual ~PrintPreviewDialogController();

  // Handler for the RENDERER_PROCESS_CLOSED notification. This is observed when
  // the initiator renderer crashed.
  void OnRendererProcessClosed(content::RenderProcessHost* rph);

  // Handler for the WEB_CONTENTS_DESTROYED notification. This is observed when
  // either tab is closed.
  void OnWebContentsDestroyed(content::WebContents* tab);

  // Handler for the NAV_ENTRY_COMMITTED notification. This is observed when the
  // renderer is navigated to a different page.
  void OnNavEntryCommitted(content::WebContents* tab,
                           content::LoadCommittedDetails* details);

  // Creates a new print preview tab.
  content::WebContents* CreatePrintPreviewTab(
      content::WebContents* initiator_tab);

  // Helper function to store the title of the initiator tab associated with
  // |preview_dialog| in |preview_dialog|'s PrintPreviewUI.
  void SaveInitiatorTabTitle(content::WebContents* preview_dialog);

  // Adds/Removes observers for notifications from |tab|.
  void AddObservers(content::WebContents* tab);
  void RemoveObservers(content::WebContents* tab);

  // Removes tabs when they close/crash/navigate.
  void RemoveInitiatorTab(content::WebContents* initiator_tab);
  void RemovePreviewTab(content::WebContents* preview_tab);

  // Mapping between print preview tab and the corresponding initiator tab.
  PrintPreviewTabMap preview_tab_map_;

  // A registrar for listening notifications.
  content::NotificationRegistrar registrar_;

  // True if the controller is waiting for a new preview tab via
  // content::NAVIGATION_TYPE_NEW_PAGE.
  bool waiting_for_new_preview_page_;

  // Whether the PrintPreviewDialogController is in the middle of creating a
  // print preview dialog.
  bool is_creating_print_preview_dialog_;

  base::Closure print_preview_tab_created_callback_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogController);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_
