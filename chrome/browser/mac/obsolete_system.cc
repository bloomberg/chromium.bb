// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mac/obsolete_system.h"

#include <sys/sysctl.h>
#include <sys/types.h>

#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(ARCH_CPU_64_BITS)

// static
bool ObsoleteSystemMac::Has32BitOnlyCPU() {
  int value;
  size_t valueSize = sizeof(value);
  if (sysctlbyname("hw.cpu64bit_capable", &value, &valueSize, NULL, 0) != 0) {
    return true;
  }
  return value == 0;
}

// static
base::string16 ObsoleteSystemMac::LocalizedObsoleteSystemString() {
  return l10n_util::GetStringUTF16(
      Is32BitEndOfTheLine() ? IDS_MAC_32_BIT_OBSOLETE_NOW :
                              IDS_MAC_32_BIT_OBSOLETE_SOON);
}

#endif
