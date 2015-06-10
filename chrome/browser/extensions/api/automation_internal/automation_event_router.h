// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_

#include <vector>

#include "base/memory/singleton.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}  // namespace content

struct ExtensionMsg_AccessibilityEventParams;

namespace extensions {

struct AutomationListener;

class AutomationEventRouter : public content::NotificationObserver {
 public:
  static AutomationEventRouter* GetInstance();

  // Indicates that the listener at |listener_process_id|, |listener_routing_id|
  // wants to receive automation events from the accessibility tree indicated
  // by |source_ax_tree_id|. Automation events are forwarded from now on
  // until the listener process dies.
  void RegisterListenerForOneTree(int listener_process_id,
                                  int listener_routing_id,
                                  int source_ax_tree_id);

  // Indicates that the listener at |listener_process_id|, |listener_routing_id|
  // wants to receive automation events from all accessibility trees because
  // it has Desktop permission.
  void RegisterListenerWithDesktopPermission(int listener_process_id,
                                             int listener_routing_id);

  void DispatchAccessibilityEvent(
      const ExtensionMsg_AccessibilityEventParams& params);

  // Notify all automation extensions that an accessibility tree was
  // destroyed. If |browser_context| is null,
  void DispatchTreeDestroyedEvent(
      int tree_id,
      content::BrowserContext* browser_context);

 private:
  struct AutomationListener {
    AutomationListener();
    ~AutomationListener();

    int routing_id;
    int process_id;
    bool desktop;
    std::set<int> tree_ids;
  };

  AutomationEventRouter();
  ~AutomationEventRouter() override;

  void Register(
      int listener_process_id,
      int listener_routing_id,
      int source_ax_tree_id,
      bool desktop);

  // content::NotificationObserver interface.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  content::NotificationRegistrar registrar_;
  std::vector<AutomationListener> listeners_;

  friend struct DefaultSingletonTraits<AutomationEventRouter>;

  DISALLOW_COPY_AND_ASSIGN(AutomationEventRouter);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOMATION_INTERNAL_AUTOMATION_EVENT_ROUTER_H_
