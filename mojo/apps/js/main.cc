// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu_util.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/apps/js/application_delegate_impl.h"
#include "mojo/public/c/system/main.h"

MojoResult MojoMain(MojoHandle shell_handle) {
  base::i18n::InitializeICU();
  mojo::ApplicationRunnerChromium runner(
      new mojo::apps::ApplicationDelegateImpl());
  return runner.Run(shell_handle);
}
