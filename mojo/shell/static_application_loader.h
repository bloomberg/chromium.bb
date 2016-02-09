// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_STATIC_APPLICATION_LOADER_H_
#define MOJO_SHELL_STATIC_APPLICATION_LOADER_H_

#include <list>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/shell/application_loader.h"

namespace base {
class SimpleThread;
}

namespace mojo {
class ShellClient;
}

namespace mojo {
namespace shell {

// An ApplicationLoader which loads a single type of app from a given
// mojo::ShellClient factory. A Load() request is fulfilled by creating an
// instance of the app on a new thread. Only one instance of the app will run at
// a time. Any Load requests received while the app is running will be dropped.
class StaticApplicationLoader : public mojo::shell::ApplicationLoader {
 public:
  using ApplicationFactory =
      base::Callback<scoped_ptr<mojo::ShellClient>()>;

  // Constructs a static loader for |factory|.
  explicit StaticApplicationLoader(const ApplicationFactory& factory);

  // Constructs a static loader for |factory| with a closure that will be called
  // when the loaded application quits.
  StaticApplicationLoader(const ApplicationFactory& factory,
                          const base::Closure& quit_callback);

  ~StaticApplicationLoader() override;

  // mojo::shell::ApplicationLoader:
  void Load(const GURL& url,
            InterfaceRequest<mojom::ShellClient> request) override;

 private:
  void StopAppThread();

  // The factory used t create new instances of the application delegate.
  ApplicationFactory factory_;

  // If not null, this is run when the loaded application quits.
  base::Closure quit_callback_;

  // Thread for the application if currently running.
  scoped_ptr<base::SimpleThread> thread_;

  base::WeakPtrFactory<StaticApplicationLoader> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StaticApplicationLoader);
};

}  // namespace shell
}  // namespace mojo

#endif  // MOJO_SHELL_STATIC_APPLICATION_LOADER_H_
