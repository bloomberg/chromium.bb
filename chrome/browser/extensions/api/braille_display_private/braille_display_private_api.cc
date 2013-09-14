// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"
#include "chrome/browser/extensions/extension_system.h"

namespace OnDisplayStateChanged =
    extensions::api::braille_display_private::OnDisplayStateChanged;
namespace OnKeyEvent = extensions::api::braille_display_private::OnKeyEvent;
namespace WriteDots = extensions::api::braille_display_private::WriteDots;
using extensions::api::braille_display_private::DisplayState;
using extensions::api::braille_display_private::KeyEvent;
using extensions::api::braille_display_private::BrailleController;

namespace extensions {
BrailleDisplayPrivateAPI::BrailleDisplayPrivateAPI(Profile* profile)
    : profile_(profile), scoped_observer_(this) {
  EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
  event_router->RegisterObserver(this, OnDisplayStateChanged::kEventName);
  event_router->RegisterObserver(this, OnKeyEvent::kEventName);
}

BrailleDisplayPrivateAPI::~BrailleDisplayPrivateAPI() {
  ExtensionSystem::Get(profile_)->event_router()->UnregisterObserver(this);
}

void BrailleDisplayPrivateAPI::Shutdown() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<BrailleDisplayPrivateAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<BrailleDisplayPrivateAPI>*
BrailleDisplayPrivateAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

void BrailleDisplayPrivateAPI::OnDisplayStateChanged(
    const DisplayState& display_state) {
  scoped_ptr<Event> event(new Event(
      OnDisplayStateChanged::kEventName,
      OnDisplayStateChanged::Create(display_state)));
  ExtensionSystem::Get(profile_)->event_router()->BroadcastEvent(event.Pass());
}

void BrailleDisplayPrivateAPI::OnKeyEvent(
    const KeyEvent& key_event) {
  // TODO(plundblad): Only send the event to the active profile.
  scoped_ptr<Event> event(new Event(
      OnKeyEvent::kEventName, OnKeyEvent::Create(key_event)));
  ExtensionSystem::Get(profile_)->event_router()->BroadcastEvent(event.Pass());
}

void BrailleDisplayPrivateAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  BrailleController* braille_controller = BrailleController::GetInstance();
  if (!scoped_observer_.IsObserving(braille_controller))
    scoped_observer_.Add(braille_controller);
}

void BrailleDisplayPrivateAPI::OnListenerRemoved(
    const EventListenerInfo& details) {
  BrailleController* braille_controller = BrailleController::GetInstance();
  EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
  if (!(event_router->HasEventListener(OnDisplayStateChanged::kEventName) ||
        event_router->HasEventListener(OnKeyEvent::kEventName)) &&
      scoped_observer_.IsObserving(braille_controller)) {
    scoped_observer_.Remove(braille_controller);
  }
}

namespace api {
bool BrailleDisplayPrivateGetDisplayStateFunction::Prepare() {
  return true;
}

void BrailleDisplayPrivateGetDisplayStateFunction::Work() {
  SetResult(
      BrailleController::GetInstance()->GetDisplayState()->ToValue().release());
}

bool BrailleDisplayPrivateGetDisplayStateFunction::Respond() {
  return true;
}

BrailleDisplayPrivateWriteDotsFunction::
BrailleDisplayPrivateWriteDotsFunction() {
}

BrailleDisplayPrivateWriteDotsFunction::
~BrailleDisplayPrivateWriteDotsFunction() {
}

bool BrailleDisplayPrivateWriteDotsFunction::Prepare() {
  params_ = WriteDots::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(params_);
  return true;
}

void BrailleDisplayPrivateWriteDotsFunction::Work() {
  BrailleController::GetInstance()->WriteDots(params_->cells);
}

bool BrailleDisplayPrivateWriteDotsFunction::Respond() {
  return true;
}
}  // namespace api
}  // namespace extensions
