// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_clipboard_api.h"

#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tabs_module.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace {
// Errors.
const char kNoTabError[] = "No tab with id: *.";
}

bool ClipboardFunction::RunImpl() {
  int tab_id;
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id));

  TabContentsWrapper* contents = NULL;
  if (!ExtensionTabUtil::GetTabById(tab_id, profile(), include_incognito(),
                                    NULL, NULL, &contents, NULL)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        kNoTabError, base::IntToString(tab_id));
    return false;
  }

  RenderViewHost* render_view_host = contents->render_view_host();
  if (!render_view_host) {
    return false;
  }

  return RunImpl(render_view_host);
}

bool ExecuteCopyClipboardFunction::RunImpl(RenderViewHost* render_view_host) {
  render_view_host->Copy();
  return true;
}

bool ExecuteCutClipboardFunction::RunImpl(RenderViewHost* render_view_host) {
  render_view_host->Cut();
  return true;
}

bool ExecutePasteClipboardFunction::RunImpl(RenderViewHost* render_view_host) {
  render_view_host->Paste();
  return true;
}
