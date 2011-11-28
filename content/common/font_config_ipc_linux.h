// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FONT_CONFIG_IPC_LINUX_H_
#define CONTENT_COMMON_FONT_CONFIG_IPC_LINUX_H_
#pragma once

#include "base/compiler_specific.h"
#include "skia/ext/SkFontHost_fontconfig_impl.h"

#include <string>

// FontConfig implementation for Skia that proxies out of process to get out
// of the sandbox. See http://code.google.com/p/chromium/wiki/LinuxSandboxIPC
class FontConfigIPC : public FontConfigInterface {
 public:
  explicit FontConfigIPC(int fd);
  virtual ~FontConfigIPC();

  // FontConfigInterface implementation.
  virtual bool Match(std::string* result_family,
                     unsigned* result_filefaceid,
                     bool filefaceid_valid,
                     unsigned filefaceid,
                     const std::string& family,
                     const void* characters,
                     size_t characters_bytes,
                     bool* is_bold, bool* is_italic) OVERRIDE;
  virtual int Open(unsigned filefaceid) OVERRIDE;

  enum Method {
    METHOD_MATCH = 0,
    METHOD_OPEN = 1,
  };

 private:
  const int fd_;
};

#endif  // CONTENT_COMMON_FONT_CONFIG_IPC_LINUX_H_
