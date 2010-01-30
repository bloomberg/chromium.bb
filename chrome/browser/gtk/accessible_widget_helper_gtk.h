// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_ACCESSIBLE_WIDGET_HELPER_GTK_H_
#define CHROME_BROWSER_GTK_ACCESSIBLE_WIDGET_HELPER_GTK_H_

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "chrome/browser/gtk/accessibility_event_router_gtk.h"
#include "chrome/common/accessibility_events.h"

class Profile;

// Helper class that helps to manage the accessibility information for all
// of the widgets in a container.  Create an instance of this class for
// each container GtkWidget (like a dialog) that should send accessibility
// events for all of its descendants.
//
// Most controls have default behavior for accessibility; when this needs
// to be augmented, call one of the methods below to ignore a particular
// widget or change its details.
//
// All of the information managed by this class is registered with the
// (global) AccessibilityEventRouter and unregistered when this object is
// destroyed.
class AccessibleWidgetHelper {
 public:
  // Contruct an AccessibleWidgetHelper that makes the given root widget
  // accessible for the lifetime of this object, sending accessibility
  // notifications to the given profile.
  AccessibleWidgetHelper(GtkWidget* root_widget, Profile* profile);

  virtual ~AccessibleWidgetHelper();

  // Do not send accessibility events for this widget
  void IgnoreWidget(GtkWidget* widget);

  // Use the following string as the name of this widget, instead of the
  // gtk label associated with the widget.
  void SetWidgetName(GtkWidget* widget, std::string name);

  // Use the following string as the name of this widget, instead of the
  // gtk label associated with the widget.
  void SetWidgetName(GtkWidget* widget, int string_id);

 private:
  AccessibilityEventRouter* accessibility_event_router_;
  GtkWidget* root_widget_;
  std::vector<GtkWidget*> managed_widgets_;
};

#endif  // CHROME_BROWSER_GTK_ACCESSIBLE_WIDGET_HELPER_GTK_H_
