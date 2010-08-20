// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_IMPORT_DATA_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_IMPORT_DATA_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"
#include "chrome/browser/importer/importer.h"

// Chrome personal stuff import data overlay UI handler.
class ImportDataHandler : public OptionsPageUIHandler,
                          public ImporterHost::Observer {
 public:
  ImportDataHandler();
  virtual ~ImportDataHandler();

  virtual void Initialize();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  void LoadImporter(const ListValue* args);
  void DetectSupportedBrowsers();
  void ImportData(const ListValue* args);

  //Callback from ImporterHost. Close the Dialog.
  virtual void ImportStarted();
  virtual void ImportItemStarted(importer::ImportItem item);
  virtual void ImportItemEnded(importer::ImportItem item);
  virtual void ImportEnded();

  // If non-null it means importing is in progress. ImporterHost takes care
  // of deleting itself when done import.
  scoped_refptr<ImporterHost> importer_host_;

  DISALLOW_COPY_AND_ASSIGN(ImportDataHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_IMPORT_DATA_HANDLER_H_
