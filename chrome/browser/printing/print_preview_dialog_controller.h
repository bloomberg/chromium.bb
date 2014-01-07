// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "chrome/browser/sessions/session_id.h"

class GURL;

namespace content {
struct LoadCommittedDetails;
class RenderProcessHost;
class WebContents;
}

namespace printing {

// For print preview, the WebContents that initiates the printing operation is
// the initiator, and the constrained dialog that shows the print preview is the
// print preview dialog.
// This class manages print preview dialog creation and destruction, and keeps
// track of the 1:1 relationship between initiator tabs and print preview
// dialogs.
class PrintPreviewDialogController
    : public base::RefCounted<PrintPreviewDialogController> {
 public:
  PrintPreviewDialogController();

  static PrintPreviewDialogController* GetInstance();

  // Initiate print preview for |initiator|.
  // Call this instead of GetOrCreatePreviewDialog().
  static void PrintPreview(content::WebContents* initiator);

  // Get/Create the print preview dialog for |initiator|.
  // Exposed for unit tests.
  content::WebContents* GetOrCreatePreviewDialog(
      content::WebContents* initiator);

  // Returns the preview dialog for |contents|.
  // Returns |contents| if |contents| is a preview dialog.
  // Returns NULL if no preview dialog exists for |contents|.
  content::WebContents* GetPrintPreviewForContents(
      content::WebContents* contents) const;

  // Returns the initiator for |preview_dialog|.
  // Returns NULL if no initiator exists for |preview_dialog|.
  content::WebContents* GetInitiator(content::WebContents* preview_dialog);

  // Returns true if |contents| is a print preview dialog.
  static bool IsPrintPreviewDialog(content::WebContents* contents);

  // Returns true if |url| is a print preview url.
  static bool IsPrintPreviewURL(const GURL& url);

  // Erase the initiator info associated with |preview_dialog|.
  void EraseInitiatorInfo(content::WebContents* preview_dialog);

  bool is_creating_print_preview_dialog() const {
    return is_creating_print_preview_dialog_;
  }

 private:
  friend class base::RefCounted<PrintPreviewDialogController>;
  struct Operation;

  virtual ~PrintPreviewDialogController();

  // Handlers for observed events.
  void OnRenderProcessExited(content::RenderProcessHost* rph);
  void OnWebContentsDestroyed(content::WebContents* contents);
  void OnNavigationEntryCommitted(content::WebContents* contents,
                                  const content::LoadCommittedDetails* details);

  // Creates a new print preview dialog.
  content::WebContents* CreatePrintPreviewDialog(
      content::WebContents* initiator);

  // Helper function to store the title of the initiator associated with
  // |preview_dialog| in |preview_dialog|'s PrintPreviewUI.
  void SaveInitiatorTitle(content::WebContents* preview_dialog);

  // Removes WebContents when they close/crash/navigate.
  void RemoveInitiator(content::WebContents* initiator);
  void RemovePreviewDialog(content::WebContents* preview_dialog);

  // The list of the currently active preview operations.
  std::vector<Operation*> preview_operations_;

  // True if the controller is waiting for a new preview dialog via
  // content::NAVIGATION_TYPE_NEW_PAGE.
  bool waiting_for_new_preview_page_;

  // Whether the PrintPreviewDialogController is in the middle of creating a
  // print preview dialog.
  bool is_creating_print_preview_dialog_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogController);
};

}  // namespace printing

#endif  // CHROME_BROWSER_PRINTING_PRINT_PREVIEW_DIALOG_CONTROLLER_H_
