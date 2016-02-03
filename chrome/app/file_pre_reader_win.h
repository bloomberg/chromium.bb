// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a function to pre-read a file in order to avoid touching
// the disk when it is subsequently used.

#ifndef CHROME_APP_FILE_PRE_READER_WIN_H_
#define CHROME_APP_FILE_PRE_READER_WIN_H_

namespace base {
class FilePath;
class MemoryMappedFile;
}

// Reads |file_path| to avoid touching the disk when the file is actually used.
void PreReadFile(const base::FilePath& file_path);

// Reads |memory_mapped_file| to avoid touching the disk when the mapped file is
// actually used. The function checks the Windows version to determine which
// pre-reading mechanism to use. On Win8+, it uses ::PrefetchVirtualMemory. On
// previous Windows versions, is uses PreReadFile (declared above). |file_path|
// is the path to the memory mapped file.
void PreReadMemoryMappedFile(const base::MemoryMappedFile& memory_mapped_file,
                             const base::FilePath& file_path);

#endif  // CHROME_APP_FILE_PRE_READER_WIN_H_
