// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_
#define CHROME_BROWSER_UI_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_helpers.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/shell_integration.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "ui/base/gtk/gtk_signal.h"

using content::BrowserThread;

typedef struct _GdkPixbuf GdkPixbuf;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkWindow GtkWindow;

class Extension;
class Profile;
class TabContentsWrapper;

class CreateApplicationShortcutsDialogGtk
    : public base::RefCountedThreadSafe<CreateApplicationShortcutsDialogGtk,
                                        BrowserThread::DeleteOnUIThread> {
 protected:
  explicit CreateApplicationShortcutsDialogGtk(GtkWindow* parent);
  virtual ~CreateApplicationShortcutsDialogGtk();

  CHROMEGTK_CALLBACK_1(CreateApplicationShortcutsDialogGtk, void,
                       OnCreateDialogResponse, int);

  CHROMEGTK_CALLBACK_1(CreateApplicationShortcutsDialogGtk, void,
                       OnErrorDialogResponse, int);

  CHROMEGTK_CALLBACK_0(CreateApplicationShortcutsDialogGtk, void,
                       OnToggleCheckbox);

  virtual void CreateDialogBox(GtkWindow* parent);
  virtual void CreateIconPixBuf(const gfx::Image& image);

  // This method is called after a shortcut is created.
  // Subclasses can override it to take some action at that time.
  virtual void OnCreatedShortcut(void) {}

  virtual void CreateDesktopShortcut(
      const ShellIntegration::ShortcutInfo& shortcut_info);
  virtual void ShowErrorDialog();

  GtkWindow* parent_;

  // UI elements.
  GtkWidget* desktop_checkbox_;
  GtkWidget* menu_checkbox_;

  // ShortcutInfo for the new shortcut.
  ShellIntegration::ShortcutInfo shortcut_info_;

  // Image associated with the site.
  GdkPixbuf* favicon_pixbuf_;

  // Dialog box that allows the user to create an application shortcut.
  GtkWidget* create_dialog_;

  // Dialog box that shows the error message.
  GtkWidget* error_dialog_;

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<CreateApplicationShortcutsDialogGtk>;
  DISALLOW_COPY_AND_ASSIGN(CreateApplicationShortcutsDialogGtk);
};

class CreateWebApplicationShortcutsDialogGtk
    : public CreateApplicationShortcutsDialogGtk {
 public:
  // Displays the dialog box to create application shortcuts for |tab_contents|.
  static void Show(GtkWindow* parent, TabContentsWrapper* tab_contents);

  CreateWebApplicationShortcutsDialogGtk(GtkWindow* parent,
                                         TabContentsWrapper* tab_contents);

  virtual void OnCreatedShortcut(void) OVERRIDE;

 protected:
  virtual ~CreateWebApplicationShortcutsDialogGtk() {}

 private:
  // TabContentsWrapper for which the shortcut will be created.
  TabContentsWrapper* tab_contents_;

  DISALLOW_COPY_AND_ASSIGN(CreateWebApplicationShortcutsDialogGtk);
};

class CreateChromeApplicationShortcutsDialogGtk
  : public CreateApplicationShortcutsDialogGtk,
    public ImageLoadingTracker::Observer {
 public:
  // Displays the dialog box to create application shortcuts for |app|.
  static void Show(GtkWindow* parent, Profile* profile, const Extension* app);

  CreateChromeApplicationShortcutsDialogGtk(GtkWindow* parent,
                                            Profile* profile,
                                            const Extension* app);

  // Implement ImageLoadingTracker::Observer.  |tracker_| is used to
  // load the app's icon.  This method recieves the icon, and adds
  // it to the "Create Shortcut" dailog box.
  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE;

 protected:
  virtual ~CreateChromeApplicationShortcutsDialogGtk() {}

  virtual void CreateDesktopShortcut(
      const ShellIntegration::ShortcutInfo& shortcut_info) OVERRIDE;

 private:
  const Extension* app_;
  FilePath profile_path_;
  ImageLoadingTracker tracker_;
  DISALLOW_COPY_AND_ASSIGN(CreateChromeApplicationShortcutsDialogGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_CREATE_APPLICATION_SHORTCUTS_DIALOG_GTK_H_
