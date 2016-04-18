// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/compositor/webp_decoder.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "blimp/common/blob_cache/blob_cache.h"
#include "blimp/common/blob_cache/id_util.h"
#include "blimp/common/blob_cache/in_memory_blob_cache.h"
#include "blimp/common/proto/blob_cache.pb.h"
#include "third_party/libwebp/webp/decode.h"
#include "third_party/libwebp/webp/demux.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blimp {
namespace {

// TODO(nyquist): Make this not be infinite size.
static base::LazyInstance<InMemoryBlobCache> g_blob_cache =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

bool WebPDecoder(const void* input, size_t input_size, SkBitmap* bitmap) {
  DCHECK(bitmap);

  // Initialize an empty WebPDecoderConfig.
  WebPDecoderConfig config;
  if (!WebPInitDecoderConfig(&config)) {
    LOG(WARNING) << "Failed to initialize WebP config.";
    return false;
  }

  BlobCacheImageMetadata deserialized;
  int signed_size = base::checked_cast<int>(input_size);
  if (!deserialized.ParseFromArray(input, signed_size)) {
    LOG(WARNING) << "Failed to parse BlobCacheImageMetadata";
    return false;
  }

  // The cache uses the SHA1 hex string of the ID.
  if (deserialized.id().length() != base::kSHA1Length) {
    LOG(WARNING) << "Length of ID is not base::kSHA1Length ("
                 << base::kSHA1Length << "), but "
                 << deserialized.id().length();
    return false;
  }
  std::string hex_id = FormatBlobId(deserialized.id());

  // Declared here to still be in scope while decoding WebP data.
  BlobData cached;

  // Set to true if the client already has the data in its cache. If it does not
  // keeping |found_in_cache| as false will trigger caching the input in the
  // end of this function.
  bool found_in_cache = false;

  // Used later to decode the image and is initialized either based on a cached
  // item or from the |payload| of the proto.
  WebPData webp_data;

  if (g_blob_cache.Get().Contains(deserialized.id())) {
    // The image was found in the cache, so decode using cached data.
    cached = g_blob_cache.Get().Get(deserialized.id());
    webp_data.bytes = reinterpret_cast<const uint8_t*>(cached->data.data());
    webp_data.size = cached->data.size();
    DVLOG(2) << "Found SHA1 " << hex_id << " with size = " << webp_data.size;
    found_in_cache = true;
  } else {
    // The image was not found in the cache, so decode using the payload.
    DCHECK(deserialized.has_payload());
    webp_data.bytes =
        reinterpret_cast<const uint8_t*>(deserialized.payload().c_str());
    webp_data.size = deserialized.payload().size();
  }

  // Read WebP feature information into |config.input|, which is a
  // WebPBitstreamFeatures. It contains information such as width, height and
  // whether the WebP image has an alpha channel or not.
  if (WebPGetFeatures(webp_data.bytes, webp_data.size, &config.input) !=
      VP8_STATUS_OK) {
    LOG(WARNING) << "Failed to get WebP features.";
    return false;
  }
  // Animations are not supported.
  DCHECK_EQ(0, config.input.has_animation);

  // Allocate correct size for the bitmap based on the WebPBitstreamFeatures.
  bitmap->allocN32Pixels(config.input.width, config.input.height);
  DCHECK_EQ(kPremul_SkAlphaType, bitmap->alphaType());

  // Setup the decoder buffer based on the WebPBitstreamFeatures.
  WebPDecBuffer decoderBuffer;

#if SK_B32_SHIFT  // Output little-endian RGBA pixels (Android).
  decoderBuffer.colorspace = MODE_rgbA;
#else  // Output little-endian BGRA pixels.
  decoderBuffer.colorspace = MODE_bgrA;
#endif
  decoderBuffer.u.RGBA.stride = config.input.width * 4;
  decoderBuffer.u.RGBA.size = decoderBuffer.u.RGBA.stride * config.input.height;

  // Instead of using the default WebPDecBuffer output, make WebPDecode directly
  // write into the SkBitmap.
  decoderBuffer.is_external_memory = 1;
  decoderBuffer.u.RGBA.rgba =
      reinterpret_cast<uint8_t*>(bitmap->getAddr32(0, 0));

  // Set the config up to use the decoding buffer we created.
  config.output = decoderBuffer;

  // Decode the input data into the bitmap buffer.
  bool success =
      WebPDecode(webp_data.bytes, webp_data.size, &config) == VP8_STATUS_OK;

  // Now free the buffer. It is safe to call this even when the buffer is
  // external and not allocated by WebPDecode.
  WebPFreeDecBuffer(&config.output);

  if (!success) {
    LOG(WARNING) << "Failed to decode WebP data.";
    return false;
  }

  if (!found_in_cache) {
    DVLOG(2) << "Inserting image to cache with SHA1: " << hex_id
             << " size: " << webp_data.size;
    BlobData to_cache(new base::RefCountedData<const std::string>(std::string(
        reinterpret_cast<const char*>(webp_data.bytes), webp_data.size)));
    g_blob_cache.Get().Put(deserialized.id(), std::move(to_cache));
  }

  return true;
}

}  // namespace blimp
