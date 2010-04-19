// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_ACCESSIBILITY_EVENT_ROUTER_VIEWS_H_
#define CHROME_BROWSER_VIEWS_ACCESSIBILITY_EVENT_ROUTER_VIEWS_H_

#include <string>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/singleton.h"
#include "base/task.h"
#include "chrome/browser/accessibility_events.h"
#include "views/view.h"
#include "views/widget/root_view.h"

class Profile;

// Allows us to use (View*) and (FocusManager*) in a hash_map with gcc.
#if defined(COMPILER_GCC)
namespace __gnu_cxx {
template<>
struct hash<views::FocusManager*> {
  size_t operator()(views::FocusManager* focus_manager) const {
    return reinterpret_cast<size_t>(focus_manager);
  }
};
template<>
struct hash<views::View*> {
  size_t operator()(views::View* view) const {
    return reinterpret_cast<size_t>(view);
  }
};
}  // namespace __gnu_cxx
#endif  // defined(COMPILER_GCC)

// NOTE: This class is part of the Accessibility Extension API, which lets
// extensions receive accessibility events. It's distinct from code that
// implements platform accessibility APIs like MSAA or ATK.
//
// Singleton class that adds listeners to many views, then sends an
// accessibility notification whenever a relevant event occurs in an
// accessible view.
//
// Views are not accessible by default. When you register a root widget,
// that widget and all of its descendants will start sending accessibility
// event notifications. You can then override the default behavior for
// specific descendants using other methods.
//
// You can use Profile::PauseAccessibilityEvents to prevent a flurry
// of accessibility events when a window is being created or initialized.
class AccessibilityEventRouterViews
    : public views::FocusChangeListener {
 public:
  // Internal information about a particular view to override the
  // information we get directly from the view.
  struct ViewInfo {
    ViewInfo() : ignore(false), focus_manager(NULL) {}

    // If nonempty, will use this name instead of the view's label.
    std::string name;

    // If true, will ignore this widget and not send accessibility events.
    bool ignore;

    // The focus manager that this view is part of - saved because
    // GetFocusManager may not succeed while a view is being deleted.
    views::FocusManager* focus_manager;
  };

  // Get the single instance of this class.
  static AccessibilityEventRouterViews* GetInstance();

  // Start sending accessibility events for this view and all of its
  // descendants.  Notifications will go to the specified profile.
  // Returns true on success, false if "view" was already registered.
  // It is the responsibility of the caller to call RemoveViewTree if
  // this view is ever deleted; consider using AccessibleViewHelper.
  bool AddViewTree(views::View* view, Profile* profile);

  // Stop sending accessibility events for this view and all of its
  // descendants.
  void RemoveViewTree(views::View* view);

  // Don't send any events for this view.
  void IgnoreView(views::View* view);

  // Use the following string as the name of this view, instead of the
  // gtk label associated with the view.
  void SetViewName(views::View* view, std::string name);

  // Forget all information about this view.
  void RemoveView(views::View* view);

  // Implementation of views::FocusChangeListener:
  virtual void FocusWillChange(
      views::View* focused_before, views::View* focused_now);

 private:
  // Given a view, determine if it's part of a view tree that's mapped to
  // a profile and if so, if it's marked as accessible.
  void FindView(views::View* view, Profile** profile, bool* is_accessible);

  // Checks the type of the view and calls one of the more specific
  // Send*Notification methods, below.
  void DispatchAccessibilityNotification(
      views::View* view, NotificationType type);

  // Return the name of a view.
  std::string GetViewName(views::View* view);

  // Each of these methods constructs an AccessibilityControlInfo object
  // and sends a notification of a specific accessibility event.
  void SendButtonNotification(
      views::View* view, NotificationType type, Profile* profile);
  void SendLinkNotification(
      views::View* view, NotificationType type, Profile* profile);
  void SendMenuNotification(
      views::View* view, NotificationType type, Profile* profile);

 private:
  AccessibilityEventRouterViews();
  virtual ~AccessibilityEventRouterViews();

  friend struct DefaultSingletonTraits<AccessibilityEventRouterViews>;

  // The set of all view tree roots; only descendants of these will generate
  // accessibility notifications.
  base::hash_map<views::View*, Profile*> view_tree_profile_map_;

  // Extra information about specific views.
  base::hash_map<views::View*, ViewInfo> view_info_map_;

  // Count of the number of references to each focus manager.
  base::hash_map<views::FocusManager*, int> focus_manager_ref_count_;

  // Used to defer handling of some events until the next time
  // through the event loop.
  ScopedRunnableMethodFactory<AccessibilityEventRouterViews> method_factory_;
};

#endif  // CHROME_BROWSER_VIEWS_ACCESSIBILITY_EVENT_ROUTER_VIEWS_H_
