// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_IMPORT_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_IMPORT_DATA_HANDLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/ui/webui/options/options_ui.h"
#include "chrome/common/importer/importer_data_types.h"
#include "ui/shell_dialogs/select_file_dialog.h"

class ExternalProcessImporterHost;
class ImporterList;

namespace options {

// Chrome personal stuff import data overlay UI handler.
class ImportDataHandler : public OptionsPageUIHandler,
                          public importer::ImporterProgressObserver,
                          public ui::SelectFileDialog::Listener {
 public:
  ImportDataHandler();
  virtual ~ImportDataHandler();

  // OptionsPageUIHandler:
  virtual void GetLocalizedValues(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void InitializeHandler() OVERRIDE;
  virtual void InitializePage() OVERRIDE;

  // content::WebUIMessageHandler:
  virtual void RegisterMessages() OVERRIDE;

 private:
  void StartImport(const importer::SourceProfile& source_profile,
                   uint16 imported_items);

  void ImportData(const base::ListValue* args);

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE;
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE;
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE;
  virtual void ImportEnded() OVERRIDE;

  // ui::SelectFileDialog::Listener:
  virtual void FileSelected(const base::FilePath& path,
                            int index,
                            void* params) OVERRIDE;

  // Opens a file selection dialog to choose the bookmarks HTML file.
  void HandleChooseBookmarksFile(const base::ListValue* args);

  scoped_ptr<ImporterList> importer_list_;

  // If non-null it means importing is in progress. ImporterHost takes care
  // of deleting itself when import is complete.
  ExternalProcessImporterHost* importer_host_;  // weak

  bool import_did_succeed_;

  scoped_refptr<ui::SelectFileDialog> select_file_dialog_;

  DISALLOW_COPY_AND_ASSIGN(ImportDataHandler);
};

}  // namespace options

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_IMPORT_DATA_HANDLER_H_
