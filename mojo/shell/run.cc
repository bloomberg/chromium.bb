// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/run.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "mojo/shell/context.h"
#include "mojo/shell/service_manager.h"
#include "mojo/shell/switches.h"
#include "url/gurl.h"

namespace mojo {
namespace shell {

void Run(Context* context) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  CommandLine::StringVector args = command_line.GetArgs();

  if (args.empty()) {
    LOG(ERROR) << "No app path specified.";
    base::MessageLoop::current()->Quit();
    return;
  }

  for (CommandLine::StringVector::const_iterator it = args.begin();
       it != args.end(); ++it) {
    GURL url(*it);
    if (url.scheme() == "mojo" && !command_line.HasSwitch(switches::kOrigin)) {
      LOG(ERROR) << "mojo: url passed with no --origin specified.";
      base::MessageLoop::current()->Quit();
      return;
    }
    ScopedMessagePipeHandle no_handle;
    context->service_manager()->Connect(GURL(*it), no_handle.Pass());
  }
  // TODO(davemoore): Currently we leak |service_manager|.
}

}  // namespace shell
}  // namespace mojo
