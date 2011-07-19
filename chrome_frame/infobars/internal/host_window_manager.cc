// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/infobars/internal/host_window_manager.h"

#include "chrome_frame/infobars/internal/displaced_window_manager.h"

namespace {

const wchar_t kIeTabContentParentWindowClass[] = L"Shell DocObject View";

}  // namespace

// Receives notification when the displaced window is destroyed, and forwards
// displaced window dimensions on to HostWindowManager::Delegate.
class HostWindowManager::DisplacedWindowDelegate
    : public DisplacedWindowManager::Delegate {
 public:
  explicit DisplacedWindowDelegate(HostWindowManager* manager);
  virtual ~DisplacedWindowDelegate();

  // DisplacedWindowManager::Delegate implementation
  virtual void AdjustDisplacedWindowDimensions(RECT* rect);

 private:
  HostWindowManager* manager_;  // Not owned by this instance
  DISALLOW_COPY_AND_ASSIGN(DisplacedWindowDelegate);
};  // class HostWindowManager::DisplacedWindowDelegate

HostWindowManager::DisplacedWindowDelegate::DisplacedWindowDelegate(
    HostWindowManager* manager) : manager_(manager) {
}

// Called when the displaced window is destroyed. Try to find a new displaced
// window.
HostWindowManager::DisplacedWindowDelegate::~DisplacedWindowDelegate() {
  HWND old_window = *manager_->displaced_window_manager_;
  // Will be deleted in its OnFinalMessage
  manager_->displaced_window_manager_ = NULL;

  // Check to see if a new window has already been created.
  if (manager_->FindDisplacedWindow(old_window))
    manager_->UpdateLayout();
}

// Forward this on to our delegate
void HostWindowManager::DisplacedWindowDelegate::
    AdjustDisplacedWindowDimensions(RECT* rect) {
  manager_->delegate()->AdjustDisplacedWindowDimensions(rect);
}

// Callback function for EnumChildWindows (looks for a window with class
// kIeTabContentParentWindowClass).
//
// lparam must point to an HWND that is either NULL or the HWND of the displaced
// window that is being destroyed. We will ignore that window if we come across
// it, and update lparam to point to the new displaced window if it is found.
static BOOL CALLBACK FindDisplacedWindowProc(HWND hwnd, LPARAM lparam) {
  DCHECK(lparam != NULL);
  HWND* window_handle = reinterpret_cast<HWND*>(lparam);

  if (hwnd == *window_handle)
    return TRUE;  // Skip this, it's the old displaced window.

  // Variable to hold the class name. The size does not matter as long as it
  // is at least can hold kIeTabContentParentWindowClass.
  wchar_t class_name[100];
  if (::GetClassName(hwnd, class_name, arraysize(class_name)) &&
      lstrcmpi(kIeTabContentParentWindowClass, class_name) == 0) {
    // We found the window. Return its handle and stop enumeration.
    *window_handle = hwnd;
    return FALSE;
  }
  return TRUE;
}

HostWindowManager::HostWindowManager() : displaced_window_manager_(NULL) {
}

HostWindowManager::~HostWindowManager() {
  // If we are holding a displaced_window_manager_, it means that
  // OnDisplacedWindowManagerDestroyed has not been called yet, and therefore
  // our DisplacedWindowDelegate might still be around, ready to invoke us.
  // Fail fast to prevent a call into lala-land.
  CHECK(displaced_window_manager_ == NULL);
}

void HostWindowManager::UpdateLayout() {
  if (FindDisplacedWindow(NULL))
    displaced_window_manager_->UpdateLayout();
}

bool HostWindowManager::FindDisplacedWindow(HWND old_window) {
  if (displaced_window_manager_ == NULL ||
      *displaced_window_manager_ == old_window) {
    // Find the window which is the container for the HTML view (parent of
    // the content). When the displaced window is destroyed, the new one might
    // already exist, so we say "find a displaced window that is not this
    // (old) one".
    HWND displaced_window = old_window;
    ::EnumChildWindows(*this, FindDisplacedWindowProc,
                       reinterpret_cast<LPARAM>(&displaced_window));

    if (displaced_window == old_window) {
      LOG(ERROR) << "Failed to locate IE renderer HWND to displace for "
                 << "Infobar installation.";
    } else {
      scoped_ptr<DisplacedWindowManager> displaced_window_manager(
          new DisplacedWindowManager());
      if (displaced_window_manager->Initialize(
              displaced_window, new DisplacedWindowDelegate(this))) {
        displaced_window_manager_ = displaced_window_manager.release();
      }
    }
  }

  return displaced_window_manager_ != NULL;
}
