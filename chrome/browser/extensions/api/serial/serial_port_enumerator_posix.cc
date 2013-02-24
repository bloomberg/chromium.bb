// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/serial/serial_port_enumerator.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/string_util.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace extensions {

// static
SerialPortEnumerator::StringSet SerialPortEnumerator::GenerateValidPatterns() {
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
// (a single trial of SerialPortEnumeratorTest.ValidPortNames took 6ms to run).
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
SerialPortEnumerator::StringSet
SerialPortEnumerator::GenerateValidSerialPortNames() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  const base::FilePath kDevRoot("/dev");
  const int kFilesAndSymLinks =
      file_util::FileEnumerator::FILES |
      file_util::FileEnumerator::SHOW_SYM_LINKS;

  StringSet valid_patterns = GenerateValidPatterns();
  StringSet name_set;
  file_util::FileEnumerator enumerator(
      kDevRoot, false, kFilesAndSymLinks);
  do {
    const base::FilePath next_device_path(enumerator.Next());
    const std::string next_device = next_device_path.value();
    if (next_device.empty())
      break;

    StringSet::const_iterator i = valid_patterns.begin();
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
