// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_HELPER_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_HELPER_H_
#pragma once

#include "content/renderer/render_view_observer.h"

class GURL;
class ListValue;

// Filters extension related messages sent to RenderViews.
class ExtensionHelper : public RenderViewObserver {
 public:
  explicit ExtensionHelper(RenderView* render_view);
  virtual ~ExtensionHelper();

 private:
  // RenderViewObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message);

  void OnExtensionResponse(int request_id, bool success,
                           const std::string& response,
                           const std::string& error);
  void OnExtensionMessageInvoke(const std::string& extension_id,
                                const std::string& function_name,
                                const ListValue& args,
                                const GURL& event_url);


  DISALLOW_COPY_AND_ASSIGN(ExtensionHelper);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_HELPER_H_
