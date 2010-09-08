// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_FILE_SELECT_HELPER_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_FILE_SELECT_HELPER_H_
#pragma once

#include <vector>

#include "chrome/browser/shell_dialogs.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "net/base/directory_lister.h"

class RenderViewHost;
class TabContents;
struct ViewHostMsg_RunFileChooser_Params;

class TabContentsFileSelectHelper
    : public SelectFileDialog::Listener,
      public net::DirectoryLister::DirectoryListerDelegate,
      public RenderViewHostDelegate::FileSelect {
 public:
  explicit TabContentsFileSelectHelper(TabContents* tab_contents);

  ~TabContentsFileSelectHelper();

  // SelectFileDialog::Listener
  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params);
  virtual void FileSelectionCanceled(void* params);

  // net::DirectoryLister::DirectoryListerDelegate
  virtual void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data);
  virtual void OnListDone(int error);

  // RenderViewHostDelegate::FileSelect
  virtual void RunFileChooser(const ViewHostMsg_RunFileChooser_Params& params);

 private:
  // Returns the RenderViewHost of tab_contents_.
  RenderViewHost* GetRenderViewHost();

  // Helper method for handling the SelectFileDialog::Listener callbacks.
  void DirectorySelected(const FilePath& path);

  // Helper method to get allowed extensions for select file dialog from
  // the specified accept types as defined in the spec:
  //   http://whatwg.org/html/number-state.html#attr-input-accept
  SelectFileDialog::FileTypeInfo* GetFileTypesFromAcceptType(
      const string16& accept_types);

  // The tab contents this class is helping.  |tab_contents_| owns this object,
  // so this pointer is guaranteed to be valid.
  TabContents* tab_contents_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // The type of file dialog last shown.
  SelectFileDialog::Type dialog_type_;

  // The current directory lister (runs on a separate thread).
  scoped_refptr<net::DirectoryLister> directory_lister_;

  // The current directory lister results, which may update incrementally
  // as the listing proceeds.
  std::vector<FilePath> directory_lister_results_;

  DISALLOW_COPY_AND_ASSIGN(TabContentsFileSelectHelper);
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_FILE_SELECT_HELPER_H_
