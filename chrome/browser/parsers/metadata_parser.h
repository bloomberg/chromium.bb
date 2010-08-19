// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PARSERS_METADATA_PARSER_H_
#define CHROME_BROWSER_PARSERS_METADATA_PARSER_H_
#pragma once

#include <string>

class FilePath;

// Allows for Iteration on the Properties of a given file.
class MetadataPropertyIterator {
 public:
  MetadataPropertyIterator() {}
  virtual ~MetadataPropertyIterator() {}


  // Gets the next Property in the iterator.  Returns false if at the end
  // of the list.
  virtual bool GetNext(std::string* key, std::string* value) = 0;

  // Gets the number of Properties in this iterator.
  virtual int Length() = 0;

  // Checks to see if we're at the end of the list.
  virtual bool IsEnd() = 0;
};

// Represents a single instance of parsing on a particular file.
class MetadataParser {
 public:
  explicit MetadataParser(const FilePath& path) {}
  virtual ~MetadataParser() {}


  static const char* kPropertyType;
  static const char* kPropertyFilesize;
  static const char* kPropertyTitle;

  // Does all the heavy work of parsing out the file. Blocking until complete.
  virtual bool Parse() = 0;

  // Gets a particular property found in a parse call.
  virtual bool GetProperty(const std::string& key, std::string* value) = 0;

  // Gets an interator allowing you to iterate over all the properties found
  // in a parse call.
  virtual MetadataPropertyIterator* GetPropertyIterator() = 0;
};

#endif  // CHROME_BROWSER_PARSERS_METADATA_PARSER_H_
