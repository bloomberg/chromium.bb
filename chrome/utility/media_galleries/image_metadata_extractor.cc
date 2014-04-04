// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/image_metadata_extractor.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "media/base/data_source.h"
#include "net/base/io_buffer.h"

extern "C" {
#include <libexif/exif-data.h>
}  // extern "C"

namespace metadata {

namespace {

const int kMaxBufferSize = 50 * 1024 * 1024;  // Arbitrary maximum of 50MB.

void FinishGetImageBytes(
    net::DrainableIOBuffer* buffer,
    media::DataSource* source,
    const base::Callback<void(net::DrainableIOBuffer*)>& callback,
    int bytes_read) {
  if (bytes_read == media::DataSource::kReadError) {
    callback.Run(NULL);
    return;
  }

  buffer->DidConsume(bytes_read);
  // Didn't get the whole file. Continue reading to get the rest.
  if (buffer->BytesRemaining() > 0) {
    source->Read(0, buffer->BytesRemaining(), (uint8*)buffer->data(),
                 base::Bind(&FinishGetImageBytes, make_scoped_refptr(buffer),
                            base::Unretained(source), callback));
    return;
  }

  buffer->SetOffset(0);
  callback.Run(make_scoped_refptr(buffer));
}

void GetImageBytes(
    media::DataSource* source,
    const base::Callback<void(net::DrainableIOBuffer*)>& callback) {
  int64 size64 = 0;
  if (!source->GetSize(&size64) || size64 > kMaxBufferSize)
    return callback.Run(NULL);
  int size = base::checked_cast<int>(size64);

  scoped_refptr<net::DrainableIOBuffer> buffer(
      new net::DrainableIOBuffer(new net::IOBuffer(size), size));
  source->Read(0, buffer->BytesRemaining(), (uint8*)buffer->data(),
               base::Bind(&FinishGetImageBytes, buffer,
                          base::Unretained(source), callback));
}

void ExtractInt(ExifData* data, ExifTag tag, int* result) {
  DCHECK(result);

  ExifEntry* entry = exif_data_get_entry(data, tag);

  if (entry) {
    ExifByteOrder order = exif_data_get_byte_order(data);

    switch (entry->format) {
      case EXIF_FORMAT_SHORT: {
        ExifShort v = exif_get_short(entry->data, order);
        *result = base::checked_cast<int>(v);
        break;
      }
      case EXIF_FORMAT_LONG: {
        ExifLong v = exif_get_long(entry->data, order);
        // Ignore values that don't fit in a signed int - probably invalid data.
        if (base::IsValueInRangeForNumericType<int>(v))
          *result = base::checked_cast<int>(v);
        break;
      }
      default: {
        // Ignore all other entry formats.
      }
    }
  }
}

void ExtractDouble(ExifData* data, ExifTag tag, double* result) {
  DCHECK(result);

  ExifEntry* entry = exif_data_get_entry(data, tag);

  if (entry) {
    ExifByteOrder order = exif_data_get_byte_order(data);

    if (entry->format == EXIF_FORMAT_RATIONAL) {
      ExifRational v = exif_get_rational(entry->data, order);
      *result = base::checked_cast<double>(v.numerator) /
          base::checked_cast<double>(v.denominator);
    }
  }
}

void ExtractString(ExifData* data, ExifTag tag, std::string* result) {
  DCHECK(result);

  ExifEntry* entry = exif_data_get_entry(data, tag);

  if (entry) {
    char buf[1024];
    exif_entry_get_value(entry, buf, sizeof(buf));
    *result = buf;
  }
}

}  // namespace

ImageMetadataExtractor::ImageMetadataExtractor()
    : extracted_(false),
      width_(-1),
      height_(-1),
      rotation_(-1),
      x_resolution_(-1),
      y_resolution_(-1),
      exposure_time_sec_(-1),
      flash_fired_(false),
      f_number_(-1),
      focal_length_mm_(-1),
      iso_equivalent_(-1) {
}

ImageMetadataExtractor::~ImageMetadataExtractor() {
}

void ImageMetadataExtractor::Extract(media::DataSource* source,
                                     const DoneCallback& callback) {
  DCHECK(!extracted_);

  GetImageBytes(source, base::Bind(&ImageMetadataExtractor::FinishExtraction,
                                   base::Unretained(this), callback));

}

int ImageMetadataExtractor::width() const {
  DCHECK(extracted_);
  return width_;
}

int ImageMetadataExtractor::height() const {
  DCHECK(extracted_);
  return height_;
}

int ImageMetadataExtractor::rotation() const {
  DCHECK(extracted_);
  return rotation_;
}

double ImageMetadataExtractor::x_resolution() const {
  DCHECK(extracted_);
  return x_resolution_;
}

double ImageMetadataExtractor::y_resolution() const {
  DCHECK(extracted_);
  return y_resolution_;
}

const std::string& ImageMetadataExtractor::date() const {
  DCHECK(extracted_);
  return date_;
}

const std::string& ImageMetadataExtractor::camera_make() const {
  DCHECK(extracted_);
  return camera_make_;
}

const std::string& ImageMetadataExtractor::camera_model() const {
  DCHECK(extracted_);
  return camera_model_;
}

double ImageMetadataExtractor::exposure_time_sec() const {
  DCHECK(extracted_);
  return exposure_time_sec_;
}

bool ImageMetadataExtractor::flash_fired() const {
  DCHECK(extracted_);
  return flash_fired_;
}

double ImageMetadataExtractor::f_number() const {
  DCHECK(extracted_);
  return f_number_;
}

double ImageMetadataExtractor::focal_length_mm() const {
  DCHECK(extracted_);
  return focal_length_mm_;
}

int ImageMetadataExtractor::iso_equivalent() const {
  DCHECK(extracted_);
  return iso_equivalent_;
}

void ImageMetadataExtractor::FinishExtraction(
    const DoneCallback& callback, net::DrainableIOBuffer* buffer) {
  if (!buffer) {
    callback.Run(false);
    return;
  }

  ExifData* data = exif_data_new_from_data(
      (unsigned char*)buffer->data(), buffer->BytesRemaining());

  if (!data) {
    callback.Run(false);
    return;
  }

  ExtractInt(data, EXIF_TAG_IMAGE_WIDTH, &width_);
  ExtractInt(data, EXIF_TAG_IMAGE_LENGTH, &height_);

  // We ignore the mirrored-aspect of the mirrored-orientations and just
  // indicate the rotation. Mirrored-orientations are very rare.
  int orientation = 0;
  ExtractInt(data, EXIF_TAG_ORIENTATION, &orientation);
  switch (orientation) {
    case 1:
    case 2:
      rotation_ = 0;
      break;
    case 3:
    case 4:
      rotation_ = 180;
      break;
    case 5:
    case 6:
      rotation_ = 90;
      break;
    case 7:
    case 8:
      rotation_ = 270;
      break;
  }

  ExtractDouble(data, EXIF_TAG_X_RESOLUTION, &x_resolution_);
  ExtractDouble(data, EXIF_TAG_Y_RESOLUTION, &y_resolution_);

  ExtractString(data, EXIF_TAG_DATE_TIME, &date_);

  ExtractString(data, EXIF_TAG_MAKE, &camera_make_);
  ExtractString(data, EXIF_TAG_MODEL, &camera_model_);
  ExtractDouble(data, EXIF_TAG_EXPOSURE_TIME, &exposure_time_sec_);

  int flash_value = -1;
  ExtractInt(data, EXIF_TAG_FLASH, &flash_value);
  if (flash_value >= 0) {
    flash_fired_ = (flash_value & 0x1) != 0;
  }

  ExtractDouble(data, EXIF_TAG_FNUMBER, &f_number_);
  ExtractDouble(data, EXIF_TAG_FOCAL_LENGTH, &focal_length_mm_);
  ExtractInt(data, EXIF_TAG_ISO_SPEED_RATINGS, &iso_equivalent_);

  exif_data_free(data);

  extracted_ = true;

  callback.Run(true);
}

}  // namespace metadata
