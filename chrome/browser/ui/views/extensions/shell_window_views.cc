// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/shell_window_views.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_host.h"
#include "chrome/common/extensions/extension.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/web_applications/web_app.h"
#include "ui/base/win/shell.h"
#endif // OS_WIN

ShellWindowViews::ShellWindowViews(ExtensionHost* host)
    : ShellWindow(host) {
  host_->view()->SetContainer(this);
  window_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = this;
  gfx::Rect bounds(0, 0, 512, 384);
  params.bounds = bounds;
  window_->Init(params);
#if defined(OS_WIN)
  std::string app_name = web_app::GenerateApplicationNameFromExtensionId(
      host_->extension()->id());
  ui::win::SetAppIdForWindow(
      ShellIntegration::GetAppId(UTF8ToWide(app_name),
      host_->profile()->GetPath()),
      GetWidget()->GetTopLevelWidget()->GetNativeWindow());
#endif // OS_WIN
  window_->Show();
}

ShellWindowViews::~ShellWindowViews() {
}

void ShellWindowViews::Close() {
  window_->Close();
}

void ShellWindowViews::DeleteDelegate() {
  delete this;
}

bool ShellWindowViews::CanResize() const {
  return true;
}

views::View* ShellWindowViews::GetContentsView() {
  return host_->view();
}

string16 ShellWindowViews::GetWindowTitle() const {
  return UTF8ToUTF16(host_->extension()->name());
}

views::Widget* ShellWindowViews::GetWidget() {
  return window_;
}

const views::Widget* ShellWindowViews::GetWidget() const {
  return window_;
}

// static
ShellWindow* ShellWindow::CreateShellWindow(ExtensionHost* host) {
  return new ShellWindowViews(host);
}
