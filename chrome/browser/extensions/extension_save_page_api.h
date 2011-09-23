// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SAVE_PAGE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SAVE_PAGE_API_H_

#include <string>

#include "chrome/browser/extensions/extension_function.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class SavePageAsMHTMLFunction : public AsyncExtensionFunction,
                                public NotificationObserver {
 public:
  SavePageAsMHTMLFunction();

 private:
  virtual ~SavePageAsMHTMLFunction();

  virtual bool RunImpl() OVERRIDE;
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Called on the file thread.
  void CreateTemporaryFile();

  // Called on the UI thread.
  void TemporaryFileCreated(bool success);
  void ReturnFailure(const std::string& error);
  void ReturnSuccess(int64 file_size);

  int tab_id_;

  // The path to the temporary file containing the MHTML data.
  FilePath mhtml_path_;

  NotificationRegistrar registrar_;
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.savePage.saveAsMHTML")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SAVE_PAGE_API_H_
