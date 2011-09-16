// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file implements commond select dialog functionality between GTK and KDE.

#include "chrome/browser/ui/gtk/dialogs_common.h"

#include "base/environment.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/nix/xdg_util.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "content/browser/browser_thread.h"

FilePath* SelectFileDialogImpl::last_saved_path_ = NULL;
FilePath* SelectFileDialogImpl::last_opened_path_ = NULL;

// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop =
      base::nix::GetDesktopEnvironment(env.get());
  if (desktop == base::nix::DESKTOP_ENVIRONMENT_KDE3 ||
      desktop == base::nix::DESKTOP_ENVIRONMENT_KDE4) {
    return SelectFileDialogImpl::NewSelectFileDialogImplKDE(listener);
  }
  return SelectFileDialogImpl::NewSelectFileDialogImplGTK(listener);
}

SelectFileDialogImpl::SelectFileDialogImpl(Listener* listener)
    : SelectFileDialog(listener),
      file_type_index_(0),
      type_(SELECT_NONE) {
  if (!last_saved_path_) {
    last_saved_path_ = new FilePath();
    last_opened_path_ = new FilePath();
  }
}

SelectFileDialogImpl::~SelectFileDialogImpl() { }

void SelectFileDialogImpl::ListenerDestroyed() {
  listener_ = NULL;
}

bool SelectFileDialogImpl::IsRunning(gfx::NativeWindow parent_window) const {
  return parents_.find(parent_window) != parents_.end();
}

bool SelectFileDialogImpl::CallDirectoryExistsOnUIThread(const FilePath& path) {
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  return file_util::DirectoryExists(path);
}

