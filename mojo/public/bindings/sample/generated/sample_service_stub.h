// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_STUB_H_
#define MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_STUB_H_

#include "mojo/public/bindings/lib/message.h"
#include "mojo/public/bindings/sample/generated/sample_service.h"

namespace sample {

class ServiceStub : public Service, public mojo::MessageReceiver {
 public:
  virtual bool Accept(mojo::Message* message) MOJO_OVERRIDE;
};

}  // namespace sample

#endif  // MOJO_GENERATED_BINDINGS_SAMPLE_SERVICE_STUB_H_
