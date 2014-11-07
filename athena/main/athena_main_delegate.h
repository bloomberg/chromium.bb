// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_MAIN_ATHENA_MAIN_DELEGATE_H_
#define ATHENA_MAIN_ATHENA_MAIN_DELEGATE_H_

#include "extensions/shell/app/shell_main_delegate.h"

namespace athena {

class AthenaMainDelegate : public extensions::ShellMainDelegate {
 public:
  AthenaMainDelegate() {}
  ~AthenaMainDelegate() override {}

 private:
  // extensions::ShellMainDelegate:
  content::ContentClient* CreateContentClient() override;
  content::ContentBrowserClient* CreateShellContentBrowserClient() override;
  content::ContentRendererClient* CreateShellContentRendererClient() override;
  void InitializeResourceBundle() override;

  DISALLOW_COPY_AND_ASSIGN(AthenaMainDelegate);
};

}  // namespace

#endif  // ATHENA_MAIN_ATHENA_MAIN_DELEGATE_H_
