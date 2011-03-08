// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_OPTIONS_IMPORT_DATA_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_OPTIONS_IMPORT_DATA_HANDLER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/ref_counted.h"
#include "chrome/browser/importer/importer_data_types.h"
#include "chrome/browser/importer/importer_list.h"
#include "chrome/browser/importer/importer_progress_observer.h"
#include "chrome/browser/ui/webui/options/options_ui.h"

class ImporterHost;

// Chrome personal stuff import data overlay UI handler.
class ImportDataHandler : public OptionsPageUIHandler,
                          public ImporterList::Observer,
                          public importer::ImporterProgressObserver {
 public:
  ImportDataHandler();
  virtual ~ImportDataHandler();

  // OptionsPageUIHandler:
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);
  virtual void Initialize();

  // WebUIMessageHandler:
  virtual void RegisterMessages();

 private:
  void ImportData(const ListValue* args);

  // ImporterList::Observer:
  virtual void SourceProfilesLoaded();

  // importer::ImporterProgressObserver:
  virtual void ImportStarted() OVERRIDE;
  virtual void ImportItemStarted(importer::ImportItem item) OVERRIDE;
  virtual void ImportItemEnded(importer::ImportItem item) OVERRIDE;
  virtual void ImportEnded() OVERRIDE;

  scoped_refptr<ImporterList> importer_list_;

  // If non-null it means importing is in progress. ImporterHost takes care
  // of deleting itself when import is complete.
  ImporterHost* importer_host_;  // weak

  DISALLOW_COPY_AND_ASSIGN(ImportDataHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_OPTIONS_IMPORT_DATA_HANDLER_H_
