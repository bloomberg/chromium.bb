// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_DB_H_
#define CHROME_BROWSER_PRIVACY_BLACKLIST_BLACKLIST_DB_H_

#include <cstdio>
#include <string>
#include <vector>

#include "base/basictypes.h"

class FilePath;

// TODO(idanan): Error handling needed. I/O errors can always happen!

////////////////////////////////////////////////////////////////////////////////
//
// Blacklist Binary Storage Output Class
//
// Stores aggregate Privacy Blacklists efficiently on disk. The public
// functions below must be called in the order they are declared, as
// the input class is expected to read them in that order. The provider
// and entry output functions must be called the number of times set.
//
////////////////////////////////////////////////////////////////////////////////
class BlacklistStoreOutput {
 public:
  explicit BlacklistStoreOutput(FILE* file);
  ~BlacklistStoreOutput();

  // Sets the number of providers stored.
  void ReserveProviders(uint32);

  // Stores a provider.
  void StoreProvider(const std::string& name, const std::string& url);

  // Sets the number of entries stored.
  void ReserveEntries(uint32);

  // Stores an entry.
  void StoreEntry(const std::string& pattern,
                  uint32 attributes,
                  const std::vector<std::string>& types,
                  uint32 provider);

 private:
  // Writes basic types to the stream.
  void WriteUInt(uint32);
  void WriteString(const std::string&);

  FILE* file_;
  DISALLOW_COPY_AND_ASSIGN(BlacklistStoreOutput);
};

////////////////////////////////////////////////////////////////////////////////
//
// Blacklist Binary Storage Input Class
//
// Stores aggregate Privacy Blacklists efficiently on disk. The public
// functions below must be called in the order they are declared, as
// the output class is expected to write them in that order. The provider
// entries read functions must be called the correct number of times.
//
////////////////////////////////////////////////////////////////////////////////
class BlacklistStoreInput {
 public:
  explicit BlacklistStoreInput(FILE* file);
  ~BlacklistStoreInput();

  // Reads the number of providers.
  uint32 ReadNumProviders();

  // Reads a provider.
  void ReadProvider(std::string* name, std::string* url);

  // Reads the number of entries.
  uint32 ReadNumEntries();

  // Reads an entry.
  void ReadEntry(std::string* pattern,
                 uint32* attributes,
                 std::vector<std::string>* types,
                 uint32* provider);

 private:
  uint32 ReadUInt();
  std::string ReadString();

  FILE* file_;
  DISALLOW_COPY_AND_ASSIGN(BlacklistStoreInput);
};

#endif
