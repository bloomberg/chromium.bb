// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NUX_SET_AS_DEFAULT_SET_AS_DEFAULT_HANDLER_H_
#define COMPONENTS_NUX_SET_AS_DEFAULT_SET_AS_DEFAULT_HANDLER_H_

#include "base/macros.h"
#include "base/values.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace content {
class WebUIDataSource;
}  // namespace content

namespace nux {

class SetAsDefaultHandler : public content::WebUIMessageHandler {
 public:
  SetAsDefaultHandler();
  ~SetAsDefaultHandler() override;

  // WebUIMessageHandler:
  void RegisterMessages() override;

  // Adds webui sources.
  static void AddSources(content::WebUIDataSource* html_source);

  DISALLOW_COPY_AND_ASSIGN(SetAsDefaultHandler);
};

}  // namespace nux

#endif  // COMPONENTS_NUX_SET_AS_DEFAULT_SET_AS_DEFAULT_HANDLER_H_
