// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_SYSUI_APPLICATION_H_
#define ASH_MUS_SYSUI_APPLICATION_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace ash {
namespace sysui {

class AshInit;

class SysUIApplication : public mojo::ShellClient {
 public:
  SysUIApplication();
  ~SysUIApplication() override;

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Shell* shell,
                  const std::string& url,
                  uint32_t id) override;

  mojo::TracingImpl tracing_;
  scoped_ptr<AshInit> ash_init_;

  DISALLOW_COPY_AND_ASSIGN(SysUIApplication);
};

}  // namespace sysui
}  // namespace ash

#endif  // ASH_MUS_SYSUI_APPLICATION_H_
