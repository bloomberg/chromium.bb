// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/devtools/devtools_adb_bridge.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

class InspectUI : public content::WebUIController,
                  public content::NotificationObserver,
                  public DevToolsAdbBridge::Listener {
 public:
  explicit InspectUI(content::WebUI* web_ui);
  virtual ~InspectUI();

  void InitUI();
  void InspectRemotePage(const std::string& page_id);
  void ActivateRemotePage(const std::string& page_id);
  void CloseRemotePage(const std::string& page_id);
  void ReloadRemotePage(const std::string& page_id);
  void OpenRemotePage(const std::string& browser_id, const std::string& url);

 private:
  class WorkerCreationDestructionListener;

  void PopulateLists();

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void StartListeningNotifications();
  void StopListeningNotifications();

  content::WebUIDataSource* CreateInspectUIHTMLSource();

  // DevToolsAdbBridge::Listener overrides.
  virtual void RemoteDevicesChanged(
      DevToolsAdbBridge::RemoteDevices* devices) OVERRIDE;

  void UpdatePortForwardingEnabled();
  void UpdatePortForwardingConfig();

  scoped_refptr<WorkerCreationDestructionListener> observer_;

  // A scoped container for notification registries.
  content::NotificationRegistrar notification_registrar_;

  // A scoped container for preference change registries.
  PrefChangeRegistrar pref_change_registrar_;

  typedef std::map<std::string, scoped_refptr<DevToolsAdbBridge::RemotePage> >
      RemotePages;
  RemotePages remote_pages_;

  typedef std::map<std::string,
      scoped_refptr<DevToolsAdbBridge::RemoteBrowser> > RemoteBrowsers;
  RemoteBrowsers remote_browsers_;

  DISALLOW_COPY_AND_ASSIGN(InspectUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_
