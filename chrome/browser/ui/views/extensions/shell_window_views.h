// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
#pragma once

#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/views/extensions/extension_view.h"
#include "ui/views/widget/widget_delegate.h"

class ExtensionHost;

class ShellWindowViews : public ShellWindow,
                         public ExtensionView::Container,
                         public views::WidgetDelegate {
 public:
  explicit ShellWindowViews(ExtensionHost* host);

  // ShellWindow implementation.
  virtual void Close() OVERRIDE;

  // WidgetDelegate implementation.
  virtual views::View* GetContentsView() OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

 private:
  virtual ~ShellWindowViews();

  views::Widget* window_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_VIEWS_H_
