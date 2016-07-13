// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SHELL_HANDLER_IMPL_WIN_H_
#define CHROME_UTILITY_SHELL_HANDLER_IMPL_WIN_H_

#include "base/macros.h"
#include "chrome/common/shell_handler_win.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

// Implements the ShellHandler mojo interface.
class ShellHandlerImpl : public mojom::ShellHandler {
 public:
  static void Create(mojom::ShellHandlerRequest request);

 private:
  explicit ShellHandlerImpl(mojom::ShellHandlerRequest request);
  ~ShellHandlerImpl() override;

  // mojom::ShellHandler:
  void IsPinnedToTaskbar(const IsPinnedToTaskbarCallback& callback) override;

  mojo::StrongBinding<ShellHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(ShellHandlerImpl);
};

#endif  // CHROME_UTILITY_SHELL_HANDLER_IMPL_WIN_H_
