// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_handlers/file_handlers_api.h"

#include "base/lazy_instance.h"
#include "chrome/common/extensions/api/file_handlers/file_handlers_parser.h"
#include "chrome/common/extensions/extension_manifest_constants.h"

namespace extensions {

FileHandlersAPI::FileHandlersAPI(Profile* profile) {
  ManifestHandler::Register(extension_manifest_keys::kFileHandlers,
                            new FileHandlersParser);
}

FileHandlersAPI::~FileHandlersAPI() {
}

static base::LazyInstance<ProfileKeyedAPIFactory<FileHandlersAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

ProfileKeyedAPIFactory<FileHandlersAPI>* FileHandlersAPI::GetFactoryInstance() {
  return &g_factory.Get();
}

}  // namespace extensions
