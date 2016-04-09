// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_CURRENT_THREAD_LOADER_H_
#define CONTENT_COMMON_MOJO_CURRENT_THREAD_LOADER_H_

#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "mojo/shell/loader.h"
#include "mojo/shell/public/cpp/shell_client.h"
#include "mojo/shell/public/cpp/shell_connection.h"

namespace content {

// A Loader which loads a single type of app from a mojo::ShellClientFactory on
// the current thread.
class CurrentThreadLoader : public mojo::shell::Loader {
 public:
  using ApplicationFactory =
      base::Callback<std::unique_ptr<mojo::ShellClient>()>;

  explicit CurrentThreadLoader(const ApplicationFactory& factory);
  ~CurrentThreadLoader() override;

  // mojo::shell::Loader:
  void Load(const std::string& name,
            mojo::shell::mojom::ShellClientRequest request) override;

 private:
  // The factory used to create new instances of the application delegate. This
  // is called exactly once since all connections share a single client.
  ApplicationFactory factory_;

  // Our shared shell client, passed to each connection.
  std::unique_ptr<mojo::ShellClient> shell_client_;

  std::vector<std::unique_ptr<mojo::ShellConnection>> connections_;

  DISALLOW_COPY_AND_ASSIGN(CurrentThreadLoader);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_CURRENT_THREAD_LOADER_H_
