// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_SAVE_PAGE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_SAVE_PAGE_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "webkit/blob/deletable_file_reference.h"

class SavePageAsMHTMLFunction : public AsyncExtensionFunction,
                                public content::NotificationObserver {
 public:
  SavePageAsMHTMLFunction();

  // Test specific delegate used to test that the temporary file gets deleted.
  class TestDelegate {
   public:
    // Called on the UI thread when the temporary file that contains the
    // generated data has been created.
    virtual void OnTemporaryFileCreated(const FilePath& temp_file) = 0;
  };
  static void SetTestDelegate(TestDelegate* delegate);

 private:
  virtual ~SavePageAsMHTMLFunction();
  virtual bool RunImpl() OVERRIDE;
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
  virtual bool OnMessageReceivedFromRenderView(
      const IPC::Message& message) OVERRIDE;

  // Called on the file thread.
  void CreateTemporaryFile();

  // Called on the UI thread.
  void TemporaryFileCreated(bool success);
  void ReturnFailure(const std::string& error);
  void ReturnSuccess(int64 file_size);

  // Returns the TabContents we are associated with, NULL if it's been closed.
  TabContents* GetTabContents();

  int tab_id_;

  // The path to the temporary file containing the MHTML data.
  FilePath mhtml_path_;

  // The file containing the MHTML.
  scoped_refptr<webkit_blob::DeletableFileReference> mhtml_file_;

  content::NotificationRegistrar registrar_;

  DECLARE_EXTENSION_FUNCTION_NAME("experimental.savePage.saveAsMHTML")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_SAVE_PAGE_API_H_
