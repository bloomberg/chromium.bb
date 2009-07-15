// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This tool manages privacy blacklists. Primarily for loading a text
// blacklist into the binary aggregate blacklist.
#include <iostream>

#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/browser/privacy_blacklist/blacklist.h"
#include "chrome/browser/privacy_blacklist/blacklist_io.h"

#ifdef OS_POSIX
#define ICHAR char
#define ICERR std::cerr
#define IMAIN main
#else
#define ICHAR wchar_t
#define ICERR std::wcerr
#define IMAIN wmain
#endif

namespace {

int PrintUsage(int argc, ICHAR* argv[]) {
  ICERR << "Usage: " << argv[0] << " <source> <target>\n"
           "       <source> is the text blacklist (.pbl) to load.\n"
           "       <target> is the binary output blacklist repository.\n\n"
           "Adds all entries from <source> to <target>.\n"
           "Creates <target> if it does not exist.\n";
  return 1;
}

}

int IMAIN(int argc, ICHAR* argv[]) {
  base::EnableTerminationOnHeapCorruption();

  if (argc < 3)
    return PrintUsage(argc, argv);

  FilePath input(argv[1]);
  FilePath output(argv[2]);

  BlacklistIO io;
  if (io.Read(input)) {
    if (io.Write(output)) {
      return 0;
    } else {
      ICERR << "Error writing output file " << argv[2] << "\n";
    }
  } else {
    ICERR << "Error reading input file " << argv[1] << "\n";
  }
  return -1;
}
