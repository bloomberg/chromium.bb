// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONFLICTS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CONFLICTS_HANDLER_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/win/enumerate_modules_model.h"
#include "content/public/browser/web_ui_message_handler.h"

class ConflictsHandler : public content::WebUIMessageHandler,
                         public EnumerateModulesModel::Observer {
 public:
  ConflictsHandler();
  ~ConflictsHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestModuleList" message.
  void HandleRequestModuleList(const base::ListValue* args);

 private:
  void SendModuleList();

  // EnumerateModulesModel::Observer implementation.
  void OnScanCompleted() override;

  ScopedObserver<EnumerateModulesModel, EnumerateModulesModel::Observer>
      observer_;

  DISALLOW_COPY_AND_ASSIGN(ConflictsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_CONFLICTS_HANDLER_H_
