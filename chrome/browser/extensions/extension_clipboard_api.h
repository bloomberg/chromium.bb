// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_CLIPBOARD_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_CLIPBOARD_API_H_
#pragma once

#include "chrome/browser/extensions/extension_function.h"

class RenderViewHost;

// Base class for clipboard function APIs.
class ClipboardFunction : public SyncExtensionFunction {
 public:
  virtual bool RunImpl();
  virtual bool RunImpl(RenderViewHost* render_view_host) = 0;

 protected:
  virtual ~ClipboardFunction() {}
};

class ExecuteCopyClipboardFunction : public ClipboardFunction {
 public:
  virtual bool RunImpl(RenderViewHost* render_view_host);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clipboard.executeCopy");
};

class ExecuteCutClipboardFunction : public ClipboardFunction {
 public:
  virtual bool RunImpl(RenderViewHost* render_view_host);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clipboard.executeCut");
};

class ExecutePasteClipboardFunction : public ClipboardFunction {
 public:
  virtual bool RunImpl(RenderViewHost* render_view_host);
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.clipboard.executePaste");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_CLIPBOARD_API_H_
