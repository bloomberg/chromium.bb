// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/sync_notifier_factory.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/sync/notifier/non_blocking_invalidation_notifier.h"
#include "chrome/browser/sync/notifier/p2p_notifier.h"
#include "chrome/browser/sync/notifier/sync_notifier.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "jingle/notifier/base/const_communicator.h"
#include "jingle/notifier/base/notifier_options.h"
#include "jingle/notifier/listener/mediator_thread_impl.h"
#include "jingle/notifier/listener/talk_mediator_impl.h"
#include "net/base/host_port_pair.h"

using content::BrowserThread;

namespace sync_notifier {
namespace {

// TODO(akalin): Figure out whether this should be a method of
// HostPortPair.
net::HostPortPair StringToHostPortPair(const std::string& host_port_str,
                                       uint16 default_port) {
  std::string::size_type colon_index = host_port_str.find(':');
  if (colon_index == std::string::npos) {
    return net::HostPortPair(host_port_str, default_port);
  }

  std::string host = host_port_str.substr(0, colon_index);
  std::string port_str = host_port_str.substr(colon_index + 1);
  int port = default_port;
  if (!base::StringToInt(port_str, &port) ||
      (port <= 0) || (port > kuint16max)) {
    LOG(WARNING) << "Could not parse valid port from " << port_str
                 << "; using port " << default_port;
    return net::HostPortPair(host, default_port);
  }

  return net::HostPortPair(host, port);
}

SyncNotifier* CreateDefaultSyncNotifier(
    const CommandLine& command_line,
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    const InvalidationVersionMap& initial_max_invalidation_versions,
    const browser_sync::WeakHandle<InvalidationVersionTracker>&
        invalidation_version_tracker,
    const std::string& client_info) {
  // Contains options specific to how sync clients send and listen to
  // jingle notifications.
  notifier::NotifierOptions notifier_options;
  notifier_options.request_context_getter = request_context_getter;

  // Override the notification server host from the command-line, if provided.
  if (command_line.HasSwitch(switches::kSyncNotificationHost)) {
    std::string value(command_line.GetSwitchValueASCII(
        switches::kSyncNotificationHost));
    if (!value.empty()) {
      notifier_options.xmpp_host_port =
          StringToHostPortPair(value, notifier::kDefaultXmppPort);
    }
    DVLOG(1) << "Using " << notifier_options.xmpp_host_port.ToString()
             << " for test sync notification server.";
  }

  notifier_options.try_ssltcp_first =
      command_line.HasSwitch(switches::kSyncTrySsltcpFirstForXmpp);
  if (notifier_options.try_ssltcp_first)
    DVLOG(1) << "Trying SSL/TCP port before XMPP port for notifications.";

  notifier_options.invalidate_xmpp_login =
      command_line.HasSwitch(switches::kSyncInvalidateXmppLogin);
  if (notifier_options.invalidate_xmpp_login) {
    DVLOG(1) << "Invalidating sync XMPP login.";
  }

  notifier_options.allow_insecure_connection =
      command_line.HasSwitch(switches::kSyncAllowInsecureXmppConnection);
  if (notifier_options.allow_insecure_connection) {
    DVLOG(1) << "Allowing insecure XMPP connections.";
  }

  if (command_line.HasSwitch(switches::kSyncNotificationMethod)) {
    const std::string notification_method_str(
        command_line.GetSwitchValueASCII(switches::kSyncNotificationMethod));
    notifier_options.notification_method =
        notifier::StringToNotificationMethod(notification_method_str);
  }

  if (notifier_options.notification_method == notifier::NOTIFICATION_P2P) {
    notifier::TalkMediator* const talk_mediator =
        new notifier::TalkMediatorImpl(
            new notifier::MediatorThreadImpl(notifier_options),
            notifier_options);
    // TODO(rlarocque): Ideally, the notification target would be
    // NOTIFY_OTHERS.  There's no good reason to notify ourselves of our own
    // commits.  We self-notify for now only because the integration tests rely
    // on this behaviour.  See crbug.com/97780.
    //
    // Takes ownership of |talk_mediator|.
    return new P2PNotifier(talk_mediator, NOTIFY_ALL);
  }

  return new NonBlockingInvalidationNotifier(
      notifier_options, initial_max_invalidation_versions,
      invalidation_version_tracker, client_info);
}
}  // namespace

SyncNotifierFactory::SyncNotifierFactory(
    const std::string& client_info,
    const scoped_refptr<net::URLRequestContextGetter>&
        request_context_getter,
    const base::WeakPtr<InvalidationVersionTracker>&
        invalidation_version_tracker,
    const CommandLine& command_line)
    : client_info_(client_info),
      request_context_getter_(request_context_getter),
      initial_max_invalidation_versions_(
          invalidation_version_tracker.get() ?
          invalidation_version_tracker->GetAllMaxVersions() :
          InvalidationVersionMap()),
      invalidation_version_tracker_(invalidation_version_tracker),
      command_line_(command_line) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

SyncNotifierFactory::~SyncNotifierFactory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

SyncNotifier* SyncNotifierFactory::CreateSyncNotifier() {
  return CreateDefaultSyncNotifier(command_line_,
                                   request_context_getter_,
                                   initial_max_invalidation_versions_,
                                   invalidation_version_tracker_,
                                   client_info_);
}
}  // namespace sync_notifier
