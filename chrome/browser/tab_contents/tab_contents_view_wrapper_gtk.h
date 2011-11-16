// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_WRAPPER_GTK_H_
#define CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_WRAPPER_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "ui/gfx/native_widget_types.h"

class TabContentsViewGtk;
struct ContextMenuParams;

// An object that supplies the Gtk parent of TabContentsViewGtk. Embedders may
// want to insert widgets that provide features that live with the
// TabContentsViewGtk.
class TabContentsViewWrapperGtk {
 public:
  // Initializes the TabContentsViewGtkWrapper by taking |view| and adding it
  // this object's GtkContainer.
  virtual void WrapView(TabContentsViewGtk* view) = 0;

  // Returns the top widget that contains |view| passed in from WrapView. This
  // is exposed through TabContentsViewGtk::GetNativeView() when a wrapper is
  // supplied to a TabContentsViewGtk.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Called during the TabContentsViewGtk. Used to associate drag handlers.
  virtual void OnCreateViewForWidget() = 0;

  // Handles a focus event from the renderer process.
  virtual void Focus() = 0;

  // Gives TabContentsViewGtkWrapper a first chance at focus events from our
  // render widget host, before the main view invokes its default
  // behaviour. Returns TRUE if |return_value| has been set and that value
  // should be returned to GTK+.
  virtual gboolean OnNativeViewFocusEvent(GtkWidget* widget,
                                          GtkDirectionType type,
                                          gboolean* return_value) = 0;

  // Complete hack because I have no idea where else to put this platform
  // specific crud.
  virtual void ShowContextMenu(const ContextMenuParams& params) = 0;

  virtual ~TabContentsViewWrapperGtk() {}
};

#endif  // CHROME_BROWSER_TAB_CONTENTS_TAB_CONTENTS_VIEW_WRAPPER_GTK_H_
