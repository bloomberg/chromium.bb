// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOM_UI_IMPORT_DATA_HANDLER_H_
#define CHROME_BROWSER_DOM_UI_IMPORT_DATA_HANDLER_H_

#include "chrome/browser/dom_ui/options_ui.h"
#include "chrome/browser/importer/importer.h"

// Chrome personal stuff import data overlay UI handler.
class ImportDataHandler : public OptionsPageUIHandler {
 public:
  ImportDataHandler();
  virtual ~ImportDataHandler();

  virtual void Initialize();

  // OptionsUIHandler implementation.
  virtual void GetLocalizedValues(DictionaryValue* localized_strings);

  // DOMMessageHandler implementation.
  virtual void RegisterMessages();

 private:
  void DetectSupportedBrowsers();

  // Utility class that does the actual import.
  scoped_refptr<ImporterHost> importer_host_;

  DISALLOW_COPY_AND_ASSIGN(ImportDataHandler);
};

#endif  // CHROME_BROWSER_DOM_UI_IMPORT_DATA_HANDLER_H_
