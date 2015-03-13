// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_FIREWALL_HOLE_H_
#define CHROMEOS_NETWORK_FIREWALL_HOLE_H_

#include <stdint.h>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace dbus {
class FileDescriptor;
}

namespace chromeos {

// This class works with the Chrome OS permission broker to open a port in the
// system firewall. It is closed on destruction.
class CHROMEOS_EXPORT FirewallHole {
 public:
  enum class PortType {
    UDP,
    TCP,
  };

  typedef base::Callback<void(scoped_ptr<FirewallHole>)> OpenCallback;

  // This provides a simple way to pass around file descriptors since they must
  // be closed on a thread that is allowed to perform I/O.
  struct FileDescriptorDeleter {
    void CHROMEOS_EXPORT operator()(dbus::FileDescriptor* fd);
  };
  typedef scoped_ptr<dbus::FileDescriptor, FileDescriptorDeleter>
      ScopedFileDescriptor;

  // Opens a port on the system firewall for the given network interface (or all
  // interfaces if |interface| is ""). The hole will be closed when the object
  // provided to the callback is destroyed.
  static void Open(PortType type,
                   uint16_t port,
                   const std::string& interface,
                   const OpenCallback& callback);

  ~FirewallHole();

 private:
  static void RequestPortAccess(PortType type,
                                uint16_t port,
                                const std::string& interface,
                                ScopedFileDescriptor lifeline_local,
                                ScopedFileDescriptor lifeline_remote,
                                const OpenCallback& callback);

  static void PortAccessGranted(PortType type,
                                uint16_t port,
                                const std::string& interface,
                                ScopedFileDescriptor lifeline_fd,
                                const FirewallHole::OpenCallback& callback,
                                bool success);

  FirewallHole(PortType type,
               uint16_t port,
               const std::string& interface,
               ScopedFileDescriptor lifeline_fd);

  const PortType type_;
  const uint16_t port_;
  const std::string interface_;

  // A file descriptor used by firewalld to track the lifetime of this process.
  ScopedFileDescriptor lifeline_fd_;
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_FIREWALL_HOLE_H_
