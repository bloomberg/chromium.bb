// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PARSERS_METADATA_PARSER_MANAGER_H_
#define CHROME_BROWSER_PARSERS_METADATA_PARSER_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"

class MetadataParserFactory;
class FilePath;
class MetadataParser;

// Metadata Parser manager is used to find the correct parser for a
// given file.  Allows parsers to register themselves.
class MetadataParserManager {
 public:
  // Creates a new MetadataParserManager.
  MetadataParserManager();
  ~MetadataParserManager();

  // Gets the singleton
  static MetadataParserManager* GetInstance();

  // Adds a new Parser to the manager, when requests come in for a parser
  // the manager will loop through the list of parsers, and query each.
  bool RegisterParserFactory(MetadataParserFactory* parser);

  // Returns a new metadata parser for a given file.
  MetadataParser* GetParserForFile(const FilePath& path);

 private:
  ScopedVector<MetadataParserFactory> factories_;

  DISALLOW_COPY_AND_ASSIGN(MetadataParserManager);
};

#endif  // CHROME_BROWSER_PARSERS_METADATA_PARSER_MANAGER_H_
