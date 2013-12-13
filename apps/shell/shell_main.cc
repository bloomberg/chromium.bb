// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/shell_main_delegate.h"
#include "content/public/app/content_main.h"

int main(int argc, const char** argv) {
  apps::ShellMainDelegate delegate;
  return content::ContentMain(argc, argv, &delegate);
}
