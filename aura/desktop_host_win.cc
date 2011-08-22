// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/desktop_host_win.h"

#include "aura/desktop.h"
#include "base/message_loop.h"

namespace aura {

// static
DesktopHost* DesktopHost::Create(const gfx::Rect& bounds) {
  return new DesktopHostWin(bounds);
}

DesktopHostWin::DesktopHostWin(const gfx::Rect& bounds) : desktop_(NULL) {
  Init(NULL, bounds);
}

DesktopHostWin::~DesktopHostWin() {
  DestroyWindow(hwnd());
}

bool DesktopHostWin::Dispatch(const MSG& msg) {
  // TODO(ben):
  return true;
}

void DesktopHostWin::SetDesktop(Desktop* desktop) {
  desktop_ = desktop;
}

gfx::AcceleratedWidget DesktopHostWin::GetAcceleratedWidget() {
  return hwnd();
}

void DesktopHostWin::Show() {
  ShowWindow(hwnd(), SW_SHOWNORMAL);
}

gfx::Size DesktopHostWin::GetSize() {
  RECT r;
  GetClientRect(hwnd(), &r);
  return gfx::Rect(r).size();
}

void DesktopHostWin::OnClose() {
  // TODO: this obviously shouldn't be here.
  MessageLoopForUI::current()->Quit();
}

void DesktopHostWin::OnPaint(HDC dc) {
  if (desktop_)
    desktop_->Draw();
  ValidateRect(hwnd(), NULL);
}

}  // namespace aura
