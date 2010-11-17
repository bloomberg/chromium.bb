// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the manager for infobar windows.

#include "ceee/ie/plugin/bho/infobar_manager.h"

#include <atlbase.h>
#include <atlapp.h>  // Must be included AFTER base.
#include <atlcrack.h>
#include <atlmisc.h>
#include <atlwin.h>

#include "base/logging.h"
#include "ceee/common/windows_constants.h"
#include "chrome_frame/utils.h"

namespace {

enum ContainerWindowUserMessages {
  TM_NOTIFY_UPDATE_POSITION = WM_USER + 600,
  TM_DELAYED_CLOSE_INFOBAR = (TM_NOTIFY_UPDATE_POSITION + 1),
};

}  // namespace

namespace infobar_api {

// ContainerWindow subclasses IE content window's container window, handles
// WM_NCCALCSIZE to resize its client area. It also handles WM_SIZE and WM_MOVE
// messages to make infobars consistent with IE content window's size and
// position.
class InfobarManager::ContainerWindow
    : public InfobarManager::ContainerWindowInterface,
      public CWindowImpl<ContainerWindow> {
 public:
  ContainerWindow(HWND container, InfobarManager* manager)
      : infobar_manager_(manager) {
    PinModule();
    destroyed_ = !::IsWindow(container) || !SubclassWindow(container);
  }

  virtual ~ContainerWindow() {
    if (!destroyed_)
      UnsubclassWindow();
  }

  virtual bool IsDestroyed() const {
    return destroyed_;
  }

  virtual HWND GetWindowHandle() const {
    return IsWindow() ? m_hWnd : NULL;
  }

  virtual bool PostWindowsMessage(UINT msg, WPARAM wparam, LPARAM lparam) {
    return PostMessage(msg, wparam, lparam) != 0;
  }

  BEGIN_MSG_MAP_EX(ContainerWindow)
    MSG_WM_NCCALCSIZE(OnNcCalcSize)
    MSG_WM_SIZE(OnSize)
    MSG_WM_MOVE(OnMove)
    MSG_WM_DESTROY(OnDestroy)
    MESSAGE_HANDLER(TM_NOTIFY_UPDATE_POSITION, OnNotifyUpdatePosition)
    MESSAGE_HANDLER(TM_DELAYED_CLOSE_INFOBAR, OnDelayedCloseInfobar)
  END_MSG_MAP()

 private:
  // Handles WM_NCCALCSIZE message.
  LRESULT OnNcCalcSize(BOOL calc_valid_rects, LPARAM lparam) {
    // Adjust client area for infobar.
    LRESULT ret = DefWindowProc(WM_NCCALCSIZE,
                                static_cast<WPARAM>(calc_valid_rects), lparam);
    // Whether calc_valid_rects is true or false, we could treat beginning of
    // lparam as a RECT object.
    RECT* rect = reinterpret_cast<RECT*>(lparam);
    if (infobar_manager_ != NULL)
      infobar_manager_->OnContainerWindowNcCalcSize(rect);

    // If infobars reserve all the space and rect becomes empty, the container
    // window won't receive subsequent WM_SIZE and WM_MOVE messages.
    // In this case, we have to explicitly notify infobars to update their
    // position.
    if (rect->right - rect->left <= 0 || rect->bottom - rect->top <= 0)
      PostMessage(TM_NOTIFY_UPDATE_POSITION, 0, 0);
    return ret;
  }

  // Handles WM_SIZE message.
  void OnSize(UINT type, CSize size) {
    DefWindowProc(WM_SIZE, static_cast<WPARAM>(type),
                  MAKELPARAM(size.cx, size.cy));
    if (infobar_manager_ != NULL)
      infobar_manager_->OnContainerWindowUpdatePosition();
  }

  // Handles WM_MOVE message.
  void OnMove(CPoint point) {
    if (infobar_manager_ != NULL)
      infobar_manager_->OnContainerWindowUpdatePosition();
  }

  // Handles WM_DESTROY message.
  void OnDestroy() {
    // When refreshing IE window, this window may be destroyed.
    if (infobar_manager_ != NULL)
      infobar_manager_->OnContainerWindowDestroy();
    if (m_hWnd && IsWindow())
      UnsubclassWindow();
    destroyed_ = true;
  }

  // Handles TM_NOTIFY_UPDATE_POSITION message - delayed window resize message.
  LRESULT OnNotifyUpdatePosition(UINT message, WPARAM wparam, LPARAM lparam,
                                 BOOL& handled) {
    if (infobar_manager_ != NULL)
      infobar_manager_->OnContainerWindowUpdatePosition();

    handled = TRUE;
    return 0;
  }

  // Handles TM_DELAYED_CLOSE_INFOBAR - delayed infobar window close request.
  LRESULT OnDelayedCloseInfobar(UINT message, WPARAM wparam, LPARAM lparam,
                                 BOOL& handled) {
    if (infobar_manager_ != NULL) {
      InfobarType type = static_cast<InfobarType>(wparam);
      infobar_manager_->OnContainerWindowDelayedCloseInfobar(type);
    }

    handled = TRUE;
    return 0;
  }

  // Pointer to infobar manager. This object is not owned by the class instance.
  InfobarManager* infobar_manager_;

  // True is this window was destroyed or not subclassed.
  bool destroyed_;

  DISALLOW_COPY_AND_ASSIGN(ContainerWindow);
};

InfobarManager::InfobarManager(HWND tab_window)
    : tab_window_(tab_window) {
}

HRESULT InfobarManager::Show(InfobarType type, int max_height,
                             const std::wstring& url, bool slide) {
  LazyInitialize(type);
  if (type < FIRST_INFOBAR_TYPE || type >= END_OF_INFOBAR_TYPE ||
      infobars_[type] == NULL) {
    return E_INVALIDARG;
  }
  // Set the URL. If the window is not created it will navigate there as soon as
  // it is created.
  infobars_[type]->Navigate(url);
  // Create the window if not created.
  if (!infobars_[type]->IsWindow()) {
    infobars_[type]->InternalCreate(tab_window_, WS_CHILD | WS_CLIPCHILDREN);
  }
  if (!infobars_[type]->IsWindow())
    return E_UNEXPECTED;

  HRESULT hr = infobars_[type]->Show(max_height, slide);
  return hr;
}

HRESULT InfobarManager::Hide(InfobarType type) {
  if (type < FIRST_INFOBAR_TYPE || type >= END_OF_INFOBAR_TYPE)
    return E_INVALIDARG;
  // No lazy initialization here - if the infobar has not been created just
  // return;
  if (infobars_[type] == NULL)
    return E_UNEXPECTED;
  // There is a choice either to hide or to destroy the infobar window.
  // This implementation destroys the infobar to save resources and stop all
  // scripts that possibly still run in the window. If we want to just hide the
  // infobar window instead then we should change Reset to Hide here, possibly
  // navigate the infobar window to "about:blank" and make sure that the code
  // in Show() does not try to create the chrome frame window again.
  infobars_[type]->Reset();
  return S_OK;
}

void InfobarManager::HideAll() {
  for (int index = 0; index < END_OF_INFOBAR_TYPE; ++index)
    Hide(static_cast<InfobarType>(index));
}

// Callback function for EnumChildWindows. lParam should be the pointer to
// HWND variable where the handle of the window which class is
// kIeTabContentParentWindowClass will be written.
static BOOL CALLBACK FindContentParentWindowsProc(HWND hwnd, LPARAM lparam) {
  HWND* window_handle = reinterpret_cast<HWND*>(lparam);
  if (NULL == window_handle) {
    // It makes no sense to continue enumeration.
    return FALSE;
  }

  // Variable to hold the class name. The size does not matter as long as it
  // is at least can hold kIeTabContentParentWindowClass.
  wchar_t class_name[100];
  if (::GetClassName(hwnd, class_name, arraysize(class_name)) &&
      lstrcmpi(windows::kIeTabContentParentWindowClass, class_name) == 0) {
    // We found the window. Return its handle and stop enumeration.
    *window_handle = hwnd;
    return FALSE;
  }
  return TRUE;
}

HWND InfobarManager::GetContainerWindow() {
  if (container_window_ != NULL && container_window_->IsDestroyed())
    container_window_.reset(NULL);

  if (container_window_ == NULL) {
    if (tab_window_ != NULL && ::IsWindow(tab_window_)) {
      // Find the window which is the container for the HTML view (parent of
      // the content).
      HWND content_parent_window = NULL;
      ::EnumChildWindows(tab_window_, FindContentParentWindowsProc,
                         reinterpret_cast<LPARAM>(&content_parent_window));
      DCHECK(content_parent_window);
      if (content_parent_window != NULL) {
        container_window_.reset(
            CreateContainerWindow(content_parent_window, this));
      }
    }
  }
  DCHECK(container_window_ != NULL &&
         container_window_->GetWindowHandle() != NULL);
  return container_window_->GetWindowHandle();
}

void InfobarManager::OnWindowClose(InfobarType type) {
  // This callback is called from CF callback so we should not destroy the
  // infobar window right away as it may result on deleting the object that
  // started this callback. So instead we post ourtselves the message.
  if (container_window_ != NULL)
    container_window_->PostWindowsMessage(TM_DELAYED_CLOSE_INFOBAR,
                                          static_cast<WPARAM>(type), 0);
}

void InfobarManager::OnContainerWindowNcCalcSize(RECT* rect) {
  if (rect == NULL)
    return;

  for (int index = 0; index < END_OF_INFOBAR_TYPE; ++index) {
    if (infobars_[index] != NULL)
      infobars_[index]->ReserveSpace(rect);
  }
}

void InfobarManager::OnContainerWindowUpdatePosition() {
  for (int index = 0; index < END_OF_INFOBAR_TYPE; ++index) {
    if (infobars_[index] != NULL)
      infobars_[index]->UpdatePosition();
  }
}

void InfobarManager::OnContainerWindowDelayedCloseInfobar(InfobarType type) {
  // Hide the infobar window. Parameter validation is handled in Hide().
  Hide(type);
}

void InfobarManager::OnContainerWindowDestroy() {
  for (int index = 0; index < END_OF_INFOBAR_TYPE; ++index) {
    if (infobars_[index] != NULL)
      infobars_[index]->Reset();
  }
}

void InfobarManager::LazyInitialize(InfobarType type) {
  DCHECK(type >= FIRST_INFOBAR_TYPE && type < END_OF_INFOBAR_TYPE);
  if (type < FIRST_INFOBAR_TYPE || type >= END_OF_INFOBAR_TYPE)
    return;

  if (infobars_[type] != NULL)
    return;

  // Note that when InfobarManager is being initialized the IE has not created
  // the tab. Therefore we cannot find the container window here and have to
  // pass interface for a function that finds windows to be called later.
  infobars_[type].reset(InfobarWindow::CreateInfobar(type, this));
}

InfobarManager::ContainerWindowInterface* InfobarManager::CreateContainerWindow(
    HWND container, InfobarManager* manager) {
  return new ContainerWindow(container, manager);
}

}  // namespace infobar_api
