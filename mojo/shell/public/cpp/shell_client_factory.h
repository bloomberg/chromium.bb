// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SHELL_PUBLIC_CPP_SHELL_CLIENT_FACTORY_H_
#define MOJO_SHELL_PUBLIC_CPP_SHELL_CLIENT_FACTORY_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mojo/services/network/public/interfaces/url_loader.mojom.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/interfaces/shell.mojom.h"
#include "mojo/shell/public/interfaces/shell_client_factory.mojom.h"

class GURL;

namespace mojo {

class ShellClientFactory
    : public InterfaceFactory<shell::mojom::ShellClientFactory> {
 public:
  class HandledApplicationHolder {
   public:
    virtual ~HandledApplicationHolder() {}
  };

  class Delegate {
   public:
    virtual ~Delegate() {}
    // Implement this method to create the Application. This method will be
    // called on a new thread. Leaving this method will quit the application.
    virtual void CreateShellClient(
        shell::mojom::ShellClientRequest request,
        const GURL& url) = 0;
  };

  class ManagedDelegate : public Delegate {
   public:
    ~ManagedDelegate() override {}
    // Implement this method to create the Application for the given content.
    // This method will be called on a new thread. The application will be run
    // on this new thread, and the returned value will be kept alive until the
    // application ends.
    virtual scoped_ptr<HandledApplicationHolder> CreateShellClientManaged(
        shell::mojom::ShellClientRequest request,
        const GURL& url) = 0;

   private:
    void CreateShellClient(shell::mojom::ShellClientRequest request,
                           const GURL& url) override;
  };

  explicit ShellClientFactory(Delegate* delegate);
  ~ShellClientFactory() override;

 private:
  // InterfaceFactory<shell::mojom::ShellClientFactory>:
  void Create(Connection* connection,
              shell::mojom::ShellClientFactoryRequest request) override;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ShellClientFactory);
};

template <class A>
class HandledApplicationHolderImpl
    : public ShellClientFactory::HandledApplicationHolder {
 public:
  explicit HandledApplicationHolderImpl(A* value) : value_(value) {}

 private:
  scoped_ptr<A> value_;
};

template <class A>
scoped_ptr<ShellClientFactory::HandledApplicationHolder>
make_handled_factory_holder(A* value) {
  return make_scoped_ptr(new HandledApplicationHolderImpl<A>(value));
}

}  // namespace mojo

#endif  // MOJO_SHELL_PUBLIC_CPP_SHELL_CLIENT_FACTORY_H_
