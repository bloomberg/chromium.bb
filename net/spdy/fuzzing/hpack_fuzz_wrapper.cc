// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "net/spdy/fuzzing/hpack_fuzz_util.h"

namespace {

// Specifies a file having HPACK header sets.
const char kFileToParse[] = "file-to-parse";

}  // namespace

using base::StringPiece;
using net::HpackFuzzUtil;
using std::string;

// Sequentially runs each given length-prefixed header block through
// decoding and encoding fuzzing stages (using HpackFuzzUtil).
int main(int argc, char** argv) {
  base::AtExitManager exit_manager;

  CommandLine::Init(argc, argv);
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();

  if (!command_line.HasSwitch(kFileToParse)) {
    LOG(ERROR) << "Usage: " << argv[0]
               << " --" << kFileToParse << "=/path/to/file.in";
    return -1;
  }
  string file_to_parse = command_line.GetSwitchValueASCII(kFileToParse);

  DVLOG(1) << "Reading input from " << file_to_parse;
  HpackFuzzUtil::Input input;
  CHECK(base::ReadFileToString(base::FilePath(file_to_parse), &input.input));

  HpackFuzzUtil::FuzzerContext context;
  HpackFuzzUtil::InitializeFuzzerContext(&context);

  size_t block_count = 0;
  StringPiece block;
  while (HpackFuzzUtil::NextHeaderBlock(&input, &block)) {
    HpackFuzzUtil::RunHeaderBlockThroughFuzzerStages(&context, block);
    ++block_count;
  }
  DVLOG(1) << "Fuzzed " << block_count << " blocks.";
  return 0;
}
