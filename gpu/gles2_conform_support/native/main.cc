// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/message_loop.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

extern "C" {
#if defined(GLES2_CONFORM_SUPPORT_ONLY)
#include "gpu/gles2_conform_support/gtf/gtf_stubs.h"
#else
#include "third_party/gles2_conform/GTF_ES/glsl/GTF/Source/GTFMain.h"
#endif
}

int main(int argc, char *argv[]) {
#if defined(TOOLKIT_GTK)
  gtk_init(&argc, &argv);
#endif

  base::AtExitManager at_exit;
  MessageLoopForUI message_loop;

  GTFMain(argc, argv);

  return 0;
}

