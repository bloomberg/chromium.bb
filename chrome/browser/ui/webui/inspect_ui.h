// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_
#define CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
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
  void RefreshUI();

 private:
  class WorkerCreationDestructionListener;

  // content::NotificationObserver overrides.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  void StopListeningNotifications();

  content::WebUIDataSource* CreateInspectUIHTMLSource();

  void PopulateLists(base::ListValue* target_list);

  // DevToolsAdbBridge::Listener overrides.
  virtual void RemotePagesChanged(
      DevToolsAdbBridge::RemotePages* pages) OVERRIDE;

  scoped_refptr<WorkerCreationDestructionListener> observer_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  DevToolsAdbBridge* adb_bridge_;
  base::WeakPtrFactory<InspectUI> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InspectUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_INSPECT_UI_H_
