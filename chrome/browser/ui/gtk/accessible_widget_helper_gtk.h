// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_ACCESSIBLE_WIDGET_HELPER_GTK_H_
#define CHROME_BROWSER_UI_GTK_ACCESSIBLE_WIDGET_HELPER_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/browser/accessibility_events.h"
#include "chrome/browser/ui/gtk/accessibility_event_router_gtk.h"

class Profile;

// NOTE: This class is part of the Accessibility Extension API, which lets
// extensions receive accessibility events. It's distinct from code that
// implements platform accessibility APIs like MSAA or ATK.
//
// Helper class that helps to manage the accessibility information for all
// of the widgets in a container.  Create an instance of this class for
// each container GtkWidget (like a dialog) that should send accessibility
// events for all of its descendants.
//
// Most controls have default behavior for accessibility; when this needs
// to be augmented, call one of the methods below to change its details.
//
// All of the information managed by this class is registered with the
// (global) AccessibilityEventRouterGtk and unregistered when this object is
// destroyed.
class AccessibleWidgetHelper {
 public:
  // Contruct an AccessibleWidgetHelper that makes the given root widget
  // accessible for the lifetime of this object, sending accessibility
  // notifications to the given profile.
  AccessibleWidgetHelper(GtkWidget* root_widget, Profile* profile);

  virtual ~AccessibleWidgetHelper();

  // Send a notification that a new window was opened now, and a
  // corresponding close window notification when this object
  // goes out of scope.
  void SendOpenWindowNotification(const std::string& window_title);

  // Use the following string as the name of this widget, instead of the
  // gtk label associated with the widget.
  void SetWidgetName(GtkWidget* widget, std::string name);

  // Use the following string as the name of this widget, instead of the
  // gtk label associated with the widget.
  void SetWidgetName(GtkWidget* widget, int string_id);

 private:
  AccessibilityEventRouterGtk* accessibility_event_router_;
  Profile* profile_;
  GtkWidget* root_widget_;
  std::string window_title_;
  std::set<GtkWidget*> managed_widgets_;
};

#endif  // CHROME_BROWSER_UI_GTK_ACCESSIBLE_WIDGET_HELPER_GTK_H_
