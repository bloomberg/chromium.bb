// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/virtual_keyboard/vk_webui_controller.h"

#include "athena/virtual_keyboard/vk_message_handler.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/keyboard_resources.h"
#include "grit/keyboard_resources_map.h"
#include "ui/keyboard/keyboard_constants.h"
#include "ui/keyboard/keyboard_util.h"

namespace athena {

namespace {

content::WebUIDataSource* CreateKeyboardUIDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(keyboard::kKeyboardHost);

  size_t count = 0;
  const GritResourceMap* resources =
      keyboard::GetKeyboardExtensionResources(&count);
  source->SetDefaultResource(IDR_KEYBOARD_INDEX);

  const std::string keyboard_host =
      base::StringPrintf("%s/", keyboard::kKeyboardHost);
  for (size_t i = 0; i < count; ++i) {
    size_t offset = 0;
    if (StartsWithASCII(std::string(resources[i].name), keyboard_host, false))
      offset = keyboard_host.length();
    source->AddResourcePath(resources[i].name + offset, resources[i].value);
  }
  return source;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// VKWebUIController:

VKWebUIController::VKWebUIController(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  web_ui->AddMessageHandler(new VKMessageHandler());
  content::WebUIDataSource::Add(browser_context, CreateKeyboardUIDataSource());
}

VKWebUIController::~VKWebUIController() {
}

////////////////////////////////////////////////////////////////////////////////
// VKWebUIControllerFactory:

content::WebUI::TypeID VKWebUIControllerFactory::GetWebUIType(
    content::BrowserContext* browser_context,
    const GURL& url) const {
  if (url == GURL(keyboard::kKeyboardURL))
    return const_cast<VKWebUIControllerFactory*>(this);

  return content::WebUI::kNoWebUI;
}

bool VKWebUIControllerFactory::UseWebUIForURL(
    content::BrowserContext* browser_context,
    const GURL& url) const {
  return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
}

bool VKWebUIControllerFactory::UseWebUIBindingsForURL(
    content::BrowserContext* browser_context,
    const GURL& url) const {
  return UseWebUIForURL(browser_context, url);
}

content::WebUIController* VKWebUIControllerFactory::CreateWebUIControllerForURL(
    content::WebUI* web_ui,
    const GURL& url) const {
  if (url == GURL(keyboard::kKeyboardURL))
    return new VKWebUIController(web_ui);
  return NULL;
}

// static
VKWebUIControllerFactory* VKWebUIControllerFactory::GetInstance() {
  return Singleton<VKWebUIControllerFactory>::get();
}

// protected
VKWebUIControllerFactory::VKWebUIControllerFactory() {
}

VKWebUIControllerFactory::~VKWebUIControllerFactory() {
}

}  // namespace athena
