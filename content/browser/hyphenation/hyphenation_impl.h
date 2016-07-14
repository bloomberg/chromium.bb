// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_HYPHENATION_HYPHENATION_IMPL_H_
#define CONTENT_BROWSER_HYPHENATION_HYPHENATION_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/WebKit/public/platform/modules/hyphenation/hyphenation.mojom.h"

namespace hyphenation {

class HyphenationImpl : public blink::mojom::Hyphenation {
 public:
  static void Create(blink::mojom::HyphenationRequest);

  // Hyphenation:
  void OpenDictionary(const mojo::String& locale,
                      const OpenDictionaryCallback& callback) override;

 private:
  explicit HyphenationImpl(blink::mojom::HyphenationRequest);
  ~HyphenationImpl() override;

  mojo::StrongBinding<blink::mojom::Hyphenation> binding_;

  DISALLOW_COPY_AND_ASSIGN(HyphenationImpl);
};

}  // namespace hyphenation

#endif  // CONTENT_BROWSER_HYPHENATION_HYPHENATION_IMPL_H_
