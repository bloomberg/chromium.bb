// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_COMPRESS_DATA_HELPER_H_
#define CHROME_BROWSER_SESSIONS_COMPRESS_DATA_HELPER_H_
#pragma once

#include "base/basictypes.h"

#include <string>

class Pickle;
class PickleIterator;

class CompressDataHelper {
 public:
  // Compresses and writes |str| into |pickle|. |str| may contain NULL
  // charaters.
  static void CompressAndWriteStringToPickle(const std::string& str,
                                             int max_bytes,
                                             Pickle* pickle,
                                             int* bytes_written);

  // Reads and decompresses a string from |pickle| and saves it to |str|. |iter|
  // indicates the position of the data. The same iterator is used by
  // Pickle::Read* functions.
  static bool ReadAndDecompressStringFromPickle(const Pickle& pickle,
                                                PickleIterator* iter,
                                                std::string* str);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CompressDataHelper);
};

#endif  // CHROME_BROWSER_SESSIONS_COMPRESS_DATA_HELPER_H_
