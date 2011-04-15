// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FILE_SELECT_HELPER_H_
#define CHROME_BROWSER_FILE_SELECT_HELPER_H_
#pragma once

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/shell_dialogs.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "net/base/directory_lister.h"

class Profile;
class RenderViewHost;
struct ViewHostMsg_RunFileChooser_Params;


// This class handles file-selection requests coming from WebUI elements
// (via the ExtensionHost class). It implements both the initialisation
// and listener functions for file-selection dialogs.
class FileSelectHelper
    : public SelectFileDialog::Listener,
      public NotificationObserver {
 public:
  explicit FileSelectHelper(Profile* profile);
  ~FileSelectHelper();

  // Show the file chooser dialog.
  void RunFileChooser(RenderViewHost* render_view_host,
                      TabContents* tab_contents,
                      const ViewHostMsg_RunFileChooser_Params& params);

  // Enumerates all the files in directory.
  void EnumerateDirectory(int request_id,
                          RenderViewHost* render_view_host,
                          const FilePath& path);

 private:
  // Utility class which can listen for directory lister events and relay
  // them to the main object with the correct tracking id.
  class DirectoryListerDispatchDelegate
      : public net::DirectoryLister::DirectoryListerDelegate {
   public:
    DirectoryListerDispatchDelegate(FileSelectHelper* parent, int id)
        : parent_(parent),
          id_(id) {}
    ~DirectoryListerDispatchDelegate() {}
    virtual void OnListFile(
        const net::DirectoryLister::DirectoryListerData& data) {
      parent_->OnListFile(id_, data);
    }
    virtual void OnListDone(int error) {
      parent_->OnListDone(id_, error);
    }
   private:
    // This FileSelectHelper owns this object.
    FileSelectHelper* parent_;
    int id_;

    DISALLOW_COPY_AND_ASSIGN(DirectoryListerDispatchDelegate);
  };

  // SelectFileDialog::Listener overrides.
  virtual void FileSelected(
      const FilePath& path, int index, void* params) OVERRIDE;
  virtual void MultiFilesSelected(const std::vector<FilePath>& files,
                                  void* params) OVERRIDE;
  virtual void FileSelectionCanceled(void* params) OVERRIDE;

  // NotificationObserver overrides.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Kicks off a new directory enumeration.
  void StartNewEnumeration(const FilePath& path,
                           int request_id,
                           RenderViewHost* render_view_host);

  // Callbacks from directory enumeration.
  virtual void OnListFile(
      int id,
      const net::DirectoryLister::DirectoryListerData& data);
  virtual void OnListDone(int id, int error);

  // Helper method to get allowed extensions for select file dialog from
  // the specified accept types as defined in the spec:
  //   http://whatwg.org/html/number-state.html#attr-input-accept
  SelectFileDialog::FileTypeInfo* GetFileTypesFromAcceptType(
      const string16& accept_types);

  // Profile used to set/retrieve the last used directory.
  Profile* profile_;

  // The RenderViewHost for the page showing a file dialog (may only be one
  // such dialog).
  RenderViewHost* render_view_host_;

  // Dialog box used for choosing files to upload from file form fields.
  scoped_refptr<SelectFileDialog> select_file_dialog_;

  // The type of file dialog last shown.
  SelectFileDialog::Type dialog_type_;

  // Maintain a list of active directory enumerations.  These could come from
  // the file select dialog or from drag-and-drop of directories, so there could
  // be more than one going on at a time.
  struct ActiveDirectoryEnumeration;
  std::map<int, ActiveDirectoryEnumeration*> directory_enumerations_;

  // Registrar for notifications regarding our RenderViewHost.
  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(FileSelectHelper);
};

class FileSelectObserver : public TabContentsObserver {
 public:
  explicit FileSelectObserver(TabContents* tab_contents);
  ~FileSelectObserver();

 private:
  // TabContentsObserver overrides.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  // Called when a file selection is to be done.
  void OnRunFileChooser(const ViewHostMsg_RunFileChooser_Params& params);

  // Called when a direction enumeration is to be done.
  void OnEnumerateDirectory(int request_id, const FilePath& path);

  // FileSelectHelper, lazily created.
  scoped_ptr<FileSelectHelper> file_select_helper_;

  DISALLOW_COPY_AND_ASSIGN(FileSelectObserver);
};

#endif  // CHROME_BROWSER_FILE_SELECT_HELPER_H_
