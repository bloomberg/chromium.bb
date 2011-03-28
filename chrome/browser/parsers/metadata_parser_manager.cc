// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/parsers/metadata_parser_manager.h"

#include "base/logging.h"
#include "base/file_util.h"
#include "base/memory/singleton.h"
#include "base/stl_util-inl.h"
#include "build/build_config.h"
#include "chrome/browser/parsers/metadata_parser_factory.h"
#include "chrome/browser/parsers/metadata_parser_jpeg_factory.h"

static const int kAmountToRead = 256;

// Gets the singleton
MetadataParserManager* MetadataParserManager::GetInstance() {
  // Uses the LeakySingletonTrait because cleanup is optional.
  return Singleton<MetadataParserManager,
                   LeakySingletonTraits<MetadataParserManager> >::get();
}

bool MetadataParserManager::RegisterParserFactory(
    MetadataParserFactory* parser) {
  factories_.push_back(parser);
  return true;
}

MetadataParserManager::MetadataParserManager() {
  MetadataParserJpegFactory *factory = new MetadataParserJpegFactory();
  RegisterParserFactory(factory);
}

MetadataParserManager::~MetadataParserManager() {}

MetadataParser* MetadataParserManager::GetParserForFile(const FilePath& path) {

  char buffer[kAmountToRead];
  int amount_read = 0;
  DLOG(ERROR) << path.value();
  amount_read = file_util::ReadFile(path, buffer, sizeof(buffer));
  if (amount_read <= 0) {
    DLOG(ERROR) << "Unable to read file";
    return NULL;
  }
  for (size_t x = 0; x < factories_.size(); ++x) {
    if (factories_[x]->CanParse(path, buffer, amount_read)) {
      return factories_[x]->CreateParser(path);
    }
  }
  return NULL;
}
