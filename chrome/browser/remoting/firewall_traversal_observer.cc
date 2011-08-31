// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/remoting/firewall_traversal_observer.h"

#include "base/stringprintf.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/notification_service.h"
#include "content/common/view_messages.h"
#include "ipc/ipc_message_macros.h"

FirewallTraversalObserver::FirewallTraversalObserver(
    TabContents* tab_contents)
    : TabContentsObserver(tab_contents) {
  // Register for notifications about all interested prefs change.
  Profile* profile =
      Profile::FromBrowserContext(tab_contents->browser_context());
  PrefService* prefs = profile->GetPrefs();
  pref_change_registrar_.Init(prefs);
  if (prefs) {
    pref_change_registrar_.Add(prefs::kRemoteAccessClientFirewallTraversal,
                               this);
    // kRemoteAccessHostFirewallTraversal intentionally not added because
    // the host plugin is going to track it itself.
  }
}

FirewallTraversalObserver::~FirewallTraversalObserver() {
  // We don't want any notifications while we're running our destructor.
  pref_change_registrar_.RemoveAll();
}

// static
void FirewallTraversalObserver::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterBooleanPref(prefs::kRemoteAccessClientFirewallTraversal,
                             true,
                             PrefService::UNSYNCABLE_PREF);
  prefs->RegisterBooleanPref(prefs::kRemoteAccessHostFirewallTraversal,
                             true,
                             PrefService::UNSYNCABLE_PREF);
}

void FirewallTraversalObserver::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {
      std::string* pref_name_in = Details<std::string>(details).ptr();
      Profile* profile =
          Profile::FromBrowserContext(tab_contents()->browser_context());
      DCHECK(Source<PrefService>(source).ptr() == profile->GetPrefs());
      if (*pref_name_in == prefs::kRemoteAccessClientFirewallTraversal) {
        UpdateFirewallTraversalState();
      } else {
        NOTREACHED() << "unexpected pref change notification" << *pref_name_in;
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void FirewallTraversalObserver::UpdateFirewallTraversalState() {
  const char* pref_name = prefs::kRemoteAccessClientFirewallTraversal;
  Profile* profile =
      Profile::FromBrowserContext(tab_contents()->browser_context());
  bool enabled = profile->GetPrefs()->GetBoolean(pref_name);

  DictionaryValue value;
  value.SetBoolean(pref_name, enabled);
  std::string policy;
  base::JSONWriter::Write(&value, false, &policy);
  Send(new ViewMsg_UpdateRemoteAccessClientFirewallTraversal(routing_id(),
                                                             policy));
}

bool FirewallTraversalObserver::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(FirewallTraversalObserver, msg)
    IPC_MESSAGE_HANDLER(ViewHostMsg_RequestRemoteAccessClientFirewallTraversal,
                        UpdateFirewallTraversalState)

    // Have the super handle all other messages.
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}
