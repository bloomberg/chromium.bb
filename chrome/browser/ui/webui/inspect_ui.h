// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

namespace base {
class Value;
class ListValue;
}

class Browser;
class DevToolsTargetsUIHandler;
class DevToolsTargetImpl;
class PortForwardingStatusSerializer;

class InspectUI : public content::WebUIController,
                  public content::NotificationObserver {
 public:
  explicit InspectUI(content::WebUI* web_ui);
  virtual ~InspectUI();

  void InitUI();
  void Inspect(const std::string& source_id, const std::string& target_id);
  void Activate(const std::string& source_id, const std::string& target_id);
  void Close(const std::string& source_id, const std::string& target_id);
  void Reload(const std::string& source_id, const std::string& target_id);
  void Open(const std::string& source_id,
            const std::string& browser_id,
            const std::string& url);
  void InspectBrowserWithCustomFrontend(
      const std::string& source_id,
      const std::string& browser_id,
      const GURL& frontend_url);

  static void InspectDevices(Browser* browser);

 private:
  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void StartListeningNotifications();
  void StopListeningNotifications();

  content::WebUIDataSource* CreateInspectUIHTMLSource();

  void UpdateDiscoverUsbDevicesEnabled();
  void UpdatePortForwardingEnabled();
  void UpdatePortForwardingConfig();

  void SetPortForwardingDefaults();

  const base::Value* GetPrefValue(const char* name);

  void AddTargetUIHandler(
      scoped_ptr<DevToolsTargetsUIHandler> handler);

  DevToolsTargetsUIHandler* FindTargetHandler(
      const std::string& source_id);
  DevToolsTargetImpl* FindTarget(const std::string& source_id,
                                 const std::string& target_id);

  void PopulateTargets(const std::string& source_id,
                       const base::ListValue& targets);

  void ForceUpdateIfNeeded(const std::string& source_id,
                           const std::string& target_type);

  void PopulatePortStatus(const base::Value& status);

  void ShowIncognitoWarning();

  // A scoped container for notification registries.
  content::NotificationRegistrar notification_registrar_;

  // A scoped container for preference change registries.
  PrefChangeRegistrar pref_change_registrar_;

  typedef std::map<std::string, DevToolsTargetsUIHandler*> TargetHandlerMap;
  TargetHandlerMap target_handlers_;

  scoped_ptr<PortForwardingStatusSerializer> port_status_serializer_;

  DISALLOW_COPY_AND_ASSIGN(InspectUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_
