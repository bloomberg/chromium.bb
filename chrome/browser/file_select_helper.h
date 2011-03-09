// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SELECT_HELPER_H_
#define CHROME_BROWSER_FILE_SELECT_HELPER_H_
#pragma once

#include <vector>

#include "chrome/browser/ui/shell_dialogs.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "net/base/directory_lister.h"

class Profile;
class RenderViewHost;
struct ViewHostMsg_RunFileChooser_Params;

class FileSelectHelper
    : public SelectFileDialog::Listener,
      public net::DirectoryLister::DirectoryListerDelegate,
      public NotificationObserver {
 public:
  explicit FileSelectHelper(Profile* profile);
  ~FileSelectHelper();

  // SelectFileDialog::Listener
  virtual void FileSelected(const FilePath& path, int index, void* params);
  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params);
  virtual void FileSelectionCanceled(void* params);

  // net::DirectoryLister::DirectoryListerDelegate
  virtual void OnListFile(
      const net::DirectoryLister::DirectoryListerData& data);
  virtual void OnListDone(int error);

  // Show the file chooser dialog.
  void RunFileChooser(RenderViewHost* render_view_host,
                      const ViewHostMsg_RunFileChooser_Params& params);

 private:
  // NotificationObserver implementation.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Helper method for handling the SelectFileDialog::Listener callbacks.
  void DirectorySelected(const FilePath& path);

  // Helper method to get allowed extensions for select file dialog from
  // the specified accept types as defined in the spec:
  //   http://whatwg.org/html/number-state.html#attr-input-accept
  SelectFileDialog::FileTypeInfo* GetFileTypesFromAcceptType(
      const string16& accept_types);

  // Profile used to set/retrieve the last used directory.
  Profile* profile_;

  // The RenderViewHost for the page we are associated with.
  RenderViewHost* render_view_host_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // The type of file dialog last shown.
  SelectFileDialog::Type dialog_type_;

  // The current directory lister (runs on a separate thread).
  scoped_refptr<net::DirectoryLister> directory_lister_;

  // The current directory lister results, which may update incrementally
  // as the listing proceeds.
  std::vector<FilePath> directory_lister_results_;

  // Registrar for notifications regarding our RenderViewHost.
  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(FileSelectHelper);
};

#endif  // CHROME_BROWSER_FILE_SELECT_HELPER_H_
