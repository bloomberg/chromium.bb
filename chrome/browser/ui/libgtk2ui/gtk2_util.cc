// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtk2ui/gtk2_util.h"

#include <gtk/gtk.h>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"

namespace {

void CommonInitFromCommandLine(const CommandLine& command_line,
                               void (*init_func)(gint*, gchar***)) {
  const std::vector<std::string>& args = command_line.argv();
  int argc = args.size();
  scoped_array<char *> argv(new char *[argc + 1]);
  for (size_t i = 0; i < args.size(); ++i) {
    // TODO(piman@google.com): can gtk_init modify argv? Just being safe
    // here.
    argv[i] = strdup(args[i].c_str());
  }
  argv[argc] = NULL;
  char **argv_pointer = argv.get();

  init_func(&argc, &argv_pointer);
  for (size_t i = 0; i < args.size(); ++i) {
    free(argv[i]);
  }
}

}  // namespace

namespace libgtk2ui {

void GtkInitFromCommandLine(const CommandLine& command_line) {
  CommonInitFromCommandLine(command_line, gtk_init);
}

}  // namespace libgtk2ui
