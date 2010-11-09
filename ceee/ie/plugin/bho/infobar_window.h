// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// @file
// Infobar window.

#ifndef CEEE_IE_PLUGIN_BHO_INFOBAR_WINDOW_H_
#define CEEE_IE_PLUGIN_BHO_INFOBAR_WINDOW_H_

#include <atlbase.h>
#include <atlapp.h>  // Must be included AFTER base.
#include <atlcrack.h>
#include <atlgdi.h>
#include <atlmisc.h>
#include <atlwin.h>

#include "base/singleton.h"
#include "base/scoped_ptr.h"
#include "ceee/ie/plugin/bho/infobar_browser_window.h"

namespace infobar_api {

enum InfobarType {
  FIRST_INFOBAR_TYPE = 0,
  TOP_INFOBAR = 0,          // Infobar at the top.
  BOTTOM_INFOBAR = 1,       // Infobar at the bottom.
  END_OF_INFOBAR_TYPE = 2
};

// InfobarWindow is the window created on the top or bottom of the browser tab
// window that contains the web browser window.
class InfobarWindow : public InfobarBrowserWindow::Delegate,
                      public CWindowImpl<InfobarWindow, CWindow> {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    // Returns the window handle for the HTML container window.
    virtual HWND GetContainerWindow() = 0;
    // Informs about window.close() event.
    virtual void OnWindowClose(InfobarType type) = 0;
  };

  static InfobarWindow* CreateInfobar(InfobarType type, Delegate* delegate);
  ~InfobarWindow();

  // Implementation of InfobarBrowserWindow::Delegate.
  // Informs about window.close() event.
  virtual void OnBrowserWindowClose();

  // Shows the infobar.
  // NOTE: Navigate should be called before Show.
  // The height of the infobar is calculated to fit the content (limited to
  // max_height if the content is too high; no limit if max_height is set to
  // 0).
  // slide indicates whether to show sliding effect.
  HRESULT Show(int max_height, bool slide);

  // Hides the infobar.
  HRESULT Hide();

  // Navigates the HTML view of the infobar.
  HRESULT Navigate(const std::wstring& url);

  // Reserves space for the infobar when IE window recalculates its size.
  void ReserveSpace(RECT* rect);

  // Updates the infobar size and position when IE content window size or
  // position is changed.
  void UpdatePosition();

  // Destroys the browser window.
  void Reset();

 private:
  BEGIN_MSG_MAP(InfobarWindow)
    MSG_WM_TIMER(OnTimer);
    MSG_WM_CREATE(OnCreate)
    MSG_WM_PAINT(OnPaint)
    MSG_WM_SIZE(OnSize)
  END_MSG_MAP()

  // Type of the infobar - whether it is displayed at the top or at the bottom
  // of the IE content window.
  InfobarType type_;

  // Delegate, connection to the infobar manager.
  Delegate* delegate_;

  // URL to navigate to.
  std::wstring url_;

  // Whether the infobar is shown or not.
  bool show_;

  // The target height of the infobar.
  int target_height_;

  // The current height of the infobar.
  int current_height_;

  // Indicates whether the infobar is sliding.
  bool sliding_infobar_;

  // The Chrome Frame host handling a Chrome Frame instance for us.
  CComPtr<IInfobarBrowserWindow> chrome_frame_host_;

  // Constructor.
  InfobarWindow(InfobarType type, Delegate* delegate);

  // If show is true, shrinks IE content window and shows the infobar
  // either at the top or at the bottom. Otherwise, hides the infobar and
  // restores IE content window.
  void StartUpdatingLayout(bool show, int max_height, bool slide);

  // Based on the current height and the target height, decides the height of
  // the next step. This is used when showing sliding effect.
  int CalculateNextHeight();

  // Calculates the position of the infobar based on its current height.
  RECT CalculatePosition();

  // Updates the layout (sizes and positions of infobar and IE content window)
  // based on the current height.
  void UpdateLayout();

  HRESULT GetContentSize(SIZE* size);

  // Event handlers.
  LRESULT OnTimer(UINT_PTR nIDEvent);
  LRESULT OnCreate(LPCREATESTRUCT lpCreateStruct);
  void OnPaint(CDCHandle dc);
  void OnSize(UINT type, CSize size);

  void AdjustSize();

  DISALLOW_COPY_AND_ASSIGN(InfobarWindow);
};

}  // namespace infobar_api

#endif  // CEEE_IE_PLUGIN_BHO_INFOBAR_WINDOW_H_
