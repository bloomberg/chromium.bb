// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ZUCCHINI_ZUCCHINI_H_
#define COMPONENTS_ZUCCHINI_ZUCCHINI_H_

#include "components/zucchini/buffer_view.h"
#include "components/zucchini/patch_reader.h"
#include "components/zucchini/patch_writer.h"

// Definitions, structures, and interfaces for the Zucchini library.

namespace zucchini {

namespace status {

// Zucchini status code, which can also be used as process exit code. Therefore
// success is explicitly 0.
enum Code {
  kStatusSuccess = 0,
  kStatusInvalidParam = 1,
  kStatusFileReadError = 2,
  kStatusFileWriteError = 3,
  kStatusPatchReadError = 4,
  kStatusPatchWriteError = 5,
  kStatusInvalidOldImage = 6,
  kStatusInvalidNewImage = 7,
  kStatusFatal = 8,
};

}  // namespace status

// Generates ensemble patch from |old_image| to |new_image|, and writes it to
// |patch_writer|.
status::Code GenerateEnsemble(ConstBufferView old_image,
                              ConstBufferView new_image,
                              EnsemblePatchWriter* patch_writer);

// Generates raw patch from |old_image| to |new_image|, and writes it to
// |patch_writer|.
status::Code GenerateRaw(ConstBufferView old_image,
                         ConstBufferView new_image,
                         EnsemblePatchWriter* patch_writer);

// Applies |patch_reader| to |old_image| to build |new_image|, which refers to
// preallocated memory of sufficient size.
status::Code Apply(ConstBufferView old_image,
                   const EnsemblePatchReader& patch_reader,
                   MutableBufferView new_image);

}  // namespace zucchini

#endif  // COMPONENTS_ZUCCHINI_ZUCCHINI_H_
