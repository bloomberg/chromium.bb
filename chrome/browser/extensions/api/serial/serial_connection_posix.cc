// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_connection.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string>
#include <termios.h>
#include <unistd.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

SerialConnection::SerialConnection(const std::string& port,
                                   APIResourceEventNotifier* event_notifier)
    : APIResource(APIResource::SerialConnectionResource, event_notifier),
      port_(port), fd_(0) {
}

SerialConnection::~SerialConnection() {
}

bool SerialConnection::Open() {
  const StringSet port_patterns(GenerateValidSerialPortNames());
  if (!DoesPortExist(port_patterns, port_)) {
    return false;
  }
  fd_ = open(port_.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
  return (fd_ > 0);
}

void SerialConnection::Close() {
  if (fd_ > 0) {
    close(fd_);
    fd_ = 0;
  }
}

int SerialConnection::Read(uint8* byte) {
  return read(fd_, byte, 1);
}

int SerialConnection::Write(scoped_refptr<net::IOBuffer> io_buffer,
                            int byte_count) {
  return write(fd_, io_buffer.get(), byte_count);
}

// static
//
// TODO(miket): this is starting to look like a
// SerialConnectionCollectionFinderManagerSingleton. Can it be extracted, then
// owned by someone like APIResourceController?
SerialConnection::StringSet SerialConnection::GenerateValidPatterns() {
  // TODO(miket): the set of patterns should be larger. See the rxtx project.
  //
  // TODO(miket): The list of patterns tested at runtime should also be
  // OS-dependent.
  const char* VALID_PATTERNS[] = {
    "/dev/*Bluetooth*",
    "/dev/*Modem*",
    "/dev/*bluetooth*",
    "/dev/*modem*",
    "/dev/*serial*",
    "/dev/ttyACM*",
    "/dev/ttyS*",
    "/dev/ttyUSB*",
  };

  StringSet valid_patterns;
  for (size_t i = 0; i < arraysize(VALID_PATTERNS); ++i)
    valid_patterns.insert(VALID_PATTERNS[i]);

  return valid_patterns;
}

// static
//
// TODO(miket): Investigate udev. Search for equivalent solutions on OSX.
// Continue to examine rxtx code.
//
// On a fairly ordinary Linux machine, ls -l /dev | wc -l returned about 200
// items. So we're doing about N(VALID_PATTERNS) * 200 = 1,600 MatchPattern
// calls to find maybe a dozen serial ports in a small number of milliseconds
// (a single trial of SerialConnectionTest.ValidPortNames took 6ms to run).
// It's not cheap, but then again, we don't expect users of this API to be
// enumerating too often (at worst on every click of a UI element that displays
// the generated list).
//
// An upside-down approach would instead take each pattern and turn it into a
// string generator (something like /dev/ttyS[0-9]{0,3}) and then expanding
// that into a series of possible paths, perhaps early-outing if we knew that
// port patterns were contiguous (e.g., /dev/ttyS1 can't exist if /dev/ttyS0
// doesn't exist).
//
// Caching seems undesirable. Many devices can be dynamically added to and
// removed from the system, so we really do want to regenerate the set each
// time.
//
// TODO(miket): this might be refactorable into serial_connection.cc, if
// Windows serial-port enumeration also entails looking through a directory.
SerialConnection::StringSet SerialConnection::GenerateValidSerialPortNames() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const FilePath DEV_ROOT("/dev");
  const file_util::FileEnumerator::FileType FILES_AND_SYM_LINKS =
      static_cast<file_util::FileEnumerator::FileType>(
          file_util::FileEnumerator::FILES |
          file_util::FileEnumerator::SHOW_SYM_LINKS);

  StringSet valid_patterns = GenerateValidPatterns();
  StringSet name_set;
  file_util::FileEnumerator enumerator(
      DEV_ROOT, false, FILES_AND_SYM_LINKS);
  do {
    const FilePath next_device_path(enumerator.Next());
    const std::string next_device = next_device_path.value();
    if (next_device.empty())
      break;

    std::set<std::string>::const_iterator i = valid_patterns.begin();
    for (; i != valid_patterns.end(); ++i) {
      if (MatchPattern(next_device, *i)) {
        name_set.insert(next_device);
        break;
      }
    }
  } while (true);

  return name_set;
}

}  // namespace extensions
