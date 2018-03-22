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

/*!\file
 * \brief This is an sample binary to create noise params from input video.
 *
 * To allow for external denoising applications, this sample binary illustrates
 * how to create a film grain table (film grain params as a function of time)
 * from an input video and its corresponding denoised source.
 *
 * The --output-grain-table file can be passed as input to the encoder (in
 * aomenc this is done through the "--film-grain-table" parameter).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../args.h"
#include "../tools_common.h"
#include "../video_writer.h"
#include "aom/aom_encoder.h"
#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/noise_model.h"
#include "aom_dsp/noise_util.h"
#include "aom_dsp/grain_table.h"
#include "aom_mem/aom_mem.h"

static const char *exec_name;

void usage_exit(void) {
  fprintf(stderr,
          "Usage: %s --input=<input> --input-denoised=<denoised> "
          "--output-grain-table=<outfile> "
          "See comments in noise_model.c for more information.\n",
          exec_name);
  exit(EXIT_FAILURE);
}

static const arg_def_t help =
    ARG_DEF(NULL, "help", 0, "Show usage options and exit");
static const arg_def_t width_arg =
    ARG_DEF("w", "width", 1, "Input width (if rawvideo)");
static const arg_def_t height_arg =
    ARG_DEF("h", "height", 1, "Input height (if rawvideo)");
static const arg_def_t skip_frames_arg =
    ARG_DEF("s", "skip-frames", 1, "Number of frames to skip (default = 1)");
static const arg_def_t fps_arg = ARG_DEF(NULL, "fps", 1, "Frame rate");
static const arg_def_t input_arg = ARG_DEF("-i", "input", 1, "Input filename");
static const arg_def_t output_grain_table_arg =
    ARG_DEF("n", "output-grain-table", 1, "Output noise file");
static const arg_def_t input_denoised_arg =
    ARG_DEF("d", "input-denoised", 1, "Input denoised filename (YUV) only");
static const arg_def_t block_size_arg =
    ARG_DEF("b", "block_size", 1, "Block size");
static const arg_def_t bit_depth_arg =
    ARG_DEF(NULL, "bit-depth", 1, "Bit depth of input");
static const arg_def_t use_i420 =
    ARG_DEF(NULL, "i420", 0, "Input file (and denoised) is I420 (default)");
static const arg_def_t use_i422 =
    ARG_DEF(NULL, "i422", 0, "Input file (and denoised) is I422");
static const arg_def_t use_i444 =
    ARG_DEF(NULL, "i444", 0, "Input file (and denoised) is I444");

typedef struct {
  int width;
  int height;
  struct aom_rational fps;
  const char *input;
  const char *input_denoised;
  const char *output_grain_table;
  int img_fmt;
  int block_size;
  int bit_depth;
  int run_flat_block_finder;
  int force_flat_psd;
  int skip_frames;
} noise_model_args_t;

void parse_args(noise_model_args_t *noise_args, int *argc, char **argv) {
  struct arg arg;
  static const arg_def_t *main_args[] = { &help,
                                          &input_arg,
                                          &fps_arg,
                                          &width_arg,
                                          &height_arg,
                                          &block_size_arg,
                                          &output_grain_table_arg,
                                          &input_denoised_arg,
                                          &use_i420,
                                          &use_i422,
                                          &use_i444,
                                          NULL };
  for (int argi = *argc + 1; *argv; argi++, argv++) {
    if (arg_match(&arg, &help, argv)) {
      fprintf(stdout, "\nOptions:\n");
      arg_show_usage(stdout, main_args);
      exit(0);
    } else if (arg_match(&arg, &width_arg, argv)) {
      noise_args->width = atoi(arg.val);
    } else if (arg_match(&arg, &height_arg, argv)) {
      noise_args->height = atoi(arg.val);
    } else if (arg_match(&arg, &input_arg, argv)) {
      noise_args->input = arg.val;
    } else if (arg_match(&arg, &input_denoised_arg, argv)) {
      noise_args->input_denoised = arg.val;
    } else if (arg_match(&arg, &output_grain_table_arg, argv)) {
      noise_args->output_grain_table = arg.val;
    } else if (arg_match(&arg, &block_size_arg, argv)) {
      noise_args->block_size = atoi(arg.val);
    } else if (arg_match(&arg, &bit_depth_arg, argv)) {
      noise_args->bit_depth = atoi(arg.val);
    } else if (arg_match(&arg, &fps_arg, argv)) {
      noise_args->fps = arg_parse_rational(&arg);
    } else if (arg_match(&arg, &use_i420, argv)) {
      noise_args->img_fmt = AOM_IMG_FMT_I420;
    } else if (arg_match(&arg, &use_i422, argv)) {
      noise_args->img_fmt = AOM_IMG_FMT_I422;
    } else if (arg_match(&arg, &use_i444, argv)) {
      noise_args->img_fmt = AOM_IMG_FMT_I444;
    } else if (arg_match(&arg, &skip_frames_arg, argv)) {
      noise_args->skip_frames = atoi(arg.val);
    } else {
      fprintf(stdout, "Unknown arg: %s\n\nUsage:\n", *argv);
      arg_show_usage(stdout, main_args);
      exit(0);
    }
  }
  if (noise_args->bit_depth > 8) {
    noise_args->img_fmt |= AOM_IMG_FMT_HIGHBITDEPTH;
  }
}

int main(int argc, char *argv[]) {
  noise_model_args_t args = { 0,  0, { 1, 25 }, 0, 0, 0, AOM_IMG_FMT_I420,
                              32, 8, 0,         0, 1 };
  aom_image_t raw, denoised;
  FILE *infile = NULL;
  AvxVideoInfo info;

  memset(&info, 0, sizeof(info));

  exec_name = argv[0];
  parse_args(&args, &argc, argv + 1);

  info.frame_width = args.width;
  info.frame_height = args.height;
  info.time_base.numerator = args.fps.den;
  info.time_base.denominator = args.fps.num;

  if (info.frame_width <= 0 || info.frame_height <= 0 ||
      (info.frame_width % 2) != 0 || (info.frame_height % 2) != 0) {
    die("Invalid frame size: %dx%d", info.frame_width, info.frame_height);
  }
  if (!aom_img_alloc(&raw, args.img_fmt, info.frame_width, info.frame_height,
                     1)) {
    die("Failed to allocate image.");
  }
  if (!aom_img_alloc(&denoised, args.img_fmt, info.frame_width,
                     info.frame_height, 1)) {
    die("Failed to allocate image.");
  }
  infile = fopen(args.input, "r");
  if (!infile) {
    die("Failed to open input file:", args.input);
  }
  fprintf(stderr, "Bit depth: %d  stride:%d\n", args.bit_depth, raw.stride[0]);

  const int high_bd = args.bit_depth > 8;
  const int block_size = args.block_size;
  aom_flat_block_finder_t block_finder;
  aom_flat_block_finder_init(&block_finder, block_size, args.bit_depth,
                             high_bd);

  const int num_blocks_w = (info.frame_width + block_size - 1) / block_size;
  const int num_blocks_h = (info.frame_height + block_size - 1) / block_size;
  uint8_t *flat_blocks = (uint8_t *)aom_malloc(num_blocks_w * num_blocks_h);
  aom_noise_model_t noise_model;
  aom_noise_model_params_t params = { AOM_NOISE_SHAPE_SQUARE, 3, args.bit_depth,
                                      high_bd };
  aom_noise_model_init(&noise_model, params);

  FILE *denoised_file = 0;
  if (args.input_denoised) {
    denoised_file = fopen(args.input_denoised, "rb");
    if (!denoised_file)
      die("Unable to open input_denoised: %s", args.input_denoised);
  } else {
    die("--input-denoised file must be specified");
  }
  aom_film_grain_table_t grain_table = { 0, 0 };

  int64_t prev_timestamp = 0;
  int frame_count = 0;
  while (aom_img_read(&raw, infile)) {
    if (args.input_denoised) {
      if (!aom_img_read(&denoised, denoised_file)) {
        die("Unable to read input denoised file");
      }
    }
    if (frame_count % args.skip_frames == 0) {
      int num_flat_blocks = num_blocks_w * num_blocks_h;
      memset(flat_blocks, 1, num_flat_blocks);
      if (args.run_flat_block_finder) {
        memset(flat_blocks, 0, num_flat_blocks);
        num_flat_blocks = aom_flat_block_finder_run(
            &block_finder, raw.planes[0], info.frame_width, info.frame_height,
            info.frame_width, flat_blocks);
        fprintf(stdout, "Num flat blocks %d\n", num_flat_blocks);
      }

      const uint8_t *planes[3] = { raw.planes[0], raw.planes[1],
                                   raw.planes[2] };
      uint8_t *denoised_planes[3] = { denoised.planes[0], denoised.planes[1],
                                      denoised.planes[2] };
      int strides[3] = { raw.stride[0] >> high_bd, raw.stride[1] >> high_bd,
                         raw.stride[2] >> high_bd };
      int chroma_sub[3] = { raw.x_chroma_shift, raw.y_chroma_shift, 0 };

      fprintf(stdout, "Updating noise model...\n");
      aom_noise_status_t status = aom_noise_model_update(
          &noise_model, (const uint8_t *const *)planes,
          (const uint8_t *const *)denoised_planes, info.frame_width,
          info.frame_height, strides, chroma_sub, flat_blocks, block_size);

      int64_t cur_timestamp =
          frame_count * 10000000ULL * args.fps.den / args.fps.num;
      if (status == AOM_NOISE_STATUS_DIFFERENT_NOISE_TYPE) {
        fprintf(stdout,
                "Noise type is different, updating parameters for time "
                "[ %" PRId64 ", %" PRId64 ")\n",
                prev_timestamp, cur_timestamp);
        aom_film_grain_t grain;
        aom_noise_model_get_grain_parameters(&noise_model, &grain);
        aom_film_grain_table_append(&grain_table, prev_timestamp, cur_timestamp,
                                    &grain);
        aom_noise_model_save_latest(&noise_model);
        prev_timestamp = cur_timestamp;
      }

      fprintf(stdout, "Done noise model update, status = %d\n", status);
    }
    frame_count++;
  }

  aom_film_grain_t grain;
  aom_noise_model_get_grain_parameters(&noise_model, &grain);
  aom_film_grain_table_append(&grain_table, prev_timestamp, INT64_MAX, &grain);
  if (args.output_grain_table) {
    struct aom_internal_error_info error_info;
    if (AOM_CODEC_OK != aom_film_grain_table_write(&grain_table,
                                                   args.output_grain_table,
                                                   &error_info)) {
      die("Unable to write output film grain table");
    }
  }
  aom_film_grain_table_free(&grain_table);

  if (infile) fclose(infile);
  if (denoised_file) fclose(denoised_file);
  aom_img_free(&raw);
  aom_img_free(&denoised);

  return EXIT_SUCCESS;
}
