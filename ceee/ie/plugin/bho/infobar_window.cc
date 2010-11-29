// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the manager for infobar windows.

#include "ceee/ie/plugin/bho/infobar_window.h"

#include <atlapp.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"

namespace {

const UINT_PTR kInfobarSlidingTimerId = 1U;
// Interval for sliding the infobar in milliseconds.
const UINT kInfobarSlidingTimerIntervalMs = 50U;
// The step when the infobar is sliding, in pixels.
const int kInfobarSlidingStep = 10;
// The default height of the infobar. See also similar constant in
// ceee/ie/plugin/bho/executor.cc which overrides this one.
const int kInfobarDefaultHeight = 39;

}  // namespace


namespace infobar_api {

InfobarWindow* InfobarWindow::CreateInfobar(InfobarType type,
                                            Delegate* delegate,
                                            IEventSender* event_sender) {
  DCHECK(delegate);
  return NULL == delegate ? NULL : new InfobarWindow(type, delegate,
      event_sender);
}

InfobarWindow::InfobarWindow(InfobarType type, Delegate* delegate,
                             IEventSender* event_sender)
    : type_(type),
      delegate_(delegate),
      show_(false),
      target_height_(1),
      current_height_(1),
      sliding_infobar_(false),
      timer_id_(0),
      event_sender_(event_sender) {
  DCHECK(delegate);
}

InfobarWindow::~InfobarWindow() {
  Reset();

  if (IsWindow()) {
    DestroyWindow();
  } else {
    NOTREACHED() << "Infobar window was not successfully created.";
  }
}

void InfobarWindow::OnBrowserWindowClose() {
  // Propagate the event to the manager.
  if (delegate_ != NULL)
    delegate_->OnWindowClose(type_);
}

HRESULT InfobarWindow::Show(int max_height, bool slide) {
  if (url_.empty())
    return E_UNEXPECTED;

  StartUpdatingLayout(true, max_height, slide);
  return S_OK;
}

HRESULT InfobarWindow::Hide() {
  // This could be called on the infobar that was not yet shown if show infobar
  // function is called with an empty URL (see CeeeExecutor::ShowInfobar
  // implementation). Therefore we should check if window exists and do not
  // treat this as an error if not.
  if (IsWindow())
    StartUpdatingLayout(false, 0, false);

  return S_OK;
}

HRESULT InfobarWindow::Navigate(const std::wstring& url) {
  // If the CF exists (which means the infobar has already been created) then
  // navigate it. Otherwise just store the URL, it will be passed to the CF when
  // it will be created.
  url_ = url;
  if (chrome_frame_host_)
    chrome_frame_host_->SetUrl(CComBSTR(url_.c_str()));
  return S_OK;
}

void InfobarWindow::ReserveSpace(RECT* rect) {
  DCHECK(rect);
  if (rect == NULL || !show_)
    return;

  switch (type_) {
    case TOP_INFOBAR:
      rect->top += current_height_;
      if (rect->top > rect->bottom)
        rect->top = rect->bottom;
      break;
    case BOTTOM_INFOBAR:
      rect->bottom -= current_height_;
      if (rect->bottom < rect->top)
        rect->bottom = rect->top;
      break;
    default:
      NOTREACHED() << "Unknown InfobarType value.";
      break;
  }
}

void InfobarWindow::UpdatePosition() {
  // Make infobar be consistent with IE window's size.
  // NOTE: Even if currently it is not visible, we still need to update its
  // position, since the contents may need to decide its layout based on the
  // width of the infobar.

  CRect rect = CalculatePosition();
  if (IsWindow())
    MoveWindow(&rect, TRUE);
}

void InfobarWindow::Reset() {
  Hide();

  if (chrome_frame_host_)
    chrome_frame_host_->Teardown();

  DCHECK(!show_ && !sliding_infobar_);
  if (m_hWnd != NULL) {
    DestroyWindow();
    m_hWnd = NULL;
  }
  url_.clear();
  chrome_frame_host_.Release();
}

void InfobarWindow::StartUpdatingLayout(bool show, int max_height, bool slide) {
  if (!IsWindow()) {
    LOG(ERROR) << "Updating infobar layout when window has not been created";
    return;
  }

  show_ = show;
  if (show) {
    int html_content_height = kInfobarDefaultHeight;
    CSize html_content_size(0, 0);
    if (SUCCEEDED(GetContentSize(&html_content_size)) &&
        html_content_size.cy > 0) {
      html_content_height = html_content_size.cy;
    }
    target_height_ = (max_height == 0 || html_content_height < max_height) ?
        html_content_height : max_height;
    if (target_height_ <= 0) {
      target_height_ = 1;
    }
  } else {
    target_height_ = 1;
  }

  if (!slide || !show) {
    current_height_ = target_height_;

    if (sliding_infobar_) {
      KillTimer(timer_id_);
      sliding_infobar_ = false;
    }
  } else {
    // If the infobar is visible and sliding effect is requested, we need to
    // start expanding/shrinking the infobar according to its current height.
    current_height_ = CalculateNextHeight();

    if (!sliding_infobar_) {
      // Set timer and store its id (it could be different from the passed one).
      timer_id_ = SetTimer(kInfobarSlidingTimerId,
                           kInfobarSlidingTimerIntervalMs, NULL);
      sliding_infobar_ = true;
    }
  }

  UpdateLayout();
}

int InfobarWindow::CalculateNextHeight() {
  if (current_height_ < target_height_) {
    return std::min(current_height_ + kInfobarSlidingStep, target_height_);
  } else if (current_height_ > target_height_) {
    return std::max(current_height_ - kInfobarSlidingStep, target_height_);
  } else {
    return current_height_;
  }
}

RECT InfobarWindow::CalculatePosition() {
  CRect rect(0, 0, 0, 0);

  if (NULL == delegate_)
    return rect;
  HWND container_window = delegate_->GetContainerWindow();
  if (container_window == NULL || !::IsWindow(container_window))
    return rect;
  HWND container_parent_window = ::GetParent(container_window);
  if (!::IsWindow(container_parent_window))
    return rect;

  ::GetWindowRect(container_window, &rect);
  ::MapWindowPoints(NULL, container_parent_window,
                    reinterpret_cast<POINT*>(&rect), 2);

  switch (type_) {
    case TOP_INFOBAR:
      if (rect.top + current_height_ < rect.bottom)
        rect.bottom = rect.top + current_height_;
      break;
    case BOTTOM_INFOBAR:
      if (rect.bottom - current_height_ > rect.top)
        rect.top = rect.bottom - current_height_;
      break;
    default:
      NOTREACHED() << "Unknown InfobarType value.";
      break;
  }
  return rect;
}

void InfobarWindow::UpdateLayout() {
  CRect rect = CalculatePosition();
  if (IsWindow()) {
    // Set infobar's z-order, place it at the top, so that it won't be hidden by
    // IE window.
    SetWindowPos(HWND_TOP, &rect, show_ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
  }

  HWND container_window = NULL;
  if (delegate_ != NULL)
    container_window = delegate_->GetContainerWindow();
  if (container_window != NULL && ::IsWindow(container_window)) {
    // Call SetWindowPos with SWP_FRAMECHANGED for IE window, then IE
    // window would receive WM_NCCALCSIZE to recalculate its client size.
    ::SetWindowPos(container_window,
                   NULL, 0, 0, 0, 0,
                   SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                   SWP_FRAMECHANGED);
  }
}

HRESULT InfobarWindow::GetContentSize(SIZE* size) {
  DCHECK(size);
  if (NULL == size)
    return E_POINTER;

  // Set the size to 0 that means we do not know it.
  // TODO(vadimb@google.com): Find how to get the content size from the CF.
  size->cx = 0;
  size->cy = 0;
  return S_OK;
}

LRESULT InfobarWindow::OnTimer(UINT_PTR nIDEvent) {
  DCHECK(nIDEvent == timer_id_);
  if (show_ && sliding_infobar_ && current_height_ != target_height_) {
    current_height_ = CalculateNextHeight();
    UpdateLayout();
  } else if (sliding_infobar_) {
    KillTimer(timer_id_);
    sliding_infobar_ = false;
  }

  return S_OK;
}

LRESULT InfobarWindow::OnCreate(LPCREATESTRUCT lpCreateStruct) {
  InitializingCoClass<InfobarBrowserWindow>::CreateInitialized(
      CComBSTR(url_.c_str()), this, event_sender_, &chrome_frame_host_);

  if (chrome_frame_host_) {
    chrome_frame_host_->CreateAndShowWindow(m_hWnd);
    AdjustSize();
  }
  return S_OK;
}

void InfobarWindow::OnPaint(CDCHandle dc) {
  RECT rc;
  if (GetUpdateRect(&rc, FALSE)) {
    PAINTSTRUCT ps = {};
    BeginPaint(&ps);

    BOOL ret = GetClientRect(&rc);
    DCHECK(ret);
    FillRect(ps.hdc, &rc, (HBRUSH)GetStockObject(GRAY_BRUSH));
    ::DrawText(ps.hdc, L"Google CEEE. No Chrome Frame found!", -1,
               &rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

    EndPaint(&ps);
  }
}

void InfobarWindow::OnSize(UINT type, CSize size) {
  AdjustSize();
}

void InfobarWindow::AdjustSize() {
  if (NULL != chrome_frame_host_) {
    CRect rect;
    GetClientRect(&rect);
    chrome_frame_host_->SetWindowSize(rect.Width(), rect.Height());
  }
}

}  // namespace infobar_api
