// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PARSERS_METADATA_PARSER_JPEG_FACTORY_H_
#define CHROME_BROWSER_PARSERS_METADATA_PARSER_JPEG_FACTORY_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/parsers/metadata_parser_factory.h"

class FilePath;

class MetadataParserJpegFactory : public MetadataParserFactory {
 public:
  MetadataParserJpegFactory();

  // Implementation of MetadataParserFactory
  virtual bool CanParse(const FilePath& path, char* bytes, int bytes_size);
  virtual MetadataParser* CreateParser(const FilePath& path);

 private:
  DISALLOW_COPY_AND_ASSIGN(MetadataParserJpegFactory);
};

#endif  // CHROME_BROWSER_PARSERS_METADATA_PARSER_JPEG_FACTORY_H_
