// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/streams_private/streams_private_api.h"

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function_registry.h"
#include "chrome/browser/extensions/extension_input_module_constants.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/mime_types_handler.h"

namespace keys = extension_input_module_constants;

namespace events {

const char kOnExecuteMimeTypeHandler[] =
    "streamsPrivate.onExecuteMimeTypeHandler";

}  // namespace events

namespace extensions {

StreamsPrivateAPI::StreamsPrivateAPI(Profile* profile)
    : profile_(profile) {
  (new MimeTypesHandlerParser)->Register();
}

StreamsPrivateAPI::~StreamsPrivateAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<StreamsPrivateAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
ProfileKeyedAPIFactory<StreamsPrivateAPI>*
    StreamsPrivateAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
