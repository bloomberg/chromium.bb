// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_PROXY_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_PROXY_H_

#include "mojo/public/bindings/lib/message.h"
#include "mojo/public/bindings/sample/generated/sample_service.h"

namespace sample {

class ServiceProxy : public Service {
 public:
  explicit ServiceProxy(mojo::MessageReceiver* receiver);

  virtual void Frobinate(const Foo* Foo, bool baz, mojo::Handle port)
      MOJO_OVERRIDE;

 private:
  mojo::MessageReceiver* receiver_;
};

}  // namespace test

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_PROXY_H_
