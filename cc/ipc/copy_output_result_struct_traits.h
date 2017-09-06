// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_IPC_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_
#define CC_IPC_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_

#include "cc/ipc/copy_output_result.mojom-shared.h"
#include "cc/ipc/texture_mailbox_releaser.mojom.h"
#include "cc/ipc/texture_mailbox_struct_traits.h"
#include "components/viz/common/quads/copy_output_result.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"

namespace mojo {

template <>
struct EnumTraits<cc::mojom::CopyOutputResultFormat,
                  viz::CopyOutputResult::Format> {
  static cc::mojom::CopyOutputResultFormat ToMojom(
      viz::CopyOutputResult::Format format) {
    switch (format) {
      case viz::CopyOutputResult::Format::RGBA_BITMAP:
        return cc::mojom::CopyOutputResultFormat::RGBA_BITMAP;
      case viz::CopyOutputResult::Format::RGBA_TEXTURE:
        return cc::mojom::CopyOutputResultFormat::RGBA_TEXTURE;
    }
    NOTREACHED();
    return cc::mojom::CopyOutputResultFormat::RGBA_BITMAP;
  }

  static bool FromMojom(cc::mojom::CopyOutputResultFormat input,
                        viz::CopyOutputResult::Format* out) {
    switch (input) {
      case cc::mojom::CopyOutputResultFormat::RGBA_BITMAP:
        *out = viz::CopyOutputResult::Format::RGBA_BITMAP;
        return true;
      case cc::mojom::CopyOutputResultFormat::RGBA_TEXTURE:
        *out = viz::CopyOutputResult::Format::RGBA_TEXTURE;
        return true;
    }
    return false;
  }
};

template <>
struct StructTraits<cc::mojom::CopyOutputResultDataView,
                    std::unique_ptr<viz::CopyOutputResult>> {
  static viz::CopyOutputResult::Format format(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    return result->format();
  }

  static const gfx::Rect& rect(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    return result->rect();
  }

  static const SkBitmap& bitmap(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    return result->AsSkBitmap();
  }

  static base::Optional<viz::TextureMailbox> texture_mailbox(
      const std::unique_ptr<viz::CopyOutputResult>& result) {
    if (HasTextureResult(*result))
      return *result->GetTextureMailbox();
    else
      return base::nullopt;
  }

  static cc::mojom::TextureMailboxReleaserPtr releaser(
      const std::unique_ptr<viz::CopyOutputResult>& result);

  static bool Read(cc::mojom::CopyOutputResultDataView data,
                   std::unique_ptr<viz::CopyOutputResult>* out_p);

 private:
  static bool HasTextureResult(const viz::CopyOutputResult& result);
};

}  // namespace mojo

#endif  // CC_IPC_COPY_OUTPUT_RESULT_STRUCT_TRAITS_H_
