// Copyright 2019 The libgav1 Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "absl/strings/numbers.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "examples/file_reader_factory.h"
#include "examples/file_reader_interface.h"
#include "examples/file_writer.h"
#include "gav1/decoder.h"

namespace {

struct Options {
  const char* input_file_name = nullptr;
  const char* output_file_name = nullptr;
  libgav1::FileWriter::FileType output_file_type =
      libgav1::FileWriter::kFileTypeRaw;
  uint8_t post_filter_mask = 0x1f;
  int threads = 1;
  int limit = 0;
  int skip = 0;
  int verbose = 0;
};

struct Timing {
  absl::Duration input;
  absl::Duration dequeue;
};

void PrintHelp(FILE* const fout) {
  fprintf(fout,
          "Usage: gav1_decode [options] <input file>"
          " [-o <output file>]\n");
  fprintf(fout, "\n");
  fprintf(fout, "Options:\n");
  fprintf(fout, "  -h, --help This help message.\n");
  fprintf(fout, "  --threads <positive integer> (Default 1).\n");
  fprintf(fout,
          "  --limit <integer> Stop decoding after N frames (0 = all).\n");
  fprintf(fout, "  --skip <integer> Skip initial N frames (Default 0).\n");
  fprintf(fout, "  --version.\n");
  fprintf(fout, "  --y4m (Default false).\n");
  fprintf(fout, "  --raw (Default true).\n");
  fprintf(fout, "  -v logging verbosity, can be used multiple times.\n");
  fprintf(fout, "  --post_filter_mask <integer> (Default 0x1f).\n");
  fprintf(fout,
          "   Mask indicating which post filters should be applied to the"
          " reconstructed\n   frame. From LSB:\n");
  fprintf(fout, "     Bit 0: Loop filter (deblocking filter)\n");
  fprintf(fout, "     Bit 1: Cdef\n");
  fprintf(fout, "     Bit 2: SuperRes\n");
  fprintf(fout, "     Bit 3: Loop Restoration\n");
  fprintf(fout, "     Bit 4: Film Grain Synthesis\n");
}

void ParseOptions(int argc, char* argv[], Options* const options) {
  for (int i = 1; i < argc; ++i) {
    int32_t value;
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      PrintHelp(stdout);
      exit(EXIT_SUCCESS);
    } else if (strcmp(argv[i], "-o") == 0) {
      if (++i >= argc) {
        fprintf(stderr, "Missing argument for '-o'\n");
        PrintHelp(stderr);
        exit(EXIT_FAILURE);
      }
      options->output_file_name = argv[i];
    } else if (strcmp(argv[i], "--version") == 0) {
      printf("gav1_decode, a libgav1 based AV1 decoder\n");
      printf("libgav1 %s\n", libgav1::GetVersionString());
      printf("max bitdepth: %d\n", libgav1::Decoder::GetMaxBitdepth());
      printf("build configuration: %s\n", libgav1::GetBuildConfiguration());
      exit(EXIT_SUCCESS);
    } else if (strcmp(argv[i], "-v") == 0) {
      ++options->verbose;
    } else if (strcmp(argv[i], "--raw") == 0) {
      options->output_file_type = libgav1::FileWriter::kFileTypeRaw;
    } else if (strcmp(argv[i], "--y4m") == 0) {
      options->output_file_type = libgav1::FileWriter::kFileTypeY4m;
    } else if (strcmp(argv[i], "--threads") == 0) {
      if (++i >= argc || !absl::SimpleAtoi(argv[i], &value)) {
        fprintf(stderr, "Missing/Invalid value for --threads.\n");
        PrintHelp(stderr);
        exit(EXIT_FAILURE);
      }
      options->threads = value;
    } else if (strcmp(argv[i], "--limit") == 0) {
      if (++i >= argc || !absl::SimpleAtoi(argv[i], &value) || value < 0) {
        fprintf(stderr, "Missing/Invalid value for --limit.\n");
        PrintHelp(stderr);
        exit(EXIT_FAILURE);
      }
      options->limit = value;
    } else if (strcmp(argv[i], "--skip") == 0) {
      if (++i >= argc || !absl::SimpleAtoi(argv[i], &value) || value < 0) {
        fprintf(stderr, "Missing/Invalid value for --skip.\n");
        PrintHelp(stderr);
        exit(EXIT_FAILURE);
      }
      options->skip = value;
    } else if (strcmp(argv[i], "--post_filter_mask") == 0) {
      // Only the last 5 bits of the mask can be set.
      if (++i >= argc || !absl::SimpleAtoi(argv[i], &value) ||
          (value & ~31) != 0) {
        fprintf(stderr, "Invalid value for --post_filter_mask.\n");
        PrintHelp(stderr);
        exit(EXIT_FAILURE);
      }
      options->post_filter_mask = value;
    } else if (strlen(argv[i]) > 1 && argv[i][0] == '-') {
      fprintf(stderr, "Unknown option '%s'!\n", argv[i]);
      exit(EXIT_FAILURE);
    } else {
      if (options->input_file_name == nullptr) {
        options->input_file_name = argv[i];
      } else {
        fprintf(stderr, "Found invalid parameter: \"%s\".\n", argv[i]);
        PrintHelp(stderr);
        exit(EXIT_FAILURE);
      }
    }
  }

  if (argc < 2 || options->input_file_name == nullptr) {
    fprintf(stderr, "Input file is required!\n");
    PrintHelp(stderr);
    exit(EXIT_FAILURE);
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  Options options;
  ParseOptions(argc, argv, &options);

  auto file_reader =
      libgav1::FileReaderFactory::OpenReader(options.input_file_name);
  if (file_reader == nullptr) {
    fprintf(stderr, "Cannot open input file!\n");
    return EXIT_FAILURE;
  }

  libgav1::Decoder decoder;
  libgav1::DecoderSettings settings = {};
  settings.post_filter_mask = options.post_filter_mask;
  settings.threads = options.threads;
  libgav1::StatusCode status = decoder.Init(&settings);
  if (status != kLibgav1StatusOk) {
    fprintf(stderr, "Error initializing decoder: %s\n",
            libgav1::GetErrorString(status));
    return EXIT_FAILURE;
  }

  fprintf(stderr, "decoding '%s'\n", options.input_file_name);
  if (options.verbose > 0 && options.skip > 0) {
    fprintf(stderr, "skipping %d frame(s).\n", options.skip);
  }

  int input_frames = 0;
  int decoded_frames = 0;
  Timing timing = {};
  std::vector<uint8_t> temporal_unit;
  std::unique_ptr<libgav1::FileWriter> file_writer;
  do {
    const absl::Time read_start = absl::Now();
    if (!file_reader->ReadTemporalUnit(&temporal_unit, /*timestamp=*/nullptr)) {
      fprintf(stderr, "Error reading input file.\n");
      return EXIT_FAILURE;
    }
    timing.input += absl::Now() - read_start;

    if (++input_frames <= options.skip) continue;

    if (options.verbose > 1) {
      fprintf(stderr, "enqueue frame (length %zu)\n", temporal_unit.size());
    }

    // If temporal_unit is empty, temporal_unit.data() may or may not return a
    // null pointer. Use nullptr for consistent behavior (to make EnqueueFrame
    // flush the decoder).
    const uint8_t* const temporal_unit_data =
        temporal_unit.empty() ? nullptr : temporal_unit.data();
    status = decoder.EnqueueFrame(temporal_unit_data, temporal_unit.size(),
                                  /*user_private_data=*/0);
    if (status != kLibgav1StatusOk) {
      fprintf(stderr, "Unable to enqueue frame: %s\n",
              libgav1::GetErrorString(status));
      return EXIT_FAILURE;
    }

    const absl::Time dequeue_start = absl::Now();
    const libgav1::DecoderBuffer* buffer;
    status = decoder.DequeueFrame(&buffer);
    timing.dequeue += absl::Now() - dequeue_start;
    if (status != kLibgav1StatusOk) {
      fprintf(stderr, "Unable to dequeue frame: %s\n",
              libgav1::GetErrorString(status));
      return EXIT_FAILURE;
    }
    if (buffer != nullptr) {
      ++decoded_frames;
      if (options.verbose > 1) {
        fprintf(stderr, "buffer dequeued\n");
      }

      if (options.output_file_name != nullptr && file_writer == nullptr) {
        libgav1::FileWriter::Y4mParameters y4m_parameters;
        y4m_parameters.width = buffer->displayed_width[0];
        y4m_parameters.height = buffer->displayed_height[0];
        y4m_parameters.frame_rate_numerator = file_reader->frame_rate();
        y4m_parameters.frame_rate_denominator = file_reader->time_scale();
        y4m_parameters.chroma_sample_position = buffer->chroma_sample_position;
        y4m_parameters.image_format = buffer->image_format;
        y4m_parameters.bitdepth = static_cast<size_t>(buffer->bitdepth);
        file_writer = libgav1::FileWriter::Open(options.output_file_name,
                                                options.output_file_type,
                                                &y4m_parameters);
        if (file_writer == nullptr) {
          fprintf(stderr, "Cannot open output file!\n");
          return EXIT_FAILURE;
        }
      }

      if (file_writer != nullptr && !file_writer->WriteFrame(*buffer)) {
        fprintf(stderr, "Error writing output file.\n");
        return EXIT_FAILURE;
      }
      if (options.limit > 0 && options.limit == decoded_frames) break;
    }
  } while (!file_reader->IsEndOfFile());

  if (options.verbose > 0) {
    fprintf(stderr, "time to read input: %d us\n",
            static_cast<int>(absl::ToInt64Microseconds(timing.input)));
    const int decode_time_us =
        static_cast<int>(absl::ToInt64Microseconds(timing.dequeue));
    const double decode_fps =
        (decoded_frames == 0) ? 0.0 : 1.0e6 * decoded_frames / decode_time_us;
    fprintf(stderr, "time to decode input: %d us (%d frames, %.2f fps)\n",
            decode_time_us, decoded_frames, decode_fps);
  }

  return EXIT_SUCCESS;
}
