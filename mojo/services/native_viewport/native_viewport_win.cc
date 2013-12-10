// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/native_viewport/native_viewport.h"

#include "ui/events/event.h"
#include "ui/gfx/win/window_impl.h"

namespace mojo {
namespace services {

class NativeViewportWin : public gfx::WindowImpl,
                          public NativeViewport {
 public:
  explicit NativeViewportWin(NativeViewportDelegate* delegate)
      : delegate_(delegate) {
  }
  virtual ~NativeViewportWin() {
    if (IsWindow(hwnd()))
      DestroyWindow(hwnd());
  }

 private:
  // Overridden from NativeViewport:
  virtual gfx::Size GetSize() OVERRIDE {
    RECT cr;
    GetClientRect(hwnd(), &cr);
    return gfx::Size(cr.right - cr.left, cr.bottom - cr.top);
  }

  virtual void Init() OVERRIDE {
    gfx::WindowImpl::Init(NULL, gfx::Rect(10, 10, 500, 500));
    ShowWindow(hwnd(), SW_SHOWNORMAL);
    SetWindowText(hwnd(), L"native_viewport::NativeViewportWin!");
  }

  virtual void Close() OVERRIDE {
    DestroyWindow(hwnd());
  }

  virtual void SetCapture() OVERRIDE {
    DCHECK(::GetCapture() != hwnd());
    ::SetCapture(hwnd());
  }

  virtual void ReleaseCapture() OVERRIDE {
    if (::GetCapture() == hwnd())
      ::ReleaseCapture();
  }

  BEGIN_MSG_MAP_EX(NativeViewportWin)
    MESSAGE_RANGE_HANDLER_EX(WM_MOUSEFIRST, WM_MOUSELAST, OnMouseRange)

    MSG_WM_CREATE(OnCreate)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_SIZE(OnSize)
    MSG_WM_DESTROY(OnDestroy)
  END_MSG_MAP()

  LRESULT OnMouseRange(UINT message, WPARAM w_param, LPARAM l_param) {
    MSG msg = { hwnd(), message, w_param, l_param, 0,
                { GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param) } };
    ui::MouseEvent event(msg);
    bool handled = delegate_->OnEvent(&event);
    SetMsgHandled(handled);
    return 0;
  }
  LRESULT OnCreate(CREATESTRUCT* create_struct) {
    delegate_->OnAcceleratedWidgetAvailable(hwnd());
    return 0;
  }
  void OnPaint(HDC) {
    RECT cr;
    GetClientRect(hwnd(), &cr);

    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd(), &ps);
    HBRUSH red_brush = CreateSolidBrush(RGB(255, 0, 0));
    HGDIOBJ old_object = SelectObject(dc, red_brush);
    Rectangle(dc, cr.left, cr.top, cr.right, cr.bottom);
    SelectObject(dc, old_object);
    DeleteObject(red_brush);
    EndPaint(hwnd(), &ps);
  }
  void OnSize(UINT param, const CSize& size) {
    delegate_->OnResized(gfx::Size(size.cx, size.cy));
  }
  void OnDestroy() {
    delegate_->OnDestroyed();
  }

  NativeViewportDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(NativeViewportWin);
};

// static
scoped_ptr<NativeViewport> NativeViewport::Create(
    shell::Context* context,
    NativeViewportDelegate* delegate) {
  return scoped_ptr<NativeViewport>(new NativeViewportWin(delegate)).Pass();
}

}  // namespace services
}  // namespace mojo
