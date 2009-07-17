// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_blacklist/blacklist_store.h"

#include <cstdio>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/logging.h"

namespace {

const char cookie[] = "GCPBL100";

}

void BlacklistStoreOutput::WriteUInt(uint32 i) {
  if (fwrite(reinterpret_cast<char*>(&i), 1, sizeof(uint32), file_) !=
      sizeof(uint32)) {
    LOG(ERROR) << "fwrite failed to write uint32";
  }
}

void BlacklistStoreOutput::WriteString(const std::string& s) {
  uint32 n = s.size();
  if (fwrite(reinterpret_cast<char*>(&n), 1, sizeof(uint32), file_) !=
      sizeof(uint32)) {
    LOG(ERROR) << "fwrite failed to write string size";
  }
  if (fwrite(s.c_str(), 1, s.size(), file_) != s.size())
    LOG(ERROR) << "fwrite failed to write string data";
}

BlacklistStoreOutput::BlacklistStoreOutput(FILE* file) : file_(file) {
  if (fwrite(cookie, 1, sizeof(cookie), file_) != sizeof(cookie))
    LOG(ERROR) << "fwrite failed to write cookie";
}

BlacklistStoreOutput::~BlacklistStoreOutput() {
  file_util::CloseFile(file_);
}

void BlacklistStoreOutput::ReserveProviders(uint32 num) {
  WriteUInt(num);
}

void BlacklistStoreOutput::StoreProvider(const std::string& name,
                                         const std::string& url) {
  WriteString(name);
  WriteString(url);
}

void BlacklistStoreOutput::ReserveEntries(uint32 num) {
  WriteUInt(num);
}

void BlacklistStoreOutput::StoreEntry(const std::string& pattern,
                                      uint32 attributes,
                                      const std::vector<std::string>& types,
                                      uint32 provider) {
  WriteString(pattern);
  WriteUInt(attributes);
  WriteUInt(types.size());
  for (uint32 i = 0; i < types.size(); ++i)
    WriteString(types[i]);
  WriteUInt(provider);
}

uint32 BlacklistStoreInput::ReadUInt() {
  uint32 buf;
  if (fread(&buf, 1, sizeof(uint32), file_) != sizeof(uint32))
    return 0;
  return buf;
}

std::string BlacklistStoreInput::ReadString() {
  uint32 size = ReadUInt();

  // Too long strings are not allowed.
  if (size > 8192) {
    return std::string();
  }

  char buf[8192];
  if (fread(buf, 1, size, file_) != size)
    return std::string();
  return std::string(buf, size);
}

BlacklistStoreInput::BlacklistStoreInput(FILE* file) : file_(file) {
  fseek(file_, sizeof(cookie), SEEK_CUR);
}

BlacklistStoreInput::~BlacklistStoreInput() {
  file_util::CloseFile(file_);
}

uint32 BlacklistStoreInput::ReadNumProviders() {
  return ReadUInt();
}

void BlacklistStoreInput::ReadProvider(std::string* name, std::string* url) {
  *name = ReadString();
  *url = ReadString();
}

uint32 BlacklistStoreInput::ReadNumEntries() {
  return ReadUInt();
}

void BlacklistStoreInput::ReadEntry(std::string* pattern,
                                    uint32* attributes,
                                    std::vector<std::string>* types,
                                    uint32* provider) {
  *pattern = ReadString();
  *attributes = ReadUInt();
  if (uint32 n = ReadUInt()) {
    while (n--)
      types->push_back(ReadString());
  }
  *provider = ReadUInt();
}
