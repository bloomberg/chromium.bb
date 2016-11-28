// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_custom_types_struct_traits.h"

#include "mojo/public/cpp/system/platform_handle.h"

namespace mojo {

// static
bool StructTraits<common::mojom::String16DataView, base::string16>::Read(
    common::mojom::String16DataView data,
    base::string16* out) {
  ArrayDataView<uint16_t> view;
  data.GetDataDataView(&view);
  out->assign(reinterpret_cast<const base::char16*>(view.data()), view.size());
  return true;
}

// static
const std::vector<uint32_t>&
StructTraits<common::mojom::VersionDataView, base::Version>::components(
    const base::Version& version) {
  return version.components();
}

// static
bool StructTraits<common::mojom::VersionDataView, base::Version>::Read(
    common::mojom::VersionDataView data,
    base::Version* out) {
  std::vector<uint32_t> components;
  if (!data.ReadComponents(&components))
    return false;

  *out = base::Version(base::Version(std::move(components)));
  return out->IsValid();
}

// static
bool StructTraits<
    common::mojom::UnguessableTokenDataView,
    base::UnguessableToken>::Read(common::mojom::UnguessableTokenDataView data,
                                  base::UnguessableToken* out) {
  uint64_t high = data.high();
  uint64_t low = data.low();

  // Receiving a zeroed UnguessableToken is a security issue.
  if (high == 0 && low == 0)
    return false;

  *out = base::UnguessableToken::Deserialize(high, low);
  return true;
}

mojo::ScopedHandle StructTraits<common::mojom::FileDataView, base::File>::fd(
    base::File& file) {
  DCHECK(file.IsValid());
  return mojo::WrapPlatformFile(file.TakePlatformFile());
}

bool StructTraits<common::mojom::FileDataView, base::File>::Read(
    common::mojom::FileDataView data,
    base::File* file) {
  base::PlatformFile platform_handle = base::kInvalidPlatformFile;
  if (mojo::UnwrapPlatformFile(data.TakeFd(), &platform_handle) !=
      MOJO_RESULT_OK) {
    return false;
  }
  *file = base::File(platform_handle);
  return true;
}

}  // namespace mojo
