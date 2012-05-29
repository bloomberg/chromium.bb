// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_GTK_H_
#define CHROME_BROWSER_UI_GTK_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "chrome/browser/ui/extensions/web_auth_flow_window.h"
#include "ui/base/gtk/gtk_signal.h"

namespace content {
class BrowserContext;
class WebContents;
}

class WebAuthFlowWindowGtk : public WebAuthFlowWindow {
 public:
  WebAuthFlowWindowGtk(Delegate* delegate,
                       content::BrowserContext* browser_context,
                       content::WebContents* contents);

  // WebAuthFlowWindow implementation.
  virtual void Show() OVERRIDE;

 private:
  virtual ~WebAuthFlowWindowGtk();

  CHROMEGTK_CALLBACK_1(WebAuthFlowWindowGtk, gboolean, OnMainWindowDeleteEvent,
                       GdkEvent*);

  GtkWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthFlowWindowGtk);
};

#endif  // CHROME_BROWSER_UI_GTK_EXTENSIONS_WEB_AUTH_FLOW_WINDOW_GTK_H_
