// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SELECT_HELPER_H_
#define CHROME_BROWSER_FILE_SELECT_HELPER_H_
#pragma once

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "net/base/directory_lister.h"

class Profile;
class RenderViewHost;

namespace content {
struct FileChooserParams;
}

// This class handles file-selection requests coming from WebUI elements
// (via the ExtensionHost class). It implements both the initialisation
// and listener functions for file-selection dialogs.
class FileSelectHelper
    : public base::RefCountedThreadSafe<FileSelectHelper>,
      public SelectFileDialog::Listener,
      public content::NotificationObserver {
 public:
  explicit FileSelectHelper(Profile* profile);

  // Show the file chooser dialog.
  void RunFileChooser(RenderViewHost* render_view_host,
                      TabContents* tab_contents,
                      const content::FileChooserParams& params);

  // Enumerates all the files in directory.
  void EnumerateDirectory(int request_id,
                          RenderViewHost* render_view_host,
                          const FilePath& path);

 private:
  friend class base::RefCountedThreadSafe<FileSelectHelper>;
  virtual ~FileSelectHelper();

  // Utility class which can listen for directory lister events and relay
  // them to the main object with the correct tracking id.
  class DirectoryListerDispatchDelegate
      : public net::DirectoryLister::DirectoryListerDelegate {
   public:
    DirectoryListerDispatchDelegate(FileSelectHelper* parent, int id)
        : parent_(parent),
          id_(id) {}
    virtual ~DirectoryListerDispatchDelegate() {}
    virtual void OnListFile(
        const net::DirectoryLister::DirectoryListerData& data) OVERRIDE {
      parent_->OnListFile(id_, data);
    }
    virtual void OnListDone(int error) OVERRIDE {
      parent_->OnListDone(id_, error);
    }
   private:
    // This FileSelectHelper owns this object.
    FileSelectHelper* parent_;
    int id_;

    DISALLOW_COPY_AND_ASSIGN(DirectoryListerDispatchDelegate);
  };

  void RunFileChooserOnFileThread(
      const content::FileChooserParams& params);
  void RunFileChooserOnUIThread(
      const content::FileChooserParams& params);

  // Cleans up and releases this instance. This must be called after the last
  // callback is received from the file chooser dialog.
  void RunFileChooserEnd();

  // SelectFileDialog::Listener overrides.
  virtual void FileSelected(
      const FilePath& path, int index, void* params) OVERRIDE;
  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Kicks off a new directory enumeration.
  void StartNewEnumeration(const FilePath& path,
                           int request_id,
                           RenderViewHost* render_view_host);

  // Callbacks from directory enumeration.
  virtual void OnListFile(
      int id,
      const net::DirectoryLister::DirectoryListerData& data);
  virtual void OnListDone(int id, int error);

  // Cleans up and releases this instance. This must be called after the last
  // callback is received from the enumeration code.
  void EnumerateDirectoryEnd();

  // Helper method to get allowed extensions for select file dialog from
  // the specified accept types as defined in the spec:
  //   http://whatwg.org/html/number-state.html#attr-input-accept
  // |accept_types| contains only valid lowercased MIME types.
  SelectFileDialog::FileTypeInfo* GetFileTypesFromAcceptType(
      const std::vector<string16>& accept_types);

  // Profile used to set/retrieve the last used directory.
  Profile* profile_;

  // The RenderViewHost and TabContents for the page showing a file dialog
  // (may only be one such dialog).
  RenderViewHost* render_view_host_;
  TabContents* tab_contents_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<SelectFileDialog> select_file_dialog_;
  scoped_ptr<SelectFileDialog::FileTypeInfo> select_file_types_;

  // The type of file dialog last shown.
  SelectFileDialog::Type dialog_type_;

  // Maintain a list of active directory enumerations.  These could come from
  // the file select dialog or from drag-and-drop of directories, so there could
  // be more than one going on at a time.
  struct ActiveDirectoryEnumeration;
  std::map<int, ActiveDirectoryEnumeration*> directory_enumerations_;

  // Registrar for notifications regarding our RenderViewHost.
  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(FileSelectHelper);
};

#endif  // CHROME_BROWSER_FILE_SELECT_HELPER_H_
