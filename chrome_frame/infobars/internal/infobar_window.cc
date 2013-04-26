// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of the manager for infobar windows.

#include "chrome_frame/infobars/internal/infobar_window.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "chrome_frame/function_stub.h"

namespace {

// length of each step when opening or closing
const UINT kInfobarSlidingTimerIntervalMs = 50U;
// pixels per step, when opening or closing
const int kInfobarSlideOpenStep = 2;
const int kInfobarSlideCloseStep = 6;

}  // namespace

VOID CALLBACK OnSliderTimer(InfobarWindow::Host* host,
                            HWND /*hwnd*/, UINT /*uMsg*/,
                            UINT_PTR /*idEvent*/, DWORD /*dwTime*/) {
  if (host)
    host->UpdateLayout();
}

InfobarWindow::InfobarWindow(InfobarType type)
    : type_(type),
      host_(NULL),
      target_height_(0),
      initial_height_(0),
      current_height_(0),
      current_width_(0),
      timer_id_(0),
      timer_stub_(NULL),
      frame_impl_(this) {
  DCHECK(type_ >= FIRST_INFOBAR_TYPE);
  DCHECK(type_ < END_OF_INFOBAR_TYPE);
}

InfobarWindow::~InfobarWindow() {
  if (StopTimer() && timer_stub_ != NULL)
    FunctionStub::Destroy(timer_stub_);
  else if (timer_stub_ != NULL)
    timer_stub_->set_argument(NULL);  // Couldn't stop it, so orphan and disable
}

void InfobarWindow::SetHost(Host* host) {
  DCHECK(host_ == NULL);
  DCHECK(timer_stub_ == NULL);
  DCHECK(host != NULL);
  host_ = host;
  timer_stub_ = FunctionStub::Create(reinterpret_cast<uintptr_t>(host),
                                     OnSliderTimer);
}

bool InfobarWindow::Show(InfobarContent* content) {
  DCHECK(host_ != NULL);
  if (host_ == NULL)
    return false;

  scoped_ptr<InfobarContent> new_content(content);
  content_.reset();

  if (!new_content->InstallInFrame(&frame_impl_))
    return false;

  // Force a call to ReserveSpace, which will capture the width of the displaced
  // window.
  if (current_width_ == 0)
    host_->UpdateLayout();
  if (current_width_ == 0)
    return false;  // Might not be any displaced window.. then we can't display.

  content_.swap(new_content);
  StartSlidingTowards(content_->GetDesiredSize(current_width_, 0));

  return true;
}

void InfobarWindow::Hide() {
  DCHECK(host_ != NULL);
  if (host_ == NULL)
    return;

  StartSlidingTowards(0);
}

void InfobarWindow::ReserveSpace(RECT* rect) {
  DCHECK(rect);
  DCHECK(host_ != NULL);
  if (rect == NULL || host_ == NULL)
    return;

  current_width_ = rect->right - rect->left;
  current_height_ = CalculateHeight();

  RECT infobar_rect = *rect;

  switch (type_) {
    case TOP_INFOBAR:
      infobar_rect.bottom = rect->top + current_height_;
      rect->top = std::min(rect->bottom, infobar_rect.bottom);
      break;
    case BOTTOM_INFOBAR:
      infobar_rect.top = rect->bottom - current_height_;
      rect->bottom = std::max(rect->top, infobar_rect.top);
      break;
    default:
      NOTREACHED() << "Unknown InfobarType value.";
      break;
  }

  if (content_ != NULL)
    content_->SetDimensions(infobar_rect);

  // Done sliding?
  if (current_height_ == target_height_) {
    StopTimer();
    if (current_height_ == 0)
      content_.reset();
  }
}

void InfobarWindow::StartSlidingTowards(int target_height) {
  initial_height_ = current_height_;
  target_height_ = target_height;

  if (StartTimer())
    slide_start_ = base::Time::Now();
  else
    slide_start_ = base::Time();  // NULL time means don't slide, resize now

  // Trigger an immediate re-laying out. The timer will handle remaining steps.
  host_->UpdateLayout();
}

bool InfobarWindow::StartTimer() {
  if (timer_id_ != 0)
    return true;

  DCHECK(timer_stub_ != NULL);
  if (timer_stub_ == NULL)
    return false;

  timer_id_ = ::SetTimer(NULL,
                         timer_id_,
                         kInfobarSlidingTimerIntervalMs,
                         reinterpret_cast<TIMERPROC>(timer_stub_->code()));

  DPLOG_IF(ERROR, timer_id_ == 0) << "Failure in SetTimer.";

  return timer_id_ != 0;
}

bool InfobarWindow::StopTimer() {
  if (timer_id_ == 0)
    return true;

  if (::KillTimer(NULL, timer_id_)) {
    timer_id_ = 0;
    return true;
  }

  DPLOG(ERROR) << "Failure in KillTimer.";
  return false;
}

int InfobarWindow::CalculateHeight() {
  if (slide_start_.is_null())
    return target_height_;

  base::TimeDelta elapsed = base::Time::Now() - slide_start_;
  int elapsed_steps = static_cast<int>(elapsed.InMilliseconds()) /
                      kInfobarSlidingTimerIntervalMs;

  if (initial_height_ < target_height_) {
    return std::min(initial_height_ + elapsed_steps * kInfobarSlideOpenStep,
                    target_height_);
  } else if (initial_height_ > target_height_) {
    return std::max(initial_height_ - elapsed_steps * kInfobarSlideCloseStep,
                    target_height_);
  } else {
    return target_height_;
  }
}

InfobarWindow::FrameImpl::FrameImpl(InfobarWindow* infobar_window)
    : infobar_window_(infobar_window) {
}

HWND InfobarWindow::FrameImpl::GetFrameWindow() {
  return infobar_window_->host_->GetContainerWindow();
}

void InfobarWindow::FrameImpl::CloseInfobar() {
  infobar_window_->Hide();
}
