// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MOJO_STATIC_APPLICATION_LOADER_H_
#define CONTENT_COMMON_MOJO_STATIC_APPLICATION_LOADER_H_

#include <list>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/shell/loader.h"

namespace base {
class SimpleThread;
}

namespace mojo {
class ShellClient;
}

namespace content {

// An Loader which loads a single type of app from a given
// mojo::ShellClientFactory. A Load() request is fulfilled by creating an
// instance of the app on a new thread. Only one instance of the app will run at
// a time. Any Load requests received while the app is running will be dropped.
class StaticLoader : public mojo::shell::Loader {
 public:
  using ApplicationFactory =
      base::Callback<std::unique_ptr<mojo::ShellClient>()>;

  // Constructs a static loader for |factory|.
  explicit StaticLoader(const ApplicationFactory& factory);

  // Constructs a static loader for |factory| with a closure that will be called
  // when the loaded application quits.
  StaticLoader(const ApplicationFactory& factory,
                          const base::Closure& quit_callback);

  ~StaticLoader() override;

  // mojo::shell::Loader:
  void Load(const std::string& name,
            mojo::shell::mojom::ShellClientRequest request) override;

 private:
  void StopAppThread();

  // The factory used t create new instances of the application delegate.
  ApplicationFactory factory_;

  // If not null, this is run when the loaded application quits.
  base::Closure quit_callback_;

  // Thread for the application if currently running.
  std::unique_ptr<base::SimpleThread> thread_;

  base::WeakPtrFactory<StaticLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StaticLoader);
};

}  // namespace content

#endif  // CONTENT_COMMON_MOJO_STATIC_APPLICATION_LOADER_H_
