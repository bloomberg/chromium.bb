// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/manifest_struct_traits.h"

#include "mojo/common/string16_struct_traits.h"
#include "third_party/WebKit/public/platform/WebDisplayModeStructTraits.h"
#include "third_party/WebKit/public/platform/modules/screen_orientation/WebScreenOrientationEnumTraits.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "url/mojo/url_gurl_struct_traits.h"

namespace mojo {
namespace {

bool ValidateColor(int64_t color) {
  return color >= std::numeric_limits<int32_t>::min() ||
         color <= std::numeric_limits<int32_t>::max() ||
         color == content::Manifest::kInvalidOrMissingColor;
}

// A wrapper around base::Optional<base::string16> so a custom StructTraits
// specialization can enforce maximum string length.
struct TruncatedString16 {
  base::Optional<base::string16> string;
};

}  // namespace

template <>
struct StructTraits<common::mojom::String16DataView, TruncatedString16> {
  static void SetToNull(TruncatedString16* output) { output->string.reset(); }

  static bool Read(common::mojom::String16DataView input,
                   TruncatedString16* output) {
    if (input.is_null()) {
      output->string.reset();
      return true;
    }
    mojo::ArrayDataView<uint16_t> buffer_view;
    input.GetDataDataView(&buffer_view);
    if (buffer_view.size() > content::Manifest::kMaxIPCStringLength)
      return false;

    output->string.emplace();
    return StructTraits<common::mojom::String16DataView, base::string16>::Read(
        input, &output->string.value());
  }
};

bool StructTraits<blink::mojom::ManifestDataView, content::Manifest>::Read(
    blink::mojom::ManifestDataView data,
    content::Manifest* out) {
  TruncatedString16 string;
  if (!data.ReadName(&string))
    return false;
  out->name = base::NullableString16(std::move(string.string));

  if (!data.ReadShortName(&string))
    return false;
  out->short_name = base::NullableString16(std::move(string.string));

  if (!data.ReadGcmSenderId(&string))
    return false;
  out->gcm_sender_id = base::NullableString16(std::move(string.string));

  if (!data.ReadStartUrl(&out->start_url))
    return false;

  if (!data.ReadIcons(&out->icons))
    return false;

  if (!data.ReadShareTarget(&out->share_target))
    return false;

  if (!data.ReadRelatedApplications(&out->related_applications))
    return false;

  out->prefer_related_applications = data.prefer_related_applications();
  out->theme_color = data.theme_color();
  if (!ValidateColor(out->theme_color))
    return false;

  out->background_color = data.background_color();
  if (!ValidateColor(out->background_color))
    return false;

  if (!data.ReadSplashScreenUrl(&out->splash_screen_url))
    return false;

  if (!data.ReadDisplay(&out->display))
    return false;

  if (!data.ReadOrientation(&out->orientation))
    return false;

  if (!data.ReadScope(&out->scope))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestIconDataView, content::Manifest::Icon>::
    Read(blink::mojom::ManifestIconDataView data,
         content::Manifest::Icon* out) {
  if (!data.ReadSrc(&out->src))
    return false;

  TruncatedString16 string;
  if (!data.ReadType(&string))
    return false;

  if (!string.string)
    return false;

  out->type = *std::move(string.string);

  if (!data.ReadSizes(&out->sizes))
    return false;

  if (!data.ReadPurpose(&out->purpose))
    return false;

  return true;
}

bool StructTraits<blink::mojom::ManifestRelatedApplicationDataView,
                  content::Manifest::RelatedApplication>::
    Read(blink::mojom::ManifestRelatedApplicationDataView data,
         content::Manifest::RelatedApplication* out) {
  TruncatedString16 string;
  if (!data.ReadPlatform(&string))
    return false;
  out->platform = base::NullableString16(std::move(string.string));

  if (!data.ReadUrl(&out->url))
    return false;

  if (!data.ReadId(&string))
    return false;
  out->id = base::NullableString16(std::move(string.string));

  return !(out->url.is_empty() && out->id.is_null());
}

bool StructTraits<blink::mojom::ManifestShareTargetDataView,
                  content::Manifest::ShareTarget>::
    Read(blink::mojom::ManifestShareTargetDataView data,
         content::Manifest::ShareTarget* out) {
  TruncatedString16 string;
  if (!data.ReadUrlTemplate(&string))
    return false;
  out->url_template = base::NullableString16(std::move(string.string));
  return true;
}

}  // namespace mojo
