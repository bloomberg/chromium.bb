// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_H_
#define CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_H_
#pragma once

#include <map>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/ui/select_file_dialog.h"
#include "chrome/browser/ui/views/extensions/extension_dialog_observer.h"
#include "ui/gfx/native_widget_types.h"  // gfx::NativeWindow

class ExtensionDialog;
class RenderViewHost;

// Shows a dialog box for selecting a file or a folder, using the
// file manager extension implementation.
class SelectFileDialogExtension
    : public SelectFileDialog,
      public ExtensionDialogObserver {
 public:
  static SelectFileDialogExtension* Create(
      SelectFileDialog::Listener* listener);

  // BaseShellDialog implementation.
  virtual bool IsRunning(gfx::NativeWindow owner_window) const OVERRIDE;
  virtual void ListenerDestroyed() OVERRIDE;

  // ExtensionDialog::Observer implementation.
  virtual void ExtensionDialogIsClosing(ExtensionDialog* dialog) OVERRIDE;

  // Routes callback to appropriate SelectFileDialog::Listener based on
  // the owning |tab_id|, then closes the dialog.
  static void OnFileSelected(int32 tab_id, const FilePath& path, int index);
  static void OnMultiFilesSelected(int32 tab_id,
                                   const std::vector<FilePath>& files);
  static void OnFileSelectionCanceled(int32 tab_id);

  // For testing, so we can inject JavaScript into the contained view.
  RenderViewHost* GetRenderViewHost();

 protected:
  // SelectFileDialog implementation.
  virtual void SelectFileImpl(Type type,
                              const string16& title,
                              const FilePath& default_path,
                              const FileTypeInfo* file_types,
                              int file_type_index,
                              const FilePath::StringType& default_extension,
                              gfx::NativeWindow owning_window,
                              void* params) OVERRIDE;


 private:
  friend class SelectFileDialogExtensionBrowserTest;
  friend class SelectFileDialogExtensionTest;

  // Object is ref-counted, use Create().
  explicit SelectFileDialogExtension(SelectFileDialog::Listener* listener);
  virtual ~SelectFileDialogExtension();

  // Closes the dialog and cleans up callbacks and listeners.
  void Close();

  // Posts a task to close the dialog.
  void PostTaskToClose();

  // Adds this to the list of pending dialogs, used for testing.
  void AddPending(int32 tab_id);

  // Check if the list of pending dialogs contains dialog for |tab_id|.
  static bool PendingExists(int32 tab_id);

  // Returns if the dialog has mutiple file type choices
  virtual bool HasMultipleFileTypeChoicesImpl() OVERRIDE;

  bool hasMultipleFileTypeChoices_;
  void* params_;

  // Host for the extension that implements this dialog.
  scoped_refptr<ExtensionDialog> extension_dialog_;

  // ID of the tab that spawned this dialog, used to route callbacks.
  int32 tab_id_;

  gfx::NativeWindow owner_window_;

  DISALLOW_COPY_AND_ASSIGN(SelectFileDialogExtension);
};

#endif  // CHROME_BROWSER_UI_VIEWS_SELECT_FILE_DIALOG_EXTENSION_H_
