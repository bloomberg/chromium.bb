// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_OPTIONS_IMPORT_DATA_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_OPTIONS_IMPORT_DATA_HANDLER_H_

#include "base/ref_counted.h"
#include "chrome/browser/dom_ui/options/options_ui.h"
#include "chrome/browser/importer/importer.h"

// Chrome personal stuff import data overlay UI handler.
class ImportDataHandler : public OptionsPageUIHandler,
                          public ImporterHost::Observer,
                          public ImporterList::Observer {
 public:
  ImportDataHandler();
  virtual ~ImportDataHandler();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  void ImportData(const ListValue* args);

  // ImporterHost::Observer implementation.
  virtual void ImportStarted();
  virtual void ImportItemStarted(importer::ImportItem item);
  virtual void ImportItemEnded(importer::ImportItem item);
  virtual void ImportEnded();

  // ImporterList::Observer implementation.
  virtual void SourceProfilesLoaded();

  scoped_refptr<ImporterList> importer_list_;

  // If non-null it means importing is in progress. ImporterHost takes care
  // of deleting itself when import is complete.
  ImporterHost* importer_host_;  // weak

  DISALLOW_COPY_AND_ASSIGN(ImportDataHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_OPTIONS_IMPORT_DATA_HANDLER_H_
