// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/dock_info.h"

#include "chrome/browser/ui/host_desktop.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/x/x11_util.h"

#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_root_window_host_x11.h"
#endif

#if !defined(OS_CHROMEOS)

namespace {

////////////////////////////////////////////////////////////////////////////////
// BaseWindowFinder
//
// Base class used to locate a window. A subclass need only override
// ShouldStopIterating to determine when iteration should stop.
class BaseWindowFinder : public ui::EnumerateWindowsDelegate {
 public:
  explicit BaseWindowFinder(const std::set<aura::Window*>& ignore) {
    std::set<aura::Window*>::iterator iter;
    for (iter = ignore.begin(); iter != ignore.end(); iter++) {
      XID xid = (*iter)->GetRootWindow()->GetAcceleratedWidget();
      ignore_.insert(xid);
    }
  }

  virtual ~BaseWindowFinder() {}

 protected:
  // Returns true if |window| is in the ignore list.
  bool ShouldIgnoreWindow(XID window) {
    return (ignore_.find(window) != ignore_.end());
  }

  // Returns true if iteration should stop, false otherwise.
  virtual bool ShouldStopIterating(XID window) {
    return false;
  }

 private:
  std::set<XID> ignore_;

  DISALLOW_COPY_AND_ASSIGN(BaseWindowFinder);
};

////////////////////////////////////////////////////////////////////////////////
// TopMostFinder
//
// Helper class to determine if a particular point of a window is not obscured
// by another window.
class TopMostFinder : public BaseWindowFinder {
 public:
  // Returns true if |window| is not obscured by another window at the
  // location |screen_loc|, not including the windows in |ignore|.
  static bool IsTopMostWindowAtPoint(XID window,
                                     const gfx::Point& screen_loc,
                                     const std::set<aura::Window*>& ignore) {
    TopMostFinder finder(window, screen_loc, ignore);
    return finder.is_top_most_;
  }

 protected:
  virtual bool ShouldStopIterating(XID window) {
    if (BaseWindowFinder::ShouldIgnoreWindow(window))
      return false;

    if (window == target_) {
      // Window is topmost, stop iterating.
      is_top_most_ = true;
      return true;
    }

    if (!ui::IsWindowVisible(window)) {
      // The window isn't visible, keep iterating.
      return false;
    }

    // At this point we haven't found our target window, so this window is
    // higher in the z-order than the target window.  If this window contains
    // the point, then we can stop the search now because this window is
    // obscuring the target window at this point.
    return ui::WindowContainsPoint(window, screen_loc_);
  }

 private:
  TopMostFinder(XID window,
                const gfx::Point& screen_loc,
                const std::set<aura::Window*>& ignore)
    : BaseWindowFinder(ignore),
      target_(window),
      screen_loc_(screen_loc),
      is_top_most_(false) {
    ui::EnumerateTopLevelWindows(this);
  }

  // The window we're looking for.
  XID target_;

  // Location of window to find.
  gfx::Point screen_loc_;

  // Is target_ the top most window? This is initially false but set to true
  // in ShouldStopIterating if target_ is passed in.
  bool is_top_most_;

  DISALLOW_COPY_AND_ASSIGN(TopMostFinder);
};

////////////////////////////////////////////////////////////////////////////////
// LocalProcessWindowFinder
//
// Helper class to determine if a particular point of a window from our process
// is not obscured by another window.
class LocalProcessWindowFinder : public BaseWindowFinder {
 public:
  // Returns the XID from our process at screen_loc that is not obscured by
  // another window. Returns 0 otherwise.
  static XID GetProcessWindowAtPoint(const gfx::Point& screen_loc,
                                     const std::set<aura::Window*>& ignore) {
    LocalProcessWindowFinder finder(screen_loc, ignore);
    if (finder.result_ &&
        TopMostFinder::IsTopMostWindowAtPoint(finder.result_, screen_loc,
                                              ignore)) {
      return finder.result_;
    }
    return 0;
  }

 protected:
  virtual bool ShouldStopIterating(XID window) {
    if (BaseWindowFinder::ShouldIgnoreWindow(window))
      return false;

    // Check if this window is in our process.
    if (!aura::RootWindow::GetForAcceleratedWidget(window))
      return false;

    if (!ui::IsWindowVisible(window))
      return false;

    if (ui::WindowContainsPoint(window, screen_loc_)) {
      result_ = window;
      return true;
    }

    return false;
  }

 private:
  LocalProcessWindowFinder(const gfx::Point& screen_loc,
                           const std::set<aura::Window*>& ignore)
    : BaseWindowFinder(ignore),
      screen_loc_(screen_loc),
      result_(0) {
    ui::EnumerateTopLevelWindows(this);
  }

  // Position of the mouse.
  gfx::Point screen_loc_;

  // The resulting window. This is initially null but set to true in
  // ShouldStopIterating if an appropriate window is found.
  XID result_;

  DISALLOW_COPY_AND_ASSIGN(LocalProcessWindowFinder);
};

}  // namespace

// static
DockInfo DockInfo::GetDockInfoAtPoint(chrome::HostDesktopType host_desktop_type,
                                      const gfx::Point& screen_point,
                                      const std::set<gfx::NativeView>& ignore) {
  // TODO(beng):
  NOTIMPLEMENTED();
  return DockInfo();
}

// static
gfx::NativeView DockInfo::GetLocalProcessWindowAtPoint(
    chrome::HostDesktopType host_desktop_type,
    const gfx::Point& screen_point,
    const std::set<gfx::NativeView>& ignore) {
  // The X11 server is the canonical state of what the window stacking order
  // is.
  XID xid =
      LocalProcessWindowFinder::GetProcessWindowAtPoint(screen_point, ignore);
  return views::DesktopRootWindowHostX11::GetContentWindowForXID(xid);
}

bool DockInfo::GetWindowBounds(gfx::Rect* bounds) const {
  if (!window())
    return false;
  *bounds = window_->bounds();
  return true;
}

void DockInfo::SizeOtherWindowTo(const gfx::Rect& bounds) const {
  window_->SetBounds(bounds);
}

#endif
