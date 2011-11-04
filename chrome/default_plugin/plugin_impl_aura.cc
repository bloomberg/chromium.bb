// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/default_plugin/plugin_impl_aura.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/common/chrome_plugin_messages.h"
#include "chrome/default_plugin/plugin_main.h"
#include "content/common/child_thread.h"
#include "content/public/common/content_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/webkit_strings.h"
#include "unicode/locid.h"
#include "webkit/plugins/npapi/default_plugin_shared.h"

PluginInstallerImpl::PluginInstallerImpl(int16 mode) {}

PluginInstallerImpl::~PluginInstallerImpl() {
}

bool PluginInstallerImpl::Initialize(void* module_handle,
                                     NPP instance,
                                     NPMIMEType mime_type,
                                     int16 argc,
                                     char* argn[],
                                     char* argv[]) {
  DVLOG(1) << __FUNCTION__ << " MIME Type : " << mime_type;
  DCHECK(instance != NULL);

  if (mime_type == NULL || strlen(mime_type) == 0) {
    DLOG(WARNING) << __FUNCTION__ << " Invalid parameters passed in";
    NOTREACHED();
    return false;
  }

  PluginInstallerBase::SetRoutingIds(argc, argn, argv);
  return true;
}

bool PluginInstallerImpl::NPP_SetWindow(NPWindow* window_info) {
  NOTIMPLEMENTED();
  return true;
}

void PluginInstallerImpl::Shutdown() {
}

void PluginInstallerImpl::NewStream(NPStream* stream) {
  NOTIMPLEMENTED();
}

void PluginInstallerImpl::DestroyStream(NPStream* stream, NPError reason) {
  NOTIMPLEMENTED();
}

bool PluginInstallerImpl::WriteReady(NPStream* stream) {
  NOTIMPLEMENTED();
  return false;
}

int32 PluginInstallerImpl::Write(NPStream* stream, int32 offset,
                                 int32 buffer_length, void* buffer) {
  return true;
}

void PluginInstallerImpl::URLNotify(const char* url, NPReason reason) {
}

int16 PluginInstallerImpl::NPP_HandleEvent(void* event) {
  return 0;
}
