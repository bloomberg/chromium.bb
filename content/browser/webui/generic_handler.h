// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_GENERIC_HANDLER_H_
#define CONTENT_BROWSER_WEBUI_GENERIC_HANDLER_H_
#pragma once

#include "content/browser/webui/web_ui.h"

namespace base {
class ListValue;
}

// A place to add handlers for messages shared across all WebUI pages.
class GenericHandler : public WebUIMessageHandler {
 public:
  GenericHandler();
  virtual ~GenericHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;
  virtual bool IsLoading() const OVERRIDE;

 private:
  void HandleNavigateToUrl(const base::ListValue* args);

  // Javascript hook to indicate whether or not a long running operation is in
  // progress.
  void HandleSetIsLoading(const base::ListValue* args);

  // Indicates whether or not this WebUI is performing a long running operation
  // and that the throbber should reflect this.
  void SetIsLoading(bool is_loading);

  bool is_loading_;

  DISALLOW_COPY_AND_ASSIGN(GenericHandler);
};

#endif  // CONTENT_BROWSER_WEBUI_GENERIC_HANDLER_H_
