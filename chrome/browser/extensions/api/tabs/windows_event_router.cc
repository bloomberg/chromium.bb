// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/windows_event_router.h"

#include "base/values.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/extensions/window_controller_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/notification_service.h"

#if defined(TOOLKIT_GTK)
#include "ui/base/x/active_window_watcher_x.h"
#endif

namespace event_names = extensions::event_names;

namespace extensions {

WindowsEventRouter::WindowsEventRouter(Profile* profile)
    : profile_(profile),
      focused_profile_(NULL),
      focused_window_id_(extension_misc::kUnknownWindowId) {
  DCHECK(!profile->IsOffTheRecord());

  WindowControllerList::GetInstance()->AddObserver(this);
#if defined(TOOLKIT_VIEWS)
  views::WidgetFocusManager::GetInstance()->AddFocusChangeListener(this);
#elif defined(TOOLKIT_GTK)
  ui::ActiveWindowWatcherX::AddObserver(this);
#elif defined(OS_MACOSX)
  // Needed for when no suitable window can be passed to an extension as the
  // currently focused window.
  registrar_.Add(this, chrome::NOTIFICATION_NO_KEY_WINDOW,
                 content::NotificationService::AllSources());
#endif
}

WindowsEventRouter::~WindowsEventRouter() {
  WindowControllerList::GetInstance()->RemoveObserver(this);
#if defined(TOOLKIT_VIEWS)
  views::WidgetFocusManager::GetInstance()->RemoveFocusChangeListener(this);
#elif defined(TOOLKIT_GTK)
  ui::ActiveWindowWatcherX::RemoveObserver(this);
#endif
}

void WindowsEventRouter::OnWindowControllerAdded(
    WindowController* window_controller) {
  if (!profile_->IsSameProfile(window_controller->profile()))
    return;

  scoped_ptr<base::ListValue> args(new ListValue());
  DictionaryValue* window_dictionary = window_controller->CreateWindowValue();
  args->Append(window_dictionary);
  DispatchEvent(event_names::kOnWindowCreated, window_controller->profile(),
                args.Pass());
}

void WindowsEventRouter::OnWindowControllerRemoved(
    WindowController* window_controller) {
  if (!profile_->IsSameProfile(window_controller->profile()))
    return;

  int window_id = window_controller->GetWindowId();
  scoped_ptr<base::ListValue> args(new ListValue());
  args->Append(Value::CreateIntegerValue(window_id));
  DispatchEvent(event_names::kOnWindowRemoved, window_controller->profile(),
                args.Pass());
}

#if defined(TOOLKIT_VIEWS)
void WindowsEventRouter::OnNativeFocusChange(
    gfx::NativeView focused_before,
    gfx::NativeView focused_now) {
  if (!focused_now)
    OnActiveWindowChanged(NULL);
}
#elif defined(TOOLKIT_GTK)
void WindowsEventRouter::ActiveWindowChanged(
    GdkWindow* active_window) {
  if (!active_window)
    OnActiveWindowChanged(NULL);
}
#endif

void WindowsEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
#if defined(OS_MACOSX)
  if (chrome::NOTIFICATION_NO_KEY_WINDOW == type) {
      OnActiveWindowChanged(NULL);
      return;
  }
#endif
}

static void WillDispatchWindowFocusedEvent(Profile* new_active_profile,
                                           int window_id,
                                           Profile* profile,
                                           const Extension* extension,
                                           ListValue* event_args) {
  // When switching between windows in the default and incognito profiles,
  // dispatch WINDOW_ID_NONE to extensions whose profile lost focus that
  // can't see the new focused window across the incognito boundary.
  // See crbug.com/46610.
  if (new_active_profile && new_active_profile != profile &&
      !extensions::ExtensionSystem::Get(profile)->extension_service()->
          CanCrossIncognito(extension)) {
    event_args->Clear();
    event_args->Append(Value::CreateIntegerValue(
        extension_misc::kUnknownWindowId));
  } else {
    event_args->Clear();
    event_args->Append(Value::CreateIntegerValue(window_id));
  }
}

void WindowsEventRouter::OnActiveWindowChanged(
    WindowController* window_controller) {
  Profile* window_profile = NULL;
  int window_id = extension_misc::kUnknownWindowId;
  if (window_controller &&
      profile_->IsSameProfile(window_controller->profile())) {
    window_profile = window_controller->profile();
    window_id = window_controller->GetWindowId();
  }

  if (focused_window_id_ == window_id)
    return;

  // window_profile is either the default profile for the active window, its
  // incognito profile, or NULL iff the previous profile is losing focus.
  focused_profile_ = window_profile;
  focused_window_id_ = window_id;

  scoped_ptr<Event> event(new Event(event_names::kOnWindowFocusedChanged,
                                    make_scoped_ptr(new ListValue())));
  event->will_dispatch_callback =
      base::Bind(&WillDispatchWindowFocusedEvent, window_profile, window_id);
  ExtensionSystem::Get(profile_)->event_router()->BroadcastEvent(event.Pass());
}

void WindowsEventRouter::DispatchEvent(const char* event_name,
                                      Profile* profile,
                                      scoped_ptr<base::ListValue> args) {
  scoped_ptr<Event> event(new Event(event_name, args.Pass()));
  event->restrict_to_profile = profile;
  ExtensionSystem::Get(profile)->event_router()->BroadcastEvent(event.Pass());
}

}  // namespace extensions
