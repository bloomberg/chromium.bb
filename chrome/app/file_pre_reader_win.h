// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines a function to pre-read a file in order to avoid touching
// the disk when it is subsequently used.

#ifndef CHROME_APP_FILE_PRE_READER_WIN_H_
#define CHROME_APP_FILE_PRE_READER_WIN_H_

namespace base {
class FilePath;
}

// Reads |file_path| to avoid touching the disk when the file is actually used.
// The function checks the Windows version to determine which pre-reading
// mechanism to use. On Vista+, chunks of |step_size| bytes are read into a
// buffer. The bigger |step_size| is, the faster the file is pre-read (up to
// about 4MB according to local tests), but also the more memory is allocated
// for the buffer. On XP, pre-reading non-PE files is not supported.
bool PreReadFile(const base::FilePath& file_path, int step_size);

#endif  // CHROME_APP_FILE_PRE_READER_WIN_H_
