// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "gpu/skia_runner/in_process_graphics_system.h"
#include "gpu/skia_runner/sk_picture_rasterizer.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebImage.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/skia/include/core/SkOSFile.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/codec/png_codec.h"

#if defined(OS_MACOSX)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

namespace {

bool WriteSkImagePNG(const SkImage* image, const base::FilePath& path) {
  DCHECK(!path.empty());

  if (!image) {
    std::cout << "Unable to write empty bitmap for " << path.value() << ".\n";
    return false;
  }

  std::string file_path = path.MaybeAsASCII();
  SkFILEWStream stream(file_path.c_str());
  if (!stream.isValid()) {
    std::cout << "Unable to write to " << file_path.c_str() << ".\n";
    return false;
  }

  SkImageInfo info = SkImageInfo::Make(image->width(), image->height(),
                                       SkColorType::kBGRA_8888_SkColorType,
                                       SkAlphaType::kPremul_SkAlphaType);
  SkImageInfo::MakeN32Premul(image->width(), image->height());

  const size_t rowBytes = image->width() * sizeof(SkPMColor);
  std::vector<SkPMColor> pixels(image->width() * image->height());

  if (image->readPixels(info, pixels.data(), rowBytes, 0, 0)) {
    std::vector<unsigned char> png_data;

    if (gfx::PNGCodec::Encode(
            reinterpret_cast<const unsigned char*>(pixels.data()),
            gfx::PNGCodec::FORMAT_BGRA,
            gfx::Size(image->width(), image->height()),
            static_cast<int>(rowBytes), false,
            std::vector<gfx::PNGCodec::Comment>(), &png_data)) {
      if (stream.write(png_data.data(), png_data.size())) {
        return true;
      }
    }
  }

  return false;
}

bool onDecode(const void* buffer, size_t size, SkBitmap* bm) {
  blink::WebData web_data(static_cast<const char*>(buffer), size);
  blink::WebImage image = blink::WebImage::fromData(web_data, blink::WebSize());
  if (!image.isNull()) {
    *bm = image.getSkBitmap();
    return true;
  }
  std::cout << "Error decoding image.\n";
  return false;
}

skia::RefPtr<SkPicture> ReadPicture(const base::FilePath& path) {
  skia::RefPtr<SkPicture> picture;
  std::string file_path = path.MaybeAsASCII();
  SkAutoTDelete<SkStream> stream(SkStream::NewFromFile(file_path.c_str()));
  if (!stream.get()) {
    std::cout << "Unable to read " << file_path.c_str() << ".\n";
    return picture;
  }

  picture = skia::AdoptRef(SkPicture::CreateFromStream(stream.get(), onDecode));
  if (!picture) {
    std::cout << "Unable to load " << file_path.c_str()
              << " as an SkPicture.\n";
  }
  return picture;
}

base::FilePath MakeDestinationPNGFilename(base::FilePath source_file,
                                          base::FilePath destination_folder) {
  base::FilePath filename = source_file.BaseName().RemoveExtension();
  filename = filename.AddExtension(FILE_PATH_LITERAL("png"));
  return destination_folder.AsEndingWithSeparator().Append(filename);
}

std::vector<std::pair<base::FilePath, base::FilePath>> GetSkpsToRasterize(
    base::FilePath input_path,
    base::FilePath output_path) {
  std::vector<std::pair<base::FilePath, base::FilePath>> files;

  if (base::DirectoryExists(input_path)) {
    if (!output_path.empty() && !base::DirectoryExists(output_path))
      return files;

    base::FilePath::StringType extension = FILE_PATH_LITERAL(".skp");
    base::FileEnumerator file_iter(input_path, false,
                                   base::FileEnumerator::FILES);
    while (!file_iter.Next().empty()) {
      if (file_iter.GetInfo().GetName().MatchesExtension(extension)) {
        base::FilePath skp_file = file_iter.GetInfo().GetName();
        skp_file = input_path.AsEndingWithSeparator().Append(skp_file);
        base::FilePath png_file;
        if (!output_path.empty())
          png_file = MakeDestinationPNGFilename(skp_file, output_path);
        files.push_back(std::make_pair(skp_file, png_file));
      }
    }
  } else {
    // Single file passed. If the output file is a folder, make a name.
    if (base::DirectoryExists(output_path))
      output_path = MakeDestinationPNGFilename(input_path, output_path);
    files.push_back(std::make_pair(input_path, output_path));
  }
  return files;
}

int GetSwitchInt(const base::CommandLine* command_line,
                 std::string switch_name,
                 int default_value) {
  if (command_line->HasSwitch(switch_name)) {
    std::string value = command_line->GetSwitchValueASCII(switch_name);
    int result = default_value;
    if (base::StringToInt(value, &result))
      return result;
  }
  return default_value;
}

static const char kHelpMessage[] =
    "This program renders a skia SKP to a PNG using GPU rasterization via the\n"
    "command buffer.\n\n"
    "following command line flags to control its behavior:\n"
    "\n"
    "  --in-skp=skp[:DIRECTORY_PATH|FILE_PATH]\n"
    "      Input SKP file. If a directory is provided, all SKP files will be\n"
    "      converted.\n"
    "  --out-png=png[:DIRECTORY_PATH|:FILE_PATH]\n"
    "      Output PNG file. If a directory is provided, the SKP filename is "
    "used.\n\n"
    "  --use-lcd-text\n"
    "      Turn on lcd text rendering.\n"
    "  --use-distance-field-text\n"
    "      Turn on distance field text rendering.\n"
    "  --msaa-sample-count=(0|2|4|8|16)\n"
    "      Turn on multi-sample anti-aliasing.\n"
    "  --use-gl=(desktop|osmesa|egl|swiftshader)\n"
    "      Specify Gl driver. --swiftshader-path required for swiftshader.\n"
    "  --repeat-raster-count=N\n"
    "      Specify the number of times to repeat rasterization for timing.\n";

}  // namespace anonymous

int main(int argc, char** argv) {
  base::AtExitManager exit_manager;
  base::CommandLine::Init(argc, argv);
#if defined(OS_MACOSX)
  base::mac::ScopedNSAutoreleasePool autorelease_pool;
#endif
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  base::FilePath input_path(command_line->GetSwitchValuePath("in-skp"));
  base::FilePath output_path(command_line->GetSwitchValuePath("out-png"));

  if (input_path.empty()) {
    std::cout << kHelpMessage;
    return 0;
  }

  if (output_path.empty())
    std::cout << "--out-png path not specified.  Timing rasterization only.\n";

  std::vector<std::pair<base::FilePath, base::FilePath>> files =
      GetSkpsToRasterize(input_path, output_path);

  if (files.empty()) {
    if (!base::DirectoryExists(output_path))
      std::cout << "You must specify an existing directory using '--out-png'\n";
    else
      std::cout << "No skp files found at " << input_path.value() << "\n";
    return 0;
  }

  skia_runner::InProcessGraphicsSystem graphics_system;
  if (!graphics_system.IsSuccessfullyInitialized()) {
    LOG(ERROR) << "Unable to initialize rasterizer.";
    return 0;
  }

  skia_runner::SkPictureRasterizer picture_rasterizer(
      graphics_system.GetGrContext(), graphics_system.GetMaxTextureSize());

  // Set up command-line render options.
  picture_rasterizer.set_use_lcd_text(command_line->HasSwitch("use-lcd-text"));
  picture_rasterizer.set_use_distance_field_text(
      command_line->HasSwitch("use-distance-field-text"));

  int msaa = GetSwitchInt(command_line, "msaa-sample-count", 0);
  if (msaa != 0 && msaa != 2 && msaa != 4 && msaa != 8 && msaa != 16) {
    std::cout << "Error: msaa sample count must be 0, 2, 4, 8 or 16.\n";
    return 0;
  }
  picture_rasterizer.set_msaa_sample_count(msaa);

  int repeat_raster_count =
      std::max(1, GetSwitchInt(command_line, "repeat-raster-count", 1));

  // Disable the security precautions to ensure we correctly read the picture.
  SkPicture::SetPictureIOSecurityPrecautionsEnabled_Dangerous(false);

  for (auto file_pair : files) {
    skia::RefPtr<SkPicture> picture = ReadPicture(file_pair.first);
    if (!picture) {
      std::cout << "Error reading: " << file_pair.first.value() << "\n";
      continue;
    }

    // GPU Rasterize the picture and time it.
    base::TimeTicks rasterize_start = base::TimeTicks::Now();
    skia::RefPtr<SkImage> image;
    for (int i = 0; i < repeat_raster_count; ++i) {
      image = picture_rasterizer.Rasterize(picture.get());
      graphics_system.GetGrContext()->flush();
    }
    base::TimeDelta rasterize_time = base::TimeTicks::Now() - rasterize_start;
    double average_time = rasterize_time.InSecondsF() / repeat_raster_count;
    std::cout << "Average rasterization time for " << file_pair.first.value()
              << ": " << average_time << "s (" << 1.0 / average_time
              << " fps)\n";

    if (!file_pair.second.empty()) {
      if (!WriteSkImagePNG(image.get(), file_pair.second))
        std::cout << "Error writing: " << file_pair.second.value() << "\n";
      else
        std::cout << file_pair.second.value() << " successfully created.\n";
    }
  }
}
