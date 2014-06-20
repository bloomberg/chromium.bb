// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_VIRTUAL_KEYBOARD_VK_WEBUI_CONTROLLER_H_
#define ATHENA_VIRTUAL_KEYBOARD_VK_WEBUI_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"

namespace athena {

class VKWebUIController : public content::WebUIController {
 public:
  explicit VKWebUIController(content::WebUI* web_ui);
  virtual ~VKWebUIController();

 private:
  DISALLOW_COPY_AND_ASSIGN(VKWebUIController);
};

class VKWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  // WebUIControllerFactory:
  virtual content::WebUI::TypeID GetWebUIType(
      content::BrowserContext* browser_context,
      const GURL& url) const OVERRIDE;
  virtual bool UseWebUIForURL(content::BrowserContext* browser_context,
                              const GURL& url) const OVERRIDE;
  virtual bool UseWebUIBindingsForURL(content::BrowserContext* browser_context,
                                      const GURL& url) const OVERRIDE;
  virtual content::WebUIController* CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) const OVERRIDE;

  static VKWebUIControllerFactory* GetInstance();

 protected:
  VKWebUIControllerFactory();
  virtual ~VKWebUIControllerFactory();

 private:
  friend struct DefaultSingletonTraits<VKWebUIControllerFactory>;

  DISALLOW_COPY_AND_ASSIGN(VKWebUIControllerFactory);
};

}  // namespace athena

#endif  // ATHENA_VIRTUAL_KEYBOARD_VK_WEBUI_CONTROLLER_H_
