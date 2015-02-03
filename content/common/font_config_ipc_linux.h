// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FONT_CONFIG_IPC_LINUX_H_
#define CONTENT_COMMON_FONT_CONFIG_IPC_LINUX_H_

#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/synchronization/lock.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/ports/SkFontConfigInterface.h"

#include <string>

class SkString;

namespace content {

// FontConfig implementation for Skia that proxies out of process to get out
// of the sandbox. See http://code.google.com/p/chromium/wiki/LinuxSandboxIPC
class FontConfigIPC : public SkFontConfigInterface {
 public:
  explicit FontConfigIPC(int fd);
  ~FontConfigIPC() override;

  bool matchFamilyName(const char familyName[],
                       SkTypeface::Style requested,
                       FontIdentity* outFontIdentifier,
                       SkString* outFamilyName,
                       SkTypeface::Style* outStyle) override;

  SkStreamAsset* openStream(const FontIdentity&) override;

  enum Method {
    METHOD_MATCH = 0,
    METHOD_OPEN = 1,
  };

  enum {
    kMaxFontFamilyLength = 2048
  };

 private:
  class MappedFontFile;

  // Removes |mapped_font_file| from |mapped_font_files_|.
  // Does not delete the passed-in object.
  void RemoveMappedFontFile(MappedFontFile* mapped_font_file);

  const int fd_;
  // Lock preventing multiple threads from opening font file and accessing
  // |mapped_font_files_| map at the same time.
  base::Lock lock_;
  // Maps font identity ID to the memory-mapped file with font data.
  base::hash_map<uint32_t, MappedFontFile*> mapped_font_files_;
};

}  // namespace content

#endif  // CONTENT_COMMON_FONT_CONFIG_IPC_LINUX_H_
