// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/zucchini_commands.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <utility>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/macros.h"
#include "components/zucchini/buffer_view.h"
#include "components/zucchini/crc32.h"
#include "components/zucchini/io_utils.h"
#include "components/zucchini/mapped_file.h"
#include "components/zucchini/patch_writer.h"
#include "components/zucchini/zucchini_integration.h"
#include "components/zucchini/zucchini_tools.h"

namespace {

/******** Command-line Switches ********/

constexpr char kSwitchDump[] = "dump";
constexpr char kSwitchKeep[] = "keep";
constexpr char kSwitchRaw[] = "raw";

}  // namespace

zucchini::status::Code MainGen(MainParams params) {
  CHECK_EQ(3U, params.file_paths.size());

  // TODO(huangs): Move implementation to zucchini_integration.cc.
  using base::File;
  File old_file(params.file_paths[0], File::FLAG_OPEN | File::FLAG_READ);
  zucchini::MappedFileReader old_image(std::move(old_file));
  if (old_image.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[0].value() << ": "
               << old_image.error();
    return zucchini::status::kStatusFileReadError;
  }
  File new_file(params.file_paths[1], File::FLAG_OPEN | File::FLAG_READ);
  zucchini::MappedFileReader new_image(std::move(new_file));
  if (new_image.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[1].value() << ": "
               << new_image.error();
    return zucchini::status::kStatusFileReadError;
  }
  zucchini::EnsemblePatchWriter patch_writer(old_image.region(),
                                             new_image.region());

  auto generate = params.command_line.HasSwitch(kSwitchRaw)
                      ? zucchini::GenerateRaw
                      : zucchini::GenerateEnsemble;
  zucchini::status::Code result =
      generate(old_image.region(), new_image.region(), &patch_writer);
  if (result != zucchini::status::kStatusSuccess) {
    params.out << "Fatal error encountered when generating patch." << std::endl;
    return result;
  }

  // By default, delete patch on destruction, to avoid having lingering files in
  // case of a failure. On Windows deletion can be done by the OS.
  File patch_file(params.file_paths[2], File::FLAG_CREATE_ALWAYS |
                                            File::FLAG_READ | File::FLAG_WRITE |
                                            File::FLAG_SHARE_DELETE |
                                            File::FLAG_CAN_DELETE_ON_CLOSE);
  zucchini::MappedFileWriter patch(params.file_paths[2], std::move(patch_file),
                                   patch_writer.SerializedSize());
  if (patch.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[2].value() << ": "
               << patch.error();
    return zucchini::status::kStatusFileWriteError;
  }

  if (params.command_line.HasSwitch(kSwitchKeep))
    patch.Keep();

  if (!patch_writer.SerializeInto(patch.region()))
    return zucchini::status::kStatusPatchWriteError;

  // Successfully created patch. Explicitly request file to be kept.
  if (!patch.Keep())
    return zucchini::status::kStatusFileWriteError;
  return zucchini::status::kStatusSuccess;
}

zucchini::status::Code MainApply(MainParams params) {
  CHECK_EQ(3U, params.file_paths.size());
  return zucchini::Apply(params.file_paths[0], params.file_paths[1],
                         params.file_paths[2],
                         params.command_line.HasSwitch(kSwitchKeep));
}

zucchini::status::Code MainRead(MainParams params) {
  CHECK_EQ(1U, params.file_paths.size());
  base::File input_file(params.file_paths[0],
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  zucchini::MappedFileReader input(std::move(input_file));
  if (input.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[0].value() << ": "
               << input.error();
    return zucchini::status::kStatusFileReadError;
  }

  bool do_dump = params.command_line.HasSwitch(kSwitchDump);
  zucchini::status::Code status = zucchini::ReadReferences(
      {input.data(), input.length()}, do_dump, params.out);
  if (status != zucchini::status::kStatusSuccess)
    params.err << "Fatal error found when dumping references." << std::endl;
  return status;
}

zucchini::status::Code MainDetect(MainParams params) {
  CHECK_EQ(1U, params.file_paths.size());
  base::File input_file(params.file_paths[0],
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  zucchini::MappedFileReader input(std::move(input_file));
  if (input.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[0].value() << ": "
               << input.error();
    return zucchini::status::kStatusFileReadError;
  }

  std::vector<zucchini::ConstBufferView> sub_image_list;
  zucchini::status::Code result = zucchini::DetectAll(
      {input.data(), input.length()}, params.out, &sub_image_list);
  if (result != zucchini::status::kStatusSuccess)
    params.err << "Fatal error found when detecting executables." << std::endl;
  return result;
}

zucchini::status::Code MainMatch(MainParams params) {
  CHECK_EQ(2U, params.file_paths.size());
  using base::File;
  File old_file(params.file_paths[0], File::FLAG_OPEN | File::FLAG_READ);
  zucchini::MappedFileReader old_image(std::move(old_file));
  if (old_image.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[0].value() << ": "
               << old_image.error();
    return zucchini::status::kStatusFileReadError;
  }
  File new_file(params.file_paths[1], File::FLAG_OPEN | File::FLAG_READ);
  zucchini::MappedFileReader new_image(std::move(new_file));
  if (new_image.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[1].value() << ": "
               << new_image.error();
    return zucchini::status::kStatusFileReadError;
  }
  zucchini::status::Code status =
      zucchini::MatchAll({old_image.data(), old_image.length()},
                         {new_image.data(), new_image.length()}, params.out);
  if (status != zucchini::status::kStatusSuccess)
    params.err << "Fatal error found when matching executables." << std::endl;
  return status;
}

zucchini::status::Code MainCrc32(MainParams params) {
  CHECK_EQ(1U, params.file_paths.size());
  base::File image_file(params.file_paths[0],
                        base::File::FLAG_OPEN | base::File::FLAG_READ);
  zucchini::MappedFileReader image(std::move(image_file));
  if (image.HasError()) {
    LOG(ERROR) << "Error with file " << params.file_paths[0].value() << ": "
               << image.error();
    return zucchini::status::kStatusFileReadError;
  }

  uint32_t crc =
      zucchini::CalculateCrc32(image.data(), image.data() + image.length());
  params.out << "CRC32: " << zucchini::AsHex<8>(crc) << std::endl;
  return zucchini::status::kStatusSuccess;
}
