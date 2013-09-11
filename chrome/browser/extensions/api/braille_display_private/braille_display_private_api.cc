// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"

namespace OnKeyEvent = extensions::api::braille_display_private::OnKeyEvent;
namespace WriteDots = extensions::api::braille_display_private::WriteDots;
using extensions::api::braille_display_private::KeyEvent;
using extensions::api::braille_display_private::BrailleController;

namespace extensions {
BrailleDisplayPrivateAPI::BrailleDisplayPrivateAPI(Profile* profile)
    : profile_(profile) {
  // TODO(plundblad): Consider defering this until someone actually uses the
  // API so that we only watch for braille displays if cvox is enabled.
  BrailleController::GetInstance()->AddObserver(this);
}

BrailleDisplayPrivateAPI::~BrailleDisplayPrivateAPI() {
  BrailleController::GetInstance()->RemoveObserver(this);
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

void BrailleDisplayPrivateAPI::OnKeyEvent(
    const KeyEvent& keyEvent) {
  // TODO(plundblad): Only send the event to the active profile.
  scoped_ptr<Event> event(new Event(
      OnKeyEvent::kEventName, OnKeyEvent::Create(keyEvent)));
  ExtensionSystem::Get(profile_)->event_router()->BroadcastEvent(event.Pass());
}

namespace api {
bool BrailleDisplayPrivateGetDisplayStateFunction::Prepare() {
  return true;
}

void BrailleDisplayPrivateGetDisplayStateFunction::Work() {
  SetResult(BrailleController::GetInstance()->GetDisplayState().release());
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
