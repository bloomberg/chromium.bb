// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_COPY_OUTPUT_REQUEST_H_
#define COMPONENTS_VIZ_COMMON_QUADS_COPY_OUTPUT_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/task_runner.h"
#include "base/unguessable_token.h"
#include "components/viz/common/quads/single_release_callback.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "components/viz/common/viz_common_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "ui/gfx/geometry/rect.h"

class SkBitmap;

namespace viz {

namespace mojom {
class CopyOutputRequestDataView;
}

class CopyOutputResult;

class VIZ_COMMON_EXPORT CopyOutputRequest {
 public:
  using CopyOutputRequestCallback =
      base::OnceCallback<void(std::unique_ptr<CopyOutputResult> result)>;

  static std::unique_ptr<CopyOutputRequest> CreateEmptyRequest() {
    return base::WrapUnique(new CopyOutputRequest);
  }
  static std::unique_ptr<CopyOutputRequest> CreateRequest(
      CopyOutputRequestCallback result_callback) {
    return base::WrapUnique(
        new CopyOutputRequest(false, std::move(result_callback)));
  }
  static std::unique_ptr<CopyOutputRequest> CreateBitmapRequest(
      CopyOutputRequestCallback result_callback) {
    return base::WrapUnique(
        new CopyOutputRequest(true, std::move(result_callback)));
  }

  ~CopyOutputRequest();

  bool IsEmpty() const { return result_callback_.is_null(); }

  // Requests that the result callback be run as a task posted to the given
  // |task_runner|. If this is not set, the result callback could be run from
  // any context.
  void set_result_task_runner(scoped_refptr<base::TaskRunner> task_runner) {
    result_task_runner_ = std::move(task_runner);
  }
  bool has_result_task_runner() const { return !!result_task_runner_; }

  // Optionally specify the source of this copy request. If set when this copy
  // request is submitted to a layer, a prior uncommitted copy request from the
  // same source will be aborted.
  void set_source(const base::UnguessableToken& source) { source_ = source; }
  bool has_source() const { return source_.has_value(); }
  const base::UnguessableToken& source() const { return *source_; }

  bool force_bitmap_result() const { return force_bitmap_result_; }

  // By default copy requests copy the entire layer's subtree output. If an
  // area is given, then the intersection of this rect (in layer space) with
  // the layer's subtree output will be returned.
  void set_area(const gfx::Rect& area) { area_ = area; }
  bool has_area() const { return area_.has_value(); }
  const gfx::Rect& area() const { return *area_; }

  // By default copy requests create a new TextureMailbox to return contents
  // in. This allows a client to provide a TextureMailbox, and the compositor
  // will place the result inside the TextureMailbox.
  void SetTextureMailbox(const TextureMailbox& texture_mailbox);
  bool has_texture_mailbox() const { return texture_mailbox_.has_value(); }
  const TextureMailbox& texture_mailbox() const { return *texture_mailbox_; }

  void SendEmptyResult();
  void SendBitmapResult(std::unique_ptr<SkBitmap> bitmap);
  void SendTextureResult(
      const gfx::Size& size,
      const TextureMailbox& texture_mailbox,
      std::unique_ptr<SingleReleaseCallback> release_callback);

  void SendResult(std::unique_ptr<CopyOutputResult> result);

 private:
  friend struct mojo::StructTraits<mojom::CopyOutputRequestDataView,
                                   std::unique_ptr<CopyOutputRequest>>;

  CopyOutputRequest();
  CopyOutputRequest(bool force_bitmap_result,
                    CopyOutputRequestCallback result_callback);

  scoped_refptr<base::TaskRunner> result_task_runner_;
  base::Optional<base::UnguessableToken> source_;
  bool force_bitmap_result_;
  base::Optional<gfx::Rect> area_;
  base::Optional<TextureMailbox> texture_mailbox_;
  CopyOutputRequestCallback result_callback_;

  DISALLOW_COPY_AND_ASSIGN(CopyOutputRequest);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_COPY_OUTPUT_REQUEST_H_
