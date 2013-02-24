// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/parsers/metadata_parser_filebase.h"

#include "base/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"

FileMetadataParser::FileMetadataParser(const base::FilePath& path)
    : MetadataParser(path),
      path_(path) {
}

FileMetadataParser::~FileMetadataParser() {}

bool FileMetadataParser::Parse() {
  std::string value;
  int64 size;
  if (file_util::GetFileSize(path_, &size)) {
    properties_[MetadataParser::kPropertyFilesize] = base::Int64ToString(size);
  }
#if defined(OS_WIN)
  value = base::WideToUTF8(path_.BaseName().value());
  properties_[MetadataParser::kPropertyTitle] = value;
#elif defined(OS_POSIX)
  properties_[MetadataParser::kPropertyTitle] = path_.BaseName().value();
#endif
  return true;
}

bool FileMetadataParser::GetProperty(const std::string& key,
                                     std::string* value) {
  PropertyMap::iterator it = properties_.find(key.c_str());
  if (it == properties_.end()) {
    return false;
  }

  *value = properties_[key.c_str()];
  return true;
}

MetadataPropertyIterator* FileMetadataParser::GetPropertyIterator() {
  return new FileMetadataPropertyIterator(properties_);
}

FileMetadataPropertyIterator::FileMetadataPropertyIterator(
    PropertyMap& properties) : properties_(properties) {
  it = properties_.begin();
}

FileMetadataPropertyIterator::~FileMetadataPropertyIterator() {}

bool FileMetadataPropertyIterator::GetNext(std::string* key,
                                           std::string* value) {
  if (it == properties_.end()) {
    return false;
  }
  *key = it->first;
  *value = it->second;
  it++;
  return true;
}

int FileMetadataPropertyIterator::Length() {
  return properties_.size();
}

bool FileMetadataPropertyIterator::IsEnd() {
  if (it == properties_.end()) {
    return true;
  } else {
    return false;
  }
}
