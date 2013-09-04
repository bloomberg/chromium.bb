// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/braille_display_private_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"

namespace OnKeyEvent = extensions::api::braille_display_private::OnKeyEvent;
namespace WriteDots = extensions::api::braille_display_private::WriteDots;
using extensions::api::braille_display_private::KeyEvent;

namespace extensions {
BrailleDisplayPrivateAPI::BrailleDisplayPrivateAPI(Profile* profile)
    : profile_(profile) {
}

BrailleDisplayPrivateAPI::~BrailleDisplayPrivateAPI() {
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

namespace api {
bool BrailleDisplayPrivateGetDisplayStateFunction::Prepare() {
  return true;
}

void BrailleDisplayPrivateGetDisplayStateFunction::Work() {
  // TODO(plundblad): implement.
}

bool BrailleDisplayPrivateGetDisplayStateFunction::Respond() {
  return false;
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
  // TODO(plundblad): Implement.
}

bool BrailleDisplayPrivateWriteDotsFunction::Respond() {
  return true;
}
}  // namespace api
}  // namespace extensions
