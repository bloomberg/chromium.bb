// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/image_utils.h"
#include "chrome/installer/zucchini/zucchini.h"

namespace zucchini {

status::Code GenerateEnsemble(ConstBufferView old_image,
                              ConstBufferView new_image,
                              EnsemblePatchWriter* patch_writer) {
  patch_writer->SetPatchType(PatchType::kEnsemblePatch);

  // Dummy patch element to fill patch_writer.
  PatchElementWriter patch_element(
      {Element(old_image.region()), Element(new_image.region())});
  patch_element.SetEquivalenceSink({});
  patch_element.SetExtraDataSink({});
  patch_element.SetRawDeltaSink({});
  patch_element.SetReferenceDeltaSink({});
  patch_writer->AddElement(std::move(patch_element));

  // TODO(etiennep): Implement.
  return zucchini::status::kStatusSuccess;
}

status::Code GenerateRaw(ConstBufferView old_image,
                         ConstBufferView new_image,
                         EnsemblePatchWriter* patch_writer) {
  patch_writer->SetPatchType(PatchType::kRawPatch);

  // Dummy patch element to fill patch_writer.
  PatchElementWriter patch_element(
      {Element(old_image.region()), Element(new_image.region())});
  patch_element.SetEquivalenceSink({});
  patch_element.SetExtraDataSink({});
  patch_element.SetRawDeltaSink({});
  patch_element.SetReferenceDeltaSink({});
  patch_writer->AddElement(std::move(patch_element));

  // TODO(etiennep): Implement.
  return zucchini::status::kStatusSuccess;
}

}  // namespace zucchini
