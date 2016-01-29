// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PLUGINS_PLUGINS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PLUGINS_PLUGINS_UI_H_

#include "base/macros.h"
#include "chrome/browser/ui/webui/mojo_web_ui_controller.h"
#include "chrome/browser/ui/webui/plugins/plugins.mojom.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class PluginsHandler;

class PluginsUI : public MojoWebUIController<PluginsHandlerMojo> {
 public:
  explicit PluginsUI(content::WebUI* web_ui);
  ~PluginsUI() override;

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // MojoWebUIController overrides:
  void BindUIHandler(
      mojo::InterfaceRequest<PluginsHandlerMojo> request) override;

  scoped_ptr<PluginsHandler> plugins_handler_;

  DISALLOW_COPY_AND_ASSIGN(PluginsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PLUGINS_PLUGINS_UI_H_
