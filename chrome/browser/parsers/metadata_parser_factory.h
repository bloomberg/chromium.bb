// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PARSERS_METADATA_PARSER_FACTORY_H_
#define CHROME_BROWSER_PARSERS_METADATA_PARSER_FACTORY_H_
#pragma once

#include "chrome/browser/parsers/metadata_parser.h"

class FilePath;

// Used to check to see if a parser can parse a particular file, and allows
// for creation of a parser on a particular file.
class MetadataParserFactory {
 public:
  MetadataParserFactory() {}
  virtual ~MetadataParserFactory() {}

  // Used to check to see if the parser can parse the given file. This
  // should not do any additional reading of the file.
  virtual bool CanParse(const FilePath& path,
                        char* bytes,
                        int bytes_size) = 0;

  // Creates the parser on the given file.  Creating the parser does not
  // do any parsing on the file.  Parse has to be called on the parser.
  virtual MetadataParser* CreateParser(const FilePath& path) = 0;
};

#endif  // CHROME_BROWSER_PARSERS_METADATA_PARSER_FACTORY_H_
