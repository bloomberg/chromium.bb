// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_store.h"

#include <cstdio>
#include <limits>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"

namespace {

const char cookie[] = "GCPBL100";

const size_t kMaxBlockedTypes = 256;
const size_t kMaxStringSize = 8192;

}  // namespace

bool BlacklistStoreOutput::WriteUInt(uint32 i) {
  return fwrite(reinterpret_cast<char*>(&i), 1, sizeof(uint32), file_) ==
      sizeof(uint32);
}

bool BlacklistStoreOutput::WriteString(const std::string& s) {
  uint32 n = s.size();
  return WriteUInt(n) && fwrite(s.c_str(), 1, n, file_) == n;
}

BlacklistStoreOutput::BlacklistStoreOutput(FILE* file)
    : file_(file), is_good_(false) {
  is_good_ = fwrite(cookie, 1, sizeof(cookie), file_) == sizeof(cookie);
}

BlacklistStoreOutput::~BlacklistStoreOutput() {
  file_util::CloseFile(file_);
}

bool BlacklistStoreOutput::ReserveProviders(uint32 num) {
  return WriteUInt(num);
}

bool BlacklistStoreOutput::StoreProvider(const std::string& name,
                                         const std::string& url) {
  return WriteString(name) && WriteString(url);
}

bool BlacklistStoreOutput::ReserveEntries(uint32 num) {
  return WriteUInt(num);
}

bool BlacklistStoreOutput::StoreEntry(const std::string& pattern,
                                      uint32 attributes,
                                      const std::vector<std::string>& types,
                                      uint32 provider) {
  if (WriteString(pattern) &&
      WriteUInt(attributes) &&
      WriteUInt(types.size())) {
    for (uint32 i = 0; i < types.size(); ++i) {
      if (!WriteString(types[i]))
        return false;
    }
    return WriteUInt(provider);
  }
  return false;
}

uint32 BlacklistStoreInput::ReadUInt() {
  uint32 buf;
  if (fread(&buf, 1, sizeof(uint32), file_) != sizeof(uint32))
    return std::numeric_limits<uint32>::max();
  return buf;
}

std::string BlacklistStoreInput::ReadString() {
  uint32 size = ReadUInt();

  // Too long strings are not allowed. Covers the case of ReadUInt failing.
  if (size > kMaxStringSize) {
    return std::string();
  }

  char buf[kMaxStringSize];
  if (fread(buf, 1, size, file_) != size)
    return std::string();
  return std::string(buf, size);
}

BlacklistStoreInput::BlacklistStoreInput(FILE* file)
    : file_(file), is_good_(false) {
  is_good_ = !fseek(file_, sizeof(cookie), SEEK_CUR);
}

BlacklistStoreInput::~BlacklistStoreInput() {
  file_util::CloseFile(file_);
}

uint32 BlacklistStoreInput::ReadNumProviders() {
  return ReadUInt();
}

bool BlacklistStoreInput::ReadProvider(std::string* name, std::string* url) {
  *name = ReadString();
  if (name->empty())
    return false;
  *url = ReadString();
  return !url->empty();
}

uint32 BlacklistStoreInput::ReadNumEntries() {
  return ReadUInt();
}

bool BlacklistStoreInput::ReadEntry(std::string* pattern,
                                    uint32* attributes,
                                    std::vector<std::string>* types,
                                    uint32* provider) {
  *pattern = ReadString();
  if (pattern->empty())
    return false;

  *attributes = ReadUInt();
  if (*attributes == std::numeric_limits<uint32>::max())
    return false;

  if (uint32 n = ReadUInt()) {
    if (n >= kMaxBlockedTypes)
      return false;
    while (n--) {
      std::string type = ReadString();
      if (type.empty())
        return false;
      types->push_back(type);
    }
  }
  *provider = ReadUInt();
  return *provider != std::numeric_limits<uint32>::max();
}
