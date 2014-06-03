// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_COMPONENTS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_COMPONENTS_UI_H_

#include "chrome/browser/component_updater/component_updater_service.h"
#include "chrome/browser/component_updater/crx_update_item.h"
#include "content/public/browser/web_ui_controller.h"
#include "ui/base/layout.h"

namespace base {
class RefCountedMemory;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class ComponentsUI : public content::WebUIController,
                     public component_updater::ServiceObserver {
 public:
  explicit ComponentsUI(content::WebUI* web_ui);
  virtual ~ComponentsUI();

  static void OnDemandUpdate(const std::string& component_id);

  static base::ListValue* LoadComponents();

  static base::RefCountedMemory* GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor);

  // ServiceObserver implementation.
  virtual void OnEvent(Events event, const std::string& id) OVERRIDE;

 private:
  static base::string16 ComponentEventToString(Events event);
  static base::string16 ServiceStatusToString(
      component_updater::CrxUpdateItem::Status status);
  DISALLOW_COPY_AND_ASSIGN(ComponentsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_COMPONENTS_UI_H_
