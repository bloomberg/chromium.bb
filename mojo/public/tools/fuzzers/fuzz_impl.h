// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_TOOLS_FUZZERS_FUZZ_IMPL_H_
#define MOJO_PUBLIC_TOOLS_FUZZERS_FUZZ_IMPL_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/tools/fuzzers/fuzz.mojom.h"

class FuzzImpl : public fuzz::mojom::FuzzInterface {
 public:
  explicit FuzzImpl(fuzz::mojom::FuzzInterfaceRequest request);
  ~FuzzImpl() override;

  void FuzzBasic() override;
  void FuzzBasicResp(FuzzBasicRespCallback callback) override;
  void FuzzBasicSyncResp(FuzzBasicSyncRespCallback callback) override;
  void FuzzArgs(fuzz::mojom::FuzzStructPtr a,
                fuzz::mojom::FuzzStructPtr b) override;

  void FuzzArgsResp(fuzz::mojom::FuzzStructPtr a,
                    fuzz::mojom::FuzzStructPtr b,
                    FuzzArgsRespCallback callback) override;
  void FuzzArgsSyncResp(fuzz::mojom::FuzzStructPtr a,
                        fuzz::mojom::FuzzStructPtr b,
                        FuzzArgsSyncRespCallback callback) override;

  /* Expose the binding to the fuzz harness. */
  mojo::Binding<FuzzInterface> binding_;
};

#endif  // MOJO_PUBLIC_TOOLS_FUZZERS_FUZZ_IMPL_H_
