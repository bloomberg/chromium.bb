// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_PROGRESS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_PROGRESS_DIALOG_VIEW_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

class ImporterHost;
class ImporterObserver;

namespace views {
class CheckmarkThrobber;
class Label;
}

class ImportProgressDialogView : public views::DialogDelegateView,
                                 public importer::ImporterProgressObserver {
 public:
  // |items| is a bitmask of importer::ImportItem being imported.
  // |bookmark_import| is true if we're importing bookmarks from a
  // bookmarks.html file.
  ImportProgressDialogView(uint16 items,
                           ImporterHost* importer_host,
                           ImporterObserver* importer_observer,
                           const string16& importer_name,
                           bool bookmarks_import);
  virtual ~ImportProgressDialogView();

 protected:
  // views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

  // views::DialogDelegate:
  virtual int GetDialogButtons() const OVERRIDE;
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual bool Cancel() OVERRIDE;

 private:
  // Set up the control layout within this dialog.
  void InitControlLayout();

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE;
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE;
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE;
  virtual void ImportEnded() OVERRIDE;

  // Various dialog controls.
  scoped_ptr<views::CheckmarkThrobber> state_bookmarks_;
  scoped_ptr<views::CheckmarkThrobber> state_searches_;
  scoped_ptr<views::CheckmarkThrobber> state_passwords_;
  scoped_ptr<views::CheckmarkThrobber> state_history_;
  scoped_ptr<views::CheckmarkThrobber> state_cookies_;
  views::Label* label_info_;
  scoped_ptr<views::Label> label_bookmarks_;
  scoped_ptr<views::Label> label_searches_;
  scoped_ptr<views::Label> label_passwords_;
  scoped_ptr<views::Label> label_history_;
  scoped_ptr<views::Label> label_cookies_;

  // Items to import from the other browser
  uint16 items_;

  // Utility class that does the actual import.
  scoped_refptr<ImporterHost> importer_host_;

  // Observer that we need to notify about import events.
  ImporterObserver* importer_observer_;

  // True if the import operation is in progress.
  bool importing_;

  // Are we importing a bookmarks.html file?
  bool bookmarks_import_;

  DISALLOW_COPY_AND_ASSIGN(ImportProgressDialogView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_IMPORTER_IMPORT_PROGRESS_DIALOG_VIEW_H_
