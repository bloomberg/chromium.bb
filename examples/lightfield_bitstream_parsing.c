/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

// Lightfield Bitstream Parsing
// ============================
//
// This is an lightfield bitstream parsing example. It takes an input file
// containing the whole compressed lightfield bitstream(ivf file), and parses it
// and constructs and outputs a new bitstream that can be decoded by an AV1
// decoder. The output bitstream contains tile list OBUs. The lf_width and
// lf_height arguments are the number of lightfield images in each dimension.
// The lf_blocksize determines the number of reference images used.
// After running the lightfield encoder, run lightfield bitstream parsing:
// examples/lightfield_bitstream_parsing vase10x10.ivf vase_tile_list.ivf 10 10
// 5

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "aom/aom_decoder.h"
#include "aom/aom_encoder.h"
#include "aom/aomdx.h"
#include "common/tools_common.h"
#include "common/video_reader.h"
#include "common/video_writer.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(
      stderr,
      "Usage: %s <infile> <outfile> <lf_width> <lf_height> <lf_blocksize> \n",
      exec_name);
  exit(EXIT_FAILURE);
}

#define ALIGN_POWER_OF_TWO(value, n) \
  (((value) + ((1 << (n)) - 1)) & ~((1 << (n)) - 1))

// SB size: 64x64
const uint8_t output_frame_width_in_tiles_minus_1 = 512 / 64;
const uint8_t output_frame_height_in_tiles_minus_1 = 512 / 64;
const uint16_t tile_count_minus_1 = 3;

// Spec:
// typedef struct {
//   uint8_t anchor_frame_idx;
//   uint8_t tile_row;
//   uint8_t tile_col;
//   uint16_t coded_tile_data_size_minus_1;
//   uint8_t *coded_tile_data;
// } TILE_LIST_ENTRY;

// Tile list entry provided by the application
typedef struct {
  int image_idx;
  int reference_idx;
  int tile_col;
  int tile_row;
} TILE_LIST_INFO;

// M references: 0 - M-1; N images(including references): 0 - N-1;
// Note: order the image index incrementally, so that we only go through the
// bitstream once to construct the tile list.
const int num_tile_lists = 2;
const TILE_LIST_INFO tile_list[2][4] = {
  { { 0, 0, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 }, { 0, 0, 1, 1 } },
  { { 1, 0, 0, 0 }, { 1, 0, 1, 0 }, { 1, 0, 0, 1 }, { 1, 0, 1, 1 } },
};

int main(int argc, char **argv) {
  aom_codec_ctx_t codec;
  AvxVideoReader *reader = NULL;
  AvxVideoWriter *writer = NULL;
  const AvxInterface *decoder = NULL;
  const AvxVideoInfo *info = NULL;
  const char *lf_width_arg;
  const char *lf_height_arg;
  const char *lf_blocksize_arg;
  int width, height;
  int lf_width, lf_height;
  int lf_blocksize;
  int u_blocks, v_blocks;
  int n, i;

  exec_name = argv[0];
  if (argc != 6) die("Invalid number of arguments.");

  reader = aom_video_reader_open(argv[1]);
  if (!reader) die("Failed to open %s for reading.", argv[1]);

  lf_width_arg = argv[3];
  lf_height_arg = argv[4];
  lf_blocksize_arg = argv[5];

  lf_width = (int)strtol(lf_width_arg, NULL, 0);
  lf_height = (int)strtol(lf_height_arg, NULL, 0);
  lf_blocksize = (int)strtol(lf_blocksize_arg, NULL, 0);

  info = aom_video_reader_get_info(reader);
  width = info->frame_width;
  height = info->frame_height;

  // The writer to write out ivf file in tile list OBU, which can be decoded by
  // AV1 decoder.
  writer = aom_video_writer_open(argv[2], kContainerIVF, info);
  if (!writer) die("Failed to open %s for writing", argv[2]);

  decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) die("Unknown input codec.");
  printf("Using %s\n", aom_codec_iface_name(decoder->codec_interface()));

  if (aom_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0))
    die_codec(&codec, "Failed to initialize decoder.");

  // Decode anchor frames.
  aom_codec_control_(&codec, AV1_SET_TILE_MODE, 0);

  // How many anchor frames we have.
  u_blocks = (lf_width + lf_blocksize - 1) / lf_blocksize;
  v_blocks = (lf_height + lf_blocksize - 1) / lf_blocksize;

  int num_references = v_blocks * u_blocks;
  for (i = 0; i < num_references; ++i) {
    aom_video_reader_read_frame(reader);

    size_t frame_size = 0;
    const unsigned char *frame =
        aom_video_reader_get_frame(reader, &frame_size);
    aom_codec_pts_t pts =
        (aom_codec_pts_t)aom_video_reader_get_frame_pts(reader);

    // Copy references bitstream directly.
    if (!aom_video_writer_write_frame(writer, frame, frame_size, pts))
      die_codec(&codec, "Failed to copy compressed anchor frame.");

    if (aom_codec_decode(&codec, frame, frame_size, NULL))
      die_codec(&codec, "Failed to decode frame.");
  }

  // Decode camera frames.
  aom_codec_control_(&codec, AV1_SET_TILE_MODE, 1);

  // Allocate a buffer to store tile list bitstream. Image format
  // AOM_IMG_FMT_I420.
  size_t data_sz =
      ALIGN_POWER_OF_TWO(width, 5) * ALIGN_POWER_OF_TWO(height, 5) * 12 / 8;
  unsigned char *tl_buf = (unsigned char *)malloc(data_sz);
  if (tl_buf == NULL) die_codec(&codec, "Failed to allocate tile list buffer.");

  FILE *infile = aom_video_reader_get_file(reader);
  // Record the offset of the first camera image.
  const FileOffset camera_frame_pos = ftello(infile);
  aom_codec_pts_t tl_pts = 0;
  int camera_frame_header_received = 0;

  // Process 1 tile list.
  for (n = 0; n < num_tile_lists; n++) {
    unsigned char *tl = tl_buf;
    uint32_t tile_list_obu_size = 0;
    int frame_cnt = -1;
    unsigned char *saved_obu_size_loc = NULL;

    // Seek to the first camera image.
    fseeko(infile, camera_frame_pos, SEEK_SET);

    // Start to construct tile list OBU
    // Write tile list OBU header - 1 byte long. Details:
    // aom_wb_write_literal(&wb, 0, 1);  // forbidden bit.
    // aom_wb_write_literal(&wb, (int)obu_type, 4);  // "1000"
    // aom_wb_write_literal(&wb, 0, 1);  // obu_extension = 0
    // aom_wb_write_literal(&wb, 1, 1);  // obu_has_payload_length_field
    // aom_wb_write_literal(&wb, 0, 1);  // reserved
    *tl++ = 66;
    // Write OBU size - length_field_size is always 4 byte long.
    saved_obu_size_loc = tl;
    tl += 4;

    // write_tile_list_obu()
    *tl++ = output_frame_width_in_tiles_minus_1;
    *tl++ = output_frame_height_in_tiles_minus_1;
    *(uint16_t *)tl++ = tile_count_minus_1;
    tile_list_obu_size += 4;

    // Write each tile's data
    for (i = 0; i <= tile_count_minus_1; i++) {
      aom_tile_data tile_data = { 0, NULL };

      int image_idx = tile_list[n][i].image_idx;
      int ref_idx = tile_list[n][i].reference_idx;
      int tc = tile_list[n][i].tile_col;
      int tr = tile_list[n][i].tile_row;

      // Read out the camera image
      assert(frame_cnt <= image_idx);
      while (frame_cnt != image_idx) {
        aom_video_reader_read_frame(reader);
        frame_cnt++;

        // Copy first camera frame for getting camera frame header. This is done
        // only once.
        if (frame_cnt == 0 && !camera_frame_header_received) {
          size_t frame_size = 0;
          const unsigned char *frame =
              aom_video_reader_get_frame(reader, &frame_size);
          aom_codec_pts_t pts =
              (aom_codec_pts_t)aom_video_reader_get_frame_pts(reader);

          // Copy camera frame bitstream directly to get the header.
          if (!aom_video_writer_write_frame(writer, frame, frame_size, pts))
            die_codec(&codec, "Failed to copy compressed camera frame.");

          camera_frame_header_received = 1;
        }
      }

      size_t frame_size = 0;
      const unsigned char *frame =
          aom_video_reader_get_frame(reader, &frame_size);

      aom_codec_control_(&codec, AV1_SET_DECODE_TILE_ROW, tr);
      aom_codec_control_(&codec, AV1_SET_DECODE_TILE_COL, tc);

      aom_codec_err_t aom_status =
          aom_codec_decode(&codec, frame, frame_size, NULL);
      if (aom_status) die_codec(&codec, "Failed to decode tile.");

      aom_codec_control_(&codec, AV1D_GET_TILE_DATA, &tile_data);

      // Copy over tile data.
      //  uint8_t anchor_frame_idx;
      //  uint8_t tile_row;
      //  uint8_t tile_col;
      //  uint16_t coded_tile_data_size_minus_1;
      //  uint8_t *coded_tile_data;
      *tl++ = ref_idx;
      *tl++ = tr;
      *tl++ = tc;
      *(uint16_t *)tl++ = (uint16_t)(tile_data.coded_tile_data_size - 1);
      memcpy(tl, (uint8_t *)tile_data.coded_tile_data,
             tile_data.coded_tile_data_size);
      tile_list_obu_size += 5 + (uint32_t)tile_data.coded_tile_data_size;
    }

    // Write tile list OBU size.
    *(uint32_t *)saved_obu_size_loc = tile_list_obu_size;

    // Copy camera frame bitstream directly to get the header.
    if (!aom_video_writer_write_frame(writer, tl_buf, tile_list_obu_size,
                                      tl_pts))
      die_codec(&codec, "Failed to copy compressed tile list.");

    tl_pts++;
  }

  free(tl_buf);
  if (aom_codec_destroy(&codec)) die_codec(&codec, "Failed to destroy codec");
  aom_video_writer_close(writer);
  aom_video_reader_close(reader);

  return EXIT_SUCCESS;
}
