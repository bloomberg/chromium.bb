// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_CONSTRAINED_HTML_DIALOG_WIN_H_
#define CHROME_BROWSER_VIEWS_CONSTRAINED_HTML_DIALOG_WIN_H_
#pragma once

#include <string>

#include "chrome/browser/dom_ui/constrained_html_dialog.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"
#include "chrome/browser/renderer_host/render_view_host_delegate.h"
#include "chrome/browser/renderer_host/site_instance.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/renderer_preferences.h"
#include "views/controls/native/native_view_host.h"
#include "views/window/window_delegate.h"

class RenderViewHost;
class RenderWidgetHostViewWin;

class ConstrainedHtmlDialogWin : public ConstrainedHtmlDialog,
                                 public ConstrainedWindowDelegate,
                                 public RenderViewHostDelegate {
 public:
  ConstrainedHtmlDialogWin(Profile* profile,
                           HtmlDialogUIDelegate* delegate);
  ~ConstrainedHtmlDialogWin();

  // Called when the dialog is actually being added to the views hierarchy.
  void Init(gfx::NativeView parent_window);

  // ConstrainedHtmlDialog override.
  virtual ConstrainedWindowDelegate* GetConstrainedWindowDelegate();

  // ConstrainedWindowDelegate (aka views::WindowDelegate) override.
  virtual bool CanResize() const;
  virtual views::View* GetContentsView();
  virtual void WindowClosing();

  // RenderViewHostDelegate overrides.
  virtual int GetBrowserWindowID() const;
  virtual ViewType::Type GetRenderViewType() const;
  virtual RendererPreferences GetRendererPrefs(Profile* profile) const;
  virtual void ProcessDOMUIMessage(
      const ViewHostMsg_DomMessage_Params& params);
  virtual void UpdateInspectorSetting(const std::string& key,
                                      const std::string& value) {}
  virtual void ClearInspectorSettings() {}

 private:
  SiteInstance* site_instance_;
  scoped_ptr<RenderViewHost> render_view_host_;

  // URL to be displayed in the dialog.
  GURL dialog_url_;

  // Pointer owned by the |render_view_host_| object.
  RenderWidgetHostViewWin* render_widget_host_view_;

  // View pointer owned by the views hierarchy.
  views::NativeViewHost* native_view_host_;
};

#endif  // CHROME_BROWSER_VIEWS_CONSTRAINED_HTML_DIALOG_WIN_H_
