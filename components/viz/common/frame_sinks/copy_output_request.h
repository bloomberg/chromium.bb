// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_REQUEST_H_
#define COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_REQUEST_H_

#include <memory>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/optional.h"
#include "base/task_runner.h"
#include "base/unguessable_token.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "components/viz/common/quads/texture_mailbox.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/geometry/rect.h"

namespace viz {

namespace mojom {
class CopyOutputRequestDataView;
}

// Holds all the properties pertaining to a copy of a surface or layer.
// Implementations that execute these requests must provide the requested
// ResultFormat or else an "empty" result. Likewise, this means that any
// transient or permanent errors preventing the successful execution of a
// copy request will result in an "empty" result.
//
// Usage: Client code creates a CopyOutputRequest, optionally sets some/all of
// its properties, and then submits it to the compositing pipeline via one of a
// number of possible entry points (usually methods named RequestCopyOfOutput()
// or RequestCopyOfSurface()). Then, some time later, the given result callback
// will be run and the client processes the CopyOutputResult containing the
// image.
//
// Note: This should be used for one-off screen capture only, and NOT for video
// screen capture use cases (please use FrameSinkVideoCapturer instead).
class VIZ_COMMON_EXPORT CopyOutputRequest {
 public:
  using ResultFormat = CopyOutputResult::Format;

  using CopyOutputRequestCallback =
      base::OnceCallback<void(std::unique_ptr<CopyOutputResult> result)>;

  CopyOutputRequest(ResultFormat result_format,
                    CopyOutputRequestCallback result_callback);

  ~CopyOutputRequest();

  // Returns the requested result format.
  ResultFormat result_format() const { return result_format_; }

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

  // By default copy requests copy the entire surface (or layer's subtree
  // output). Specifying an area requests that only a portion be copied. Note
  // that in some cases it may be necessary to sample the pixels surrounding the
  // area.
  void set_area(const gfx::Rect& area) { area_ = area; }
  bool has_area() const { return area_.has_value(); }
  const gfx::Rect& area() const { return *area_; }

  // Legacy support for providing textures up-front, to copy results into.
  // TODO(miu): Remove these methods after tab capture is moved to VIZ.
  // http://crbug.com/754872
  void SetTextureMailbox(const TextureMailbox& texture_mailbox);
  bool has_texture_mailbox() const { return texture_mailbox_.has_value(); }
  const TextureMailbox& texture_mailbox() const { return *texture_mailbox_; }

  // Sends the result from executing this request. Called by the internal
  // implementation, usually a DirectRenderer.
  void SendResult(std::unique_ptr<CopyOutputResult> result);

  // Creates a RGBA_BITMAP request that ignores results, for testing purposes.
  static std::unique_ptr<CopyOutputRequest> CreateStubForTesting();

 private:
  // Note: The StructTraits may "steal" the |result_callback_|, to allow it to
  // outlive this CopyOutputRequest (and wait for the result from another
  // process).
  friend struct mojo::StructTraits<mojom::CopyOutputRequestDataView,
                                   std::unique_ptr<CopyOutputRequest>>;

  const ResultFormat result_format_;
  CopyOutputRequestCallback result_callback_;
  scoped_refptr<base::TaskRunner> result_task_runner_;
  base::Optional<base::UnguessableToken> source_;
  base::Optional<gfx::Rect> area_;
  base::Optional<TextureMailbox> texture_mailbox_;

  DISALLOW_COPY_AND_ASSIGN(CopyOutputRequest);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_FRAME_SINKS_COPY_OUTPUT_REQUEST_H_
