// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UTILITY_SHELL_HANDLER_IMPL_WIN_H_
#define CHROME_UTILITY_SHELL_HANDLER_IMPL_WIN_H_

#include "base/macros.h"
#include "chrome/common/shell_handler_win.mojom.h"

// Implements the ShellHandler mojo interface.
class ShellHandlerImpl : public chrome::mojom::ShellHandler {
 public:
  ShellHandlerImpl();
  ~ShellHandlerImpl() override;

  static void Create(chrome::mojom::ShellHandlerRequest request);

 private:
  // chrome::mojom::ShellHandler:
  void IsPinnedToTaskbar(const IsPinnedToTaskbarCallback& callback) override;

  DISALLOW_COPY_AND_ASSIGN(ShellHandlerImpl);
};

#endif  // CHROME_UTILITY_SHELL_HANDLER_IMPL_WIN_H_
