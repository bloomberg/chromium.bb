// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines utility functions to pre-read a PE Image in order to
// avoid hard page faults when the image is subsequently loaded into memory
// for execution.

#ifndef CHROME_APP_IMAGE_PRE_READER_WIN_H_
#define CHROME_APP_IMAGE_PRE_READER_WIN_H_

#include "base/basictypes.h"

// This class defines static helper functions to pre-read a PE Image in order
// to avoid hard page faults when the image is subsequently loaded into memory
// for execution.
class ImagePreReader {
 public:
  // Reads the file passed in as a PE Image and touches pages to avoid
  // subsequent hard page faults during LoadLibrary. The size to be pre-read
  // is passed in. If it is 0 then the whole file is paged in. The step size
  // which indicates the number of bytes to skip after every page touched is
  // also passed in.
  //
  // This function checks the Windows version to determine which pre-reading
  // mechanism to use.
  static bool PreReadImage(const wchar_t* file_path,
                           size_t size_to_read,
                           size_t step_size);

  // Loads the file passed in as PE Image and touches a percentage of the
  // pages in each of the image's sections to avoid subsequent hard page
  // faults during LoadLibrary.
  //
  // This function checks the Windows version to determine which pre-reading
  // mechanism to use.
  //
  // The percentage of the file to be read is an integral value between 0 and
  // 100, inclusive. If it is 0 then this is a NOP, if it is 100 (or greater)
  // then the whole file is paged in sequentially via PreReadImage. Otherwise,
  // for each section, in order, the given percentage of the blocks in that
  // section are paged in, starting at the beginning of each section. For
  // example: if percentage is 30 and there is a .text section and a .data
  // section, then the first 30% of .text will be paged and the first 30% of
  // .data will be paged in.
  //
  // The max_chunk_size indicates the number of bytes to read off the disk in
  // each step (for Vista and greater, where this is the way the pages are
  // warmed).
  //
  // This function is intended to be used in the context of a PE image with
  // an optimized layout, such that the blocks in each section are arranged
  // with the data and code most needed for startup moved to the front.
  // See also: http://code.google.com/p/chromium/issues/detail?id=98508
  static bool PartialPreReadImage(const wchar_t* file_path,
                                  size_t percentage,
                                  size_t max_chunk_size);

  // Helper function used by PartialPreReadImage on Windows versions (Vista+)
  // where reading through the file on disk serves to warm up the page cache.
  // Exported for unit-testing purposes.
  static bool PartialPreReadImageOnDisk(const wchar_t* file_path,
                                        size_t percentage,
                                        size_t max_chunk_size);

  // Helper function used by PartialPreReadImage on Windows versions (XP) where
  // cheaply loading the image then stepping through its address space serves
  // to warm up the page cache. Exported for unit-testing purposes.
  static bool PartialPreReadImageInMemory(const wchar_t* file_path,
                                          size_t percentage);
};  // namespace internal

#endif  // CHROME_APP_IMAGE_PRE_READER_WIN_H_
