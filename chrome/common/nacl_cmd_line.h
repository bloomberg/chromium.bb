// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NACL_CMD_LINE_H_
#define CHROME_COMMON_NACL_CMD_LINE_H_
#pragma once

class CommandLine;

namespace nacl {
  // Copy all the relevant arguments from the command line of the current
  // process to cmd_line that will be used for launching the NaCl loader/broker.
  void CopyNaClCommandLineArguments(CommandLine* cmd_line);
}

#endif  // CHROME_COMMON_NACL_CMD_LINE_H_
