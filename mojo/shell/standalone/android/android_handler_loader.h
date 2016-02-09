// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STANDALONE_ANDROID_ANDROID_HANDLER_LOADER_H_
#define MOJO_SHELL_STANDALONE_ANDROID_ANDROID_HANDLER_LOADER_H_

#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/shell/application_loader.h"
#include "mojo/shell/public/cpp/shell_connection.h"
#include "mojo/shell/standalone/android/android_handler.h"

namespace mojo {
namespace shell {

class AndroidHandlerLoader : public ApplicationLoader {
 public:
  AndroidHandlerLoader();
  virtual ~AndroidHandlerLoader();

 private:
  // ApplicationLoader overrides:
  void Load(const GURL& url,
            InterfaceRequest<mojom::ShellClient> request) override;

  AndroidHandler android_handler_;
  scoped_ptr<ShellConnection> shell_client_;

  DISALLOW_COPY_AND_ASSIGN(AndroidHandlerLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_STANDALONE_ANDROID_ANDROID_HANDLER_LOADER_H_
