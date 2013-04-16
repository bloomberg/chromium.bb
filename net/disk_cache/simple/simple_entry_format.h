// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_FORMAT_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_FORMAT_H_


#include "base/basictypes.h"
#include "base/port.h"
#include "net/base/net_export.h"

namespace base {
class Time;
}

namespace disk_cache {

const uint64 kSimpleInitialMagicNumber = GG_UINT64_C(0xfcfb6d1ba7725c30);

// A file in the Simple cache consists of a SimpleFileHeader followed
// by data.

const uint32 kSimpleVersion = 1;

static const int kSimpleEntryFileCount = 3;

struct NET_EXPORT_PRIVATE SimpleFileHeader {
  SimpleFileHeader();
  uint64 initial_magic_number;
  uint32 version;
  uint32 key_length;
  uint32 key_hash;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_ENTRY_FORMAT_H_
