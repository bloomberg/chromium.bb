// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACCESSIBLE_VIEW_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_ACCESSIBLE_VIEW_HELPER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "chrome/browser/accessibility_events.h"
#include "ui/gfx/native_widget_types.h"

#if defined(OS_LINUX)
#include "chrome/browser/ui/gtk/accessible_widget_helper_gtk.h"
#endif

class AccessibilityEventRouterViews;
class Profile;
namespace views {
class View;
}

// NOTE: This class is part of the Accessibility Extension API, which lets
// extensions receive accessibility events. It's distinct from code that
// implements platform accessibility APIs like MSAA or ATK.
//
// Helper class that helps to manage the accessibility information for a
// view and all of its descendants.  Create an instance of this class for
// the root of a tree of views (like a dialog) that should send accessibility
// events for all of its descendants.
//
// Most controls have default behavior for accessibility; when this needs
// to be augmented, call one of the methods below to ignore a particular
// view or change its details.
//
// All of the information managed by this class is registered with the
// (global) AccessibilityEventRouterViews and unregistered when this object is
// destroyed.
class AccessibleViewHelper {
 public:
  // Constructs an AccessibleViewHelper that makes the given view and all
  // of its descendants accessible for the lifetime of this object,
  // sending accessibility notifications to the given profile.
  AccessibleViewHelper(views::View* view_tree, Profile* profile);

  virtual ~AccessibleViewHelper();

  // Sends a notification that a new window was opened now, and a
  // corresponding close window notification when this object
  // goes out of scope.
  void SendOpenWindowNotification(const std::string& window_title);

  // Uses the following string as the name of this view, instead of
  // view->GetAccessibleName().
  void SetViewName(views::View* view, std::string name);

  // Uses the following string id as the name of this view, instead of
  // view->GetAccessibleName().
  void SetViewName(views::View* view, int string_id);

 private:
  // Returns a native view if the given view has a native view in it.
  gfx::NativeView GetNativeView(views::View* view) const;

  AccessibilityEventRouterViews* accessibility_event_router_;
  Profile* profile_;
  views::View* view_tree_;
  std::string window_title_;
  std::vector<views::View*> managed_views_;

#if defined(OS_LINUX)
  scoped_ptr<AccessibleWidgetHelper> widget_helper_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AccessibleViewHelper);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACCESSIBLE_VIEW_HELPER_H_
