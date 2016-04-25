// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/engine/renderer/engine_image_serialization_processor.h"

#include <stddef.h>
#include <set>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "blimp/common/blob_cache/id_util.h"
#include "blimp/common/compositor/webp_decoder.h"
#include "blimp/common/proto/blob_cache.pb.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/libwebp/webp/encode.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelSerializer.h"
#include "third_party/skia/include/core/SkUnPreMultiply.h"

namespace blimp {
namespace {

// TODO(nyquist): Add support for changing this from the client.
static base::LazyInstance<std::set<BlobId>> g_client_cache_contents =
    LAZY_INSTANCE_INITIALIZER;

SkData* BlobCacheImageMetadataProtoAsSkData(
    const BlobCacheImageMetadata& proto) {
  int signed_size = proto.ByteSize();
  size_t unsigned_size = base::checked_cast<size_t>(signed_size);
  std::vector<uint8_t> serialized(unsigned_size);
  proto.SerializeWithCachedSizesToArray(serialized.data());
  return SkData::NewWithCopy(serialized.data(), serialized.size());
}

// TODO(nyquist): Make sure encoder does not serialize images more than once.
// See crbug.com/548434.
class WebPImageEncoder : public SkPixelSerializer {
 public:
  WebPImageEncoder() {}
  ~WebPImageEncoder() override{};

  bool onUseEncodedData(const void* data, size_t len) override {
    TRACE_EVENT1("blimp", "WebPImageEncoded::UsingEncodedData",
                 "OriginalImageSize", len);
    // Encode all images regardless of their format, including WebP images.
    return false;
  }

  SkData* onEncode(const SkPixmap& pixmap) override {
    TRACE_EVENT0("blimp", "WebImageEncoder::onEncode");
    // Initialize an empty WebPConfig.
    WebPConfig config;
    if (!WebPConfigInit(&config))
      return nullptr;

    // Initialize an empty WebPPicture.
    WebPPicture picture;
    if (!WebPPictureInit(&picture))
      return nullptr;

    // Ensure width and height are valid dimensions.
    if (!pixmap.width() || pixmap.width() > WEBP_MAX_DIMENSION)
      return nullptr;
    picture.width = pixmap.width();
    if (!pixmap.height() || pixmap.height() > WEBP_MAX_DIMENSION)
      return nullptr;
    picture.height = pixmap.height();

    const BlobId blob_id = CalculateBlobId(pixmap.addr(), pixmap.getSafeSize());
    std::string blob_id_hex = BlobIdToString(blob_id);

    // Create proto with all requires information.
    BlobCacheImageMetadata proto;
    proto.set_id(blob_id);
    proto.set_width(pixmap.width());
    proto.set_height(pixmap.height());

    if (g_client_cache_contents.Get().find(blob_id) !=
        g_client_cache_contents.Get().end()) {
      // Found image in client cache, so skip sending decoded payload.
      SkData* sk_data = BlobCacheImageMetadataProtoAsSkData(proto);
      TRACE_EVENT1("blimp", "WebPImageEncoder::onEncode ImageFoundInCache",
                   "EncodedImageSize", sk_data->size());
      DVLOG(2) << "Sending cached: " << blob_id_hex
               << " size = " << sk_data->size();
      return sk_data;
    }

    DVLOG(2) << "Encoding image color_type=" << pixmap.colorType()
             << ", alpha_type=" << pixmap.alphaType() << " " << pixmap.width()
             << "x" << pixmap.height();

    // Import picture from raw pixels.
    auto pixel_chars = static_cast<const unsigned char*>(pixmap.addr());
    if (!PlatformPictureImport(pixel_chars, &picture, pixmap.alphaType()))
      return nullptr;

    // Create a buffer for where to store the output data.
    std::vector<unsigned char> encoded_data;
    picture.custom_ptr = &encoded_data;

    // Use our own WebPWriterFunction implementation.
    picture.writer = &WebPImageEncoder::WriteOutput;

    // Setup the configuration for the output WebP picture. This is currently
    // the same as the default configuration for WebP, but since any change in
    // the WebP defaults would invalidate all caches they are hard coded.
    config.lossless = 0;
    config.quality = 75.0;  // between 0 (smallest file) and 100 (biggest).

    // TODO(nyquist): Move image encoding to a different thread when
    // asynchronous loading of images is possible. The encode work currently
    // blocks the render thread so we are dropping the method down to 0.
    // crbug.com/603643.
    config.method = 0;  // quality/speed trade-off (0=fast, 6=slower-better).

    TRACE_EVENT_BEGIN0("blimp", "WebPImageEncoder::onEncode WebPEncode");
    // Encode the picture using the given configuration.
    bool success = WebPEncode(&config, &picture);
    TRACE_EVENT_END1("blimp", "WebPImageEncoder::onEncode WebPEncode",
                     "EncodedImageSize", encoded_data.size());

    // Release the memory allocated by WebPPictureImport*(). This does not free
    // the memory used by the picture object itself.
    WebPPictureFree(&picture);

    if (!success)
      return nullptr;

    // Did not find item in cache, so add it to client cache representation
    // and send full item.
    g_client_cache_contents.Get().insert(blob_id);
    proto.set_payload(&encoded_data.front(), encoded_data.size());

    // Copy proto into SkData.
    SkData* sk_data = BlobCacheImageMetadataProtoAsSkData(proto);
    DVLOG(2) << "Sending image: " << blob_id_hex
             << " size = " << sk_data->size();
    return sk_data;
  }

 private:
  // WebPWriterFunction implementation.
  static int WriteOutput(const uint8_t* data,
                         size_t size,
                         const WebPPicture* const picture) {
    std::vector<unsigned char>* dest =
        static_cast<std::vector<unsigned char>*>(picture->custom_ptr);
    dest->insert(dest->end(), data, data + size);
    return 1;
  }

  // For each pixel, un-premultiplies the alpha-channel for each of the RGB
  // channels. As an example, for a channel value that before multiplication was
  // 255, and after applying an alpha of 128, the premultiplied pixel would be
  // 128. The un-premultiply step uses the alpha-channel to get back to 255. The
  // alpha channel is kept unchanged.
  void UnPremultiply(const unsigned char* in_pixels,
                     unsigned char* out_pixels,
                     size_t pixel_count) {
    const SkUnPreMultiply::Scale* table = SkUnPreMultiply::GetScaleTable();
    for (; pixel_count-- > 0; in_pixels += 4) {
      unsigned char alpha = in_pixels[3];
      if (alpha == 255) {  // Full opacity, just blindly copy.
        *out_pixels++ = in_pixels[0];
        *out_pixels++ = in_pixels[1];
        *out_pixels++ = in_pixels[2];
        *out_pixels++ = alpha;
      } else {
        SkUnPreMultiply::Scale scale = table[alpha];
        *out_pixels++ = SkUnPreMultiply::ApplyScale(scale, in_pixels[0]);
        *out_pixels++ = SkUnPreMultiply::ApplyScale(scale, in_pixels[1]);
        *out_pixels++ = SkUnPreMultiply::ApplyScale(scale, in_pixels[2]);
        *out_pixels++ = alpha;
      }
    }
  }

  bool PlatformPictureImport(const unsigned char* pixels,
                             WebPPicture* picture,
                             SkAlphaType alphaType) {
    // Each pixel uses 4 bytes (RGBA) which affects the stride per row.
    int row_stride = picture->width * 4;
    if (alphaType == kPremul_SkAlphaType) {
      // Need to unpremultiply each pixel, each pixel using 4 bytes (RGBA).
      size_t pixel_count = picture->height * picture->width;
      std::vector<unsigned char> unpremul_pixels(pixel_count * 4);
      UnPremultiply(pixels, unpremul_pixels.data(), pixel_count);
      if (SK_B32_SHIFT)  // Android
        return WebPPictureImportRGBA(picture, unpremul_pixels.data(),
                                     row_stride);
      return WebPPictureImportBGRA(picture, unpremul_pixels.data(), row_stride);
    }

    if (SK_B32_SHIFT)  // Android
      return WebPPictureImportRGBA(picture, pixels, row_stride);
    return WebPPictureImportBGRA(picture, pixels, row_stride);
  }
};

}  // namespace

namespace engine {

EngineImageSerializationProcessor::EngineImageSerializationProcessor(
    mojom::BlobChannelPtr blob_channel)
    : blob_channel_(std::move(blob_channel)) {
  DCHECK(blob_channel_);

  pixel_serializer_.reset(new WebPImageEncoder);

  // Dummy BlobChannel command.
  // TODO(nyquist): Remove this after integrating BlobChannel.
  blob_channel_->Push("foo");
}

EngineImageSerializationProcessor::~EngineImageSerializationProcessor() {}

SkPixelSerializer* EngineImageSerializationProcessor::GetPixelSerializer() {
  return pixel_serializer_.get();
}

SkPicture::InstallPixelRefProc
EngineImageSerializationProcessor::GetPixelDeserializer() {
  return nullptr;
}

}  // namespace engine
}  // namespace blimp
