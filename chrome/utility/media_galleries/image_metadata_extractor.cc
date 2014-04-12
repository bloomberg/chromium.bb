// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/utility/media_galleries/image_metadata_extractor.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/common/content_paths.h"
#include "media/base/data_source.h"
#include "net/base/io_buffer.h"

extern "C" {
#include <libexif/exif-data.h>
#include <libexif/exif-loader.h>
}  // extern "C"

namespace metadata {

namespace {

const size_t kMaxBufferSize = 50 * 1024 * 1024;  // Arbitrary maximum of 50MB.

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
    source->Read(0, buffer->BytesRemaining(),
                 reinterpret_cast<uint8*>(buffer->data()),
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
  if (!source->GetSize(&size64) ||
      base::saturated_cast<size_t>(size64) > kMaxBufferSize) {
    return callback.Run(NULL);
  }
  int size = base::checked_cast<int>(size64);

  scoped_refptr<net::DrainableIOBuffer> buffer(
      new net::DrainableIOBuffer(new net::IOBuffer(size), size));
  source->Read(0, buffer->BytesRemaining(),
               reinterpret_cast<uint8*>(buffer->data()),
               base::Bind(&FinishGetImageBytes, buffer,
                          base::Unretained(source), callback));
}

class ExifFunctions {
 public:
  ExifFunctions() : exif_loader_write_func_(NULL),
                    exif_loader_new_func_(NULL),
                    exif_loader_unref_func_(NULL),
                    exif_loader_get_data_func_(NULL),
                    exif_data_free_func_(NULL),
                    exif_data_get_byte_order_func_(NULL),
                    exif_get_short_func_(NULL),
                    exif_get_long_func_(NULL),
                    exif_get_rational_func_(NULL),
                    exif_entry_get_value_func_(NULL),
                    exif_content_get_entry_func_(NULL) {
  }

  bool Initialize(const base::FilePath& module_dir) {
    if (exif_lib_.is_valid())
      return true;

#if defined(OS_WIN)
    base::FilePath module_path = module_dir.AppendASCII("libexif.dll");
#elif defined(OS_MACOSX)
    base::FilePath module_path = module_dir.AppendASCII("exif.so");
#elif defined(OS_CHROMEOS)
    // On ChromeOS, we build and distribute our own version of libexif.
    base::FilePath module_path = module_dir.AppendASCII("libexif.so");
#else
    // On Linux-like systems, we use the system libexif.
    base::FilePath module_path = base::FilePath().AppendASCII("libexif.so.12");
#endif

    base::ScopedNativeLibrary lib(base::LoadNativeLibrary(module_path, NULL));
    if (!lib.is_valid()) {
      LOG(ERROR) << "Couldn't load libexif.";
      return false;
    }

    if (!GetFunctionPointer(lib, &exif_loader_write_func_,
                            "exif_loader_write") ||
        !GetFunctionPointer(lib, &exif_loader_new_func_, "exif_loader_new") ||
        !GetFunctionPointer(lib, &exif_loader_unref_func_,
                            "exif_loader_unref") ||
        !GetFunctionPointer(lib, &exif_loader_get_data_func_,
                            "exif_loader_get_data") ||
        !GetFunctionPointer(lib, &exif_data_free_func_, "exif_data_free") ||
        !GetFunctionPointer(lib, &exif_data_get_byte_order_func_,
                            "exif_data_get_byte_order") ||
        !GetFunctionPointer(lib, &exif_get_short_func_, "exif_get_short") ||
        !GetFunctionPointer(lib, &exif_get_long_func_, "exif_get_long") ||
        !GetFunctionPointer(lib, &exif_get_rational_func_,
                            "exif_get_rational") ||
        !GetFunctionPointer(lib, &exif_entry_get_value_func_,
                            "exif_entry_get_value") ||
        !GetFunctionPointer(lib, &exif_content_get_entry_func_,
                            "exif_content_get_entry")) {
      return false;
    }

    exif_lib_.Reset(lib.Release());
    return true;
  }

  ExifData* ParseExifFromBuffer(unsigned char* buffer, unsigned int size) {
    DCHECK(exif_lib_.is_valid());
    ExifLoader* loader = exif_loader_new_func_();
    exif_loader_write_func_(loader, buffer, size);

    ExifData* data = exif_loader_get_data_func_(loader);

    exif_loader_unref_func_(loader);
    loader = NULL;

    return data;
  }

  void ExifDataFree(ExifData* data) {
    DCHECK(exif_lib_.is_valid());
    return exif_data_free_func_(data);
  }

  void ExtractInt(ExifData* data, ExifTag tag, int* result) {
    DCHECK(exif_lib_.is_valid());
    DCHECK(result);

    ExifEntry* entry = ExifContentGetEntry(data, tag);
    if (!entry)
      return;

    ExifByteOrder order = exif_data_get_byte_order_func_(data);
    switch (entry->format) {
      case EXIF_FORMAT_SHORT: {
        ExifShort v = exif_get_short_func_(entry->data, order);
        *result = base::checked_cast<int>(v);
        break;
      }
      case EXIF_FORMAT_LONG: {
        ExifLong v = exif_get_long_func_(entry->data, order);
        // Ignore values that don't fit in a signed int - likely invalid data.
        if (base::IsValueInRangeForNumericType<int>(v))
          *result = base::checked_cast<int>(v);
        break;
      }
      default: {
        // Ignore all other entry formats.
      }
    }
  }

  void ExtractDouble(ExifData* data, ExifTag tag, double* result) {
    DCHECK(exif_lib_.is_valid());
    DCHECK(result);

    ExifEntry* entry = ExifContentGetEntry(data, tag);
    if (!entry)
      return;

    ExifByteOrder order = exif_data_get_byte_order_func_(data);

    if (entry->format == EXIF_FORMAT_RATIONAL) {
      ExifRational v = exif_get_rational_func_(entry->data, order);
      *result = base::checked_cast<double>(v.numerator) /
          base::checked_cast<double>(v.denominator);
    }
  }

  void ExtractString(ExifData* data, ExifTag tag, std::string* result) {
    DCHECK(exif_lib_.is_valid());
    DCHECK(result);

    ExifEntry* entry = ExifContentGetEntry(data, tag);
    if (!entry)
      return;

    char buf[1024];
    exif_entry_get_value_func_(entry, buf, sizeof(buf));
    *result = buf;
  }

 private:
  // Exported by libexif.
  typedef unsigned char (*ExifLoaderWriteFunc)(
      ExifLoader *eld, unsigned char *buf, unsigned int len);
  typedef ExifLoader* (*ExifLoaderNewFunc)();
  typedef void (*ExifLoaderUnrefFunc)(ExifLoader* loader);
  typedef ExifData* (*ExifLoaderGetDataFunc)(ExifLoader* loader);
  typedef void (*ExifDataFreeFunc)(ExifData* data);
  typedef ExifByteOrder (*ExifDataGetByteOrderFunc)(ExifData* data);
  typedef ExifShort (*ExifGetShortFunc)(const unsigned char *buf,
                                        ExifByteOrder order);
  typedef ExifLong (*ExifGetLongFunc)(const unsigned char *buf,
                                      ExifByteOrder order);
  typedef ExifRational (*ExifGetRationalFunc)(const unsigned char *buf,
                                              ExifByteOrder order);
  typedef const char* (*ExifEntryGetValueFunc)(ExifEntry *e, char *val,
                                               unsigned int maxlen);
  typedef ExifEntry* (*ExifContentGetEntryFunc)(ExifContent* content,
                                                ExifTag tag);

  template<typename FunctionType>
  bool GetFunctionPointer(const base::ScopedNativeLibrary& lib,
                          FunctionType* function, const char* name) {
    DCHECK(lib.is_valid());
    DCHECK(function);
    DCHECK(!(*function));
    *function = reinterpret_cast<FunctionType>(
        lib.GetFunctionPointer(name));
    DLOG_IF(WARNING, !(*function)) << "Missing " << name;
    return *function != NULL;
  }

  // Redefines exif_content_get_entry macro in terms of function pointer.
  ExifEntry* ExifContentGetEntry(ExifData* data, ExifTag tag) {
    DCHECK(exif_lib_.is_valid());
    const ExifIfd ifds[] =
        { EXIF_IFD_0, EXIF_IFD_1, EXIF_IFD_EXIF, EXIF_IFD_GPS };

    for (size_t i = 0; i < arraysize(ifds); ++i) {
      ExifEntry* entry = exif_content_get_entry_func_(data->ifd[ifds[i]], tag);
      if (entry)
        return entry;
    }

    return NULL;
  }

  ExifLoaderWriteFunc exif_loader_write_func_;
  ExifLoaderNewFunc exif_loader_new_func_;
  ExifLoaderUnrefFunc exif_loader_unref_func_;
  ExifLoaderGetDataFunc exif_loader_get_data_func_;
  ExifDataFreeFunc exif_data_free_func_;
  ExifDataGetByteOrderFunc exif_data_get_byte_order_func_;
  ExifGetShortFunc exif_get_short_func_;
  ExifGetLongFunc exif_get_long_func_;
  ExifGetRationalFunc exif_get_rational_func_;
  ExifEntryGetValueFunc exif_entry_get_value_func_;
  ExifContentGetEntryFunc exif_content_get_entry_func_;

  base::ScopedNativeLibrary exif_lib_;
  DISALLOW_COPY_AND_ASSIGN(ExifFunctions);
};

static base::LazyInstance<ExifFunctions> g_exif_lib = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
bool ImageMetadataExtractor::InitializeLibrary() {
  base::FilePath media_path;
  if (!PathService::Get(content::DIR_MEDIA_LIBS, &media_path))
    return false;
  return g_exif_lib.Get().Initialize(media_path);
}

// static
bool ImageMetadataExtractor::InitializeLibraryForTesting() {
  base::FilePath module_dir;
  if (!PathService::Get(base::DIR_EXE, &module_dir))
    return false;
  return g_exif_lib.Get().Initialize(module_dir);
}

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

  ExifData* data = g_exif_lib.Get().ParseExifFromBuffer(
      reinterpret_cast<unsigned char*>(buffer->data()),
      buffer->BytesRemaining());

  if (!data) {
    callback.Run(false);
    return;
  }

  g_exif_lib.Get().ExtractInt(data, EXIF_TAG_IMAGE_WIDTH, &width_);
  g_exif_lib.Get().ExtractInt(data, EXIF_TAG_IMAGE_LENGTH, &height_);

  // We ignore the mirrored-aspect of the mirrored-orientations and just
  // indicate the rotation. Mirrored-orientations are very rare.
  int orientation = 0;
  g_exif_lib.Get().ExtractInt(data, EXIF_TAG_ORIENTATION, &orientation);
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

  g_exif_lib.Get().ExtractDouble(data, EXIF_TAG_X_RESOLUTION, &x_resolution_);
  g_exif_lib.Get().ExtractDouble(data, EXIF_TAG_Y_RESOLUTION, &y_resolution_);

  g_exif_lib.Get().ExtractString(data, EXIF_TAG_DATE_TIME, &date_);

  g_exif_lib.Get().ExtractString(data, EXIF_TAG_MAKE, &camera_make_);
  g_exif_lib.Get().ExtractString(data, EXIF_TAG_MODEL, &camera_model_);
  g_exif_lib.Get().ExtractDouble(data, EXIF_TAG_EXPOSURE_TIME,
                                 &exposure_time_sec_);

  int flash_value = -1;
  g_exif_lib.Get().ExtractInt(data, EXIF_TAG_FLASH, &flash_value);
  if (flash_value >= 0) {
    flash_fired_ = (flash_value & 0x1) != 0;
  }

  g_exif_lib.Get().ExtractDouble(data, EXIF_TAG_FNUMBER, &f_number_);
  g_exif_lib.Get().ExtractDouble(data, EXIF_TAG_FOCAL_LENGTH,
                                 &focal_length_mm_);
  g_exif_lib.Get().ExtractInt(data, EXIF_TAG_ISO_SPEED_RATINGS,
                              &iso_equivalent_);

  g_exif_lib.Get().ExifDataFree(data);

  extracted_ = true;

  callback.Run(true);
}

}  // namespace metadata
