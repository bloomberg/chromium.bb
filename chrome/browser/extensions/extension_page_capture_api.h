// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_CAPTURE_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_CAPTURE_API_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_function.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "webkit/blob/deletable_file_reference.h"

class FilePath;

class PageCaptureSaveAsMHTMLFunction : public AsyncExtensionFunction {
 public:
  PageCaptureSaveAsMHTMLFunction();

  // Test specific delegate used to test that the temporary file gets deleted.
  class TestDelegate {
   public:
    // Called on the UI thread when the temporary file that contains the
    // generated data has been created.
    virtual void OnTemporaryFileCreated(const FilePath& temp_file) = 0;
  };
  static void SetTestDelegate(TestDelegate* delegate);

 private:
  virtual ~PageCaptureSaveAsMHTMLFunction();
  virtual bool RunImpl() OVERRIDE;
  virtual bool OnMessageReceivedFromRenderView(
      const IPC::Message& message) OVERRIDE;

  // Called on the file thread.
  void CreateTemporaryFile();

  // Called on the UI thread.
  void TemporaryFileCreated(bool success);
  void ReturnFailure(const std::string& error);
  void ReturnSuccess(int64 file_size);

  // Callback called once the MHTML generation is done.
  void MHTMLGenerated(const FilePath& file_path, int64 mhtml_file_size);

  // Returns the TabContents we are associated with, NULL if it's been closed.
  TabContents* GetTabContents();

  int tab_id_;

  // The path to the temporary file containing the MHTML data.
  FilePath mhtml_path_;

  // The file containing the MHTML.
  scoped_refptr<webkit_blob::DeletableFileReference> mhtml_file_;

  DECLARE_EXTENSION_FUNCTION_NAME("pageCapture.saveAsMHTML")
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PAGE_CAPTURE_API_H_
