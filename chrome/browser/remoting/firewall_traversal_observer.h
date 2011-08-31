// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_REMOTING_FIREWALL_TRAVERSAL_OBSERVER_H_
#define CHROME_BROWSER_REMOTING_FIREWALL_TRAVERSAL_OBSERVER_H_
#pragma once

#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/common/notification_observer.h"
#include "ipc/ipc_channel.h"

// Per-tab class to manage the firewall traversal policies for
// the remoting plugin.
// TODO(dmaclach): Replace this with a more generic mechanism for
//                 plugins to access preferences. http://crbug.com/90543
class FirewallTraversalObserver : public NotificationObserver,
                                  public TabContentsObserver {
 public:
  explicit FirewallTraversalObserver(TabContents* tab_contents);
  virtual ~FirewallTraversalObserver();

  static void RegisterUserPrefs(PrefService* prefs);

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // IPC::Channel::Listener overrides:
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  void UpdateFirewallTraversalState();

  // Registers and unregisters us for notifications.
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(FirewallTraversalObserver);
};

#endif  // CHROME_BROWSER_REMOTING_FIREWALL_TRAVERSAL_OBSERVER_H_
