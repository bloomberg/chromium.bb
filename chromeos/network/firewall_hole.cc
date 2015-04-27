// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/firewall_hole.h"

#include <fcntl.h>
#include <unistd.h>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/worker_pool.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/permission_broker_client.h"
#include "dbus/file_descriptor.h"

namespace chromeos {

namespace {

// Creates a pair of file descriptors that form a "lifeline" between Chrome and
// firewalld. If this pipe is closed unexpectedly (i.e. Chrome crashes) then
// firewalld will notice and close the hole in the firewall.
void CreateValidLifeline(dbus::FileDescriptor* lifeline_local,
                         dbus::FileDescriptor* lifeline_remote) {
  int lifeline[2] = {-1, -1};
  if (pipe2(lifeline, O_CLOEXEC) < 0) {
    PLOG(ERROR) << "Failed to create a lifeline pipe";
    return;
  }

  lifeline_local->PutValue(lifeline[0]);
  lifeline_local->CheckValidity();

  lifeline_remote->PutValue(lifeline[1]);
  lifeline_remote->CheckValidity();
}

const char* PortTypeToString(FirewallHole::PortType type) {
  switch (type) {
    case FirewallHole::PortType::TCP:
      return "TCP";
    case FirewallHole::PortType::UDP:
      return "UDP";
  }
  NOTREACHED();
  return nullptr;
}

void PortReleased(FirewallHole::PortType type,
                  uint16_t port,
                  const std::string& interface,
                  dbus::ScopedFileDescriptor lifeline_fd,
                  bool success) {
  if (!success) {
    LOG(WARNING) << "Failed to release firewall hole for "
                 << PortTypeToString(type) << " port " << port << " on "
                 << interface << ".";
  }
}

}  // namespace

// static
void FirewallHole::Open(PortType type,
                        uint16_t port,
                        const std::string& interface,
                        const OpenCallback& callback) {
  dbus::ScopedFileDescriptor lifeline_local(new dbus::FileDescriptor());
  dbus::ScopedFileDescriptor lifeline_remote(new dbus::FileDescriptor());

  // This closure shares pointers with the one below. PostTaskAndReply
  // guarantees that it will always be deleted first.
  base::Closure create_lifeline_closure = base::Bind(
      &CreateValidLifeline, lifeline_local.get(), lifeline_remote.get());

  base::WorkerPool::PostTaskAndReply(
      FROM_HERE, create_lifeline_closure,
      base::Bind(&FirewallHole::RequestPortAccess, type, port, interface,
                 base::Passed(&lifeline_local), base::Passed(&lifeline_remote),
                 callback),
      false);
}

FirewallHole::~FirewallHole() {
  base::Callback<void(bool)> port_released_closure = base::Bind(
      &PortReleased, type_, port_, interface_, base::Passed(&lifeline_fd_));

  PermissionBrokerClient* client =
      DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";
  switch (type_) {
    case PortType::TCP:
      client->ReleaseTcpPort(port_, interface_, port_released_closure);
      return;
    case PortType::UDP:
      client->ReleaseUdpPort(port_, interface_, port_released_closure);
      return;
  }
}

void FirewallHole::RequestPortAccess(PortType type,
                                     uint16_t port,
                                     const std::string& interface,
                                     dbus::ScopedFileDescriptor lifeline_local,
                                     dbus::ScopedFileDescriptor lifeline_remote,
                                     const OpenCallback& callback) {
  if (!lifeline_local->is_valid() || !lifeline_remote->is_valid()) {
    callback.Run(nullptr);
    return;
  }

  base::Callback<void(bool)> access_granted_closure =
      base::Bind(&FirewallHole::PortAccessGranted, type, port, interface,
                 base::Passed(&lifeline_local), callback);

  PermissionBrokerClient* client =
      DBusThreadManager::Get()->GetPermissionBrokerClient();
  DCHECK(client) << "Could not get permission broker client.";

  switch (type) {
    case PortType::TCP:
      client->RequestTcpPortAccess(port, interface, *lifeline_remote,
                                   access_granted_closure);
      return;
    case PortType::UDP:
      client->RequestUdpPortAccess(port, interface, *lifeline_remote,
                                   access_granted_closure);
      return;
  }
}

void FirewallHole::PortAccessGranted(PortType type,
                                     uint16_t port,
                                     const std::string& interface,
                                     dbus::ScopedFileDescriptor lifeline_fd,
                                     const FirewallHole::OpenCallback& callback,
                                     bool success) {
  if (success) {
    callback.Run(make_scoped_ptr(
        new FirewallHole(type, port, interface, lifeline_fd.Pass())));
  } else {
    callback.Run(nullptr);
  }
}

FirewallHole::FirewallHole(PortType type,
                           uint16_t port,
                           const std::string& interface,
                           dbus::ScopedFileDescriptor lifeline_fd)
    : type_(type),
      port_(port),
      interface_(interface),
      lifeline_fd_(lifeline_fd.Pass()) {
}

}  // namespace chromeos
