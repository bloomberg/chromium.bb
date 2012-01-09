// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSIONS_SHELL_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSIONS_SHELL_WINDOW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/gtk/extensions/extension_view_gtk.h"
#include "ui/base/gtk/gtk_signal.h"

class ExtensionHost;

class ShellWindowGtk : public ShellWindow,
                       public ExtensionViewGtk::Container {
 public:
  explicit ShellWindowGtk(ExtensionHost* host);

  // ShellWindow implementation.
  virtual void Close() OVERRIDE;

 private:
  virtual ~ShellWindowGtk();

  CHROMEGTK_CALLBACK_1(ShellWindowGtk, gboolean, OnMainWindowDeleteEvent,
                       GdkEvent*);

  GtkWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSIONS_SHELL_WINDOW_GTK_H_
