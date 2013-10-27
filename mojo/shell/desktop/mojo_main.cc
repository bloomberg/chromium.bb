// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/run.h"

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);

  base::MessageLoop message_loop(base::MessageLoop::TYPE_UI);
  mojo::shell::Context context;

  message_loop.PostTask(FROM_HERE, base::Bind(mojo::shell::Run,
                                              &context));
  message_loop.Run();

  return 0;
}
