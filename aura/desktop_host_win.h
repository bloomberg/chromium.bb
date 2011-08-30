// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_DESKTOP_HOST_WIN_H_
#define AURA_DESKTOP_HOST_WIN_H_
#pragma once

#include "aura/desktop_host.h"
#include "base/compiler_specific.h"
#include "ui/base/win/window_impl.h"

namespace aura {

class DesktopHostWin : public DesktopHost, public ui::WindowImpl {
 public:
  explicit DesktopHostWin(const gfx::Rect& bounds);
  virtual ~DesktopHostWin();

  // MessageLoop::Dispatcher:
  virtual bool Dispatch(const MSG& msg);

  // DesktopHost:
  virtual void SetDesktop(Desktop* desktop) OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;

 private:
  BEGIN_MSG_MAP_EX(DesktopHostWin)
    // Range handlers must go first!
    MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)
    MESSAGE_RANGE_HANDLER_EX(WM_NCMOUSEMOVE, WM_NCXBUTTONDBLCLK, OnMouseRange)

    MSG_WM_CLOSE(OnClose)
    MSG_WM_PAINT(OnPaint)
  END_MSG_MAP()

  void OnClose();
  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param);
  void OnPaint(HDC dc);

  Desktop* desktop_;

  DISALLOW_COPY_AND_ASSIGN(DesktopHostWin);
};

}  // namespace aura

#endif  // AURA_DESKTOP_HOST_WIN_H_
