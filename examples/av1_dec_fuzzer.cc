/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/*
 * See build_av1_dec_fuzzer.sh for building instructions.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory>

#include "config/aom_config.h"
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "aom_ports/mem_ops.h"

#define IVF_FRAME_HDR_SZ (4 + 8) /* 4 byte size + 8 byte timestamp */
#define IVF_FILE_HDR_SZ 32

static void close_file(FILE *file) { fclose(file); }

/* read_frame is derived from ivf_read_frame in ivfdec.c
 * Returns 0 on success and 1 on failure.
 * This function doesn't call warn(), but instead ignores those errors.
 * This is done to minimize the prints on console when running fuzzer
 * Also if fread fails to read frame_size number of bytes, instead of
 * returning an error, this returns with partial frames.
 * This is done to ensure that partial frames are sent to decoder.
 */
static int read_frame(FILE *infile, uint8_t **buffer, size_t *bytes_read,
                      size_t *buffer_size) {
  char raw_header[IVF_FRAME_HDR_SZ] = { 0 };
  size_t frame_size = 0;

  if (fread(raw_header, IVF_FRAME_HDR_SZ, 1, infile) == 1) {
    frame_size = mem_get_le32(raw_header);

    if (frame_size > 256 * 1024 * 1024) {
      frame_size = 0;
    }

    if (frame_size > *buffer_size) {
      uint8_t *new_buffer = (uint8_t *)realloc(*buffer, 2 * frame_size);

      if (new_buffer) {
        *buffer = new_buffer;
        *buffer_size = 2 * frame_size;
      } else {
        frame_size = 0;
      }
    }
  }

  if (!feof(infile)) {
    *bytes_read = fread(*buffer, 1, frame_size, infile);
    return 0;
  }

  return 1;
}

extern "C" void usage_exit(void) { exit(EXIT_FAILURE); }

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
  std::unique_ptr<FILE, decltype(&close_file)> file(
      fmemopen((void *)data, size, "rb"), &close_file);
  if (file == nullptr) {
    return 0;
  }

  char header[32];
  if (fread(header, 1, 32, file.get()) != 32) {
    return 0;
  }

  const aom_codec_iface_t *codec_interface = aom_codec_av1_dx();
  aom_codec_ctx_t codec;
  // Set thread count in the range [1, 64].
  const unsigned int threads = (header[0] & 0x3f) + 1;
  aom_codec_dec_cfg_t cfg = { threads, 0, 0, CONFIG_LOWBITDEPTH, { 1 } };
  if (aom_codec_dec_init(&codec, codec_interface, &cfg, 0)) {
    return 0;
  }

  uint8_t *buffer = nullptr;
  size_t buffer_size = 0;
  size_t frame_size = 0;
  while (!read_frame(file.get(), &buffer, &frame_size, &buffer_size)) {
    const aom_codec_err_t err =
        aom_codec_decode(&codec, buffer, frame_size, nullptr);
    static_cast<void>(err);
    aom_codec_iter_t iter = nullptr;
    aom_image_t *img = nullptr;
    while ((img = aom_codec_get_frame(&codec, &iter)) != nullptr) {
    }
  }
  aom_codec_destroy(&codec);
  free(buffer);
  return 0;
}
