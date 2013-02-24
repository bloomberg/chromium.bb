// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Standalone benchmarking application based on FFmpeg.  This tool is used to
// measure decoding performance between different FFmpeg compile and run-time
// options.  We also use this tool to measure performance regressions when
// testing newer builds of FFmpeg from trunk.

#include <iomanip>
#include <iostream>
#include <string>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "media/base/djb2.h"
#include "media/base/media.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "media/filters/in_memory_url_protocol.h"

// For pipe _setmode to binary
#if defined(OS_WIN)
#include <fcntl.h>
#include <io.h>
#endif

namespace switches {
const char kStream[]       = "stream";
const char kVideoThreads[] = "video-threads";
const char kFast2[]        = "fast2";
const char kErrorCorrection[] = "error-correction";
const char kSkip[]         = "skip";
const char kFlush[]        = "flush";
const char kDjb2[]         = "djb2";
const char kMd5[]          = "md5";
const char kFrames[]       = "frames";
const char kLoop[]         = "loop";

}  // namespace switches

#if defined(OS_WIN)

// Enable to build with exception handler
// #define ENABLE_WINDOWS_EXCEPTIONS 1

#ifdef ENABLE_WINDOWS_EXCEPTIONS
// warning: disable warning about exception handler.
#pragma warning(disable:4509)
#endif

// Thread priorities to make benchmark more stable.

void EnterTimingSection() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
}

void LeaveTimingSection() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
}
#else
void EnterTimingSection() {
  pthread_attr_t pta;
  struct sched_param param;

  pthread_attr_init(&pta);
  memset(&param, 0, sizeof(param));
  param.sched_priority = 78;
  pthread_attr_setschedparam(&pta, &param);
  pthread_attr_destroy(&pta);
}

void LeaveTimingSection() {
}
#endif

int main(int argc, const char** argv) {
  base::AtExitManager exit_manager;

  CommandLine::Init(argc, argv);

  logging::InitLogging(
      NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,  // Ignored.
      logging::DELETE_OLD_LOG_FILE,  // Ignored.
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  const CommandLine::StringVector& filenames = cmd_line->GetArgs();
  if (filenames.empty()) {
    std::cerr << "Usage: " << argv[0] << " [OPTIONS] FILE [DUMPFILE]\n"
              << "  --stream=[audio|video]          "
              << "Benchmark either the audio or video stream\n"
              << "  --video-threads=N               "
              << "Decode video using N threads\n"
              << "  --frames=N                      "
              << "Decode N frames\n"
              << "  --loop=N                        "
              << "Loop N times\n"
              << "  --fast2                         "
              << "Enable fast2 flag\n"
              << "  --error-correction              "
              << "Enable ffmpeg error correction\n"
              << "  --flush                         "
              << "Flush last frame\n"
              << "  --djb2 (aka --hash)             "
              << "Hash decoded buffers (DJB2)\n"
              << "  --md5                           "
              << "Hash decoded buffers (MD5)\n"
              << "  --skip=[1|2|3]                  "
              << "1=loop nonref, 2=loop, 3= frame nonref\n" << std::endl;
    return 1;
  }

  // Initialize our media library (try loading DLLs, etc.) before continuing.
  base::FilePath media_path;
  PathService::Get(base::DIR_MODULE, &media_path);
  if (!media::InitializeMediaLibrary(media_path)) {
    std::cerr << "Unable to initialize the media library." << std::endl;
    return 1;
  }

  // Retrieve command line options.
  base::FilePath in_path(filenames[0]);
  base::FilePath out_path;
  if (filenames.size() > 1)
    out_path = base::FilePath(filenames[1]);
  AVMediaType target_codec = AVMEDIA_TYPE_UNKNOWN;

  // Determine whether to benchmark audio or video decoding.
  std::string stream(cmd_line->GetSwitchValueASCII(switches::kStream));
  if (!stream.empty()) {
    if (stream.compare("audio") == 0) {
      target_codec = AVMEDIA_TYPE_AUDIO;
    } else if (stream.compare("video") == 0) {
      target_codec = AVMEDIA_TYPE_VIDEO;
    } else {
      std::cerr << "Unknown --stream option " << stream << std::endl;
      return 1;
    }
  }

  // Determine number of threads to use for video decoding (optional).
  int video_threads = 0;
  std::string threads(cmd_line->GetSwitchValueASCII(switches::kVideoThreads));
  if (!threads.empty() &&
      !base::StringToInt(threads, &video_threads)) {
    video_threads = 0;
  }

  // Determine number of frames to decode (optional).
  int max_frames = 0;
  std::string frames_opt(cmd_line->GetSwitchValueASCII(switches::kFrames));
  if (!frames_opt.empty() &&
      !base::StringToInt(frames_opt, &max_frames)) {
    max_frames = 0;
  }

  // Determine number of times to loop (optional).
  int max_loops = 0;
  std::string loop_opt(cmd_line->GetSwitchValueASCII(switches::kLoop));
  if (!loop_opt.empty() &&
      !base::StringToInt(loop_opt, &max_loops)) {
    max_loops = 0;
  }

  bool fast2 = false;
  if (cmd_line->HasSwitch(switches::kFast2)) {
    fast2 = true;
  }

  bool error_correction = false;
  if (cmd_line->HasSwitch(switches::kErrorCorrection)) {
    error_correction = true;
  }

  bool flush = false;
  if (cmd_line->HasSwitch(switches::kFlush)) {
    flush = true;
  }

  unsigned int hash_value = 5381u;  // Seed for DJB2.
  bool hash_djb2 = false;
  if (cmd_line->HasSwitch(switches::kDjb2)) {
    hash_djb2 = true;
  }

  base::MD5Context ctx;  // Intermediate MD5 data: do not use
  base::MD5Init(&ctx);
  bool hash_md5 = false;
  if (cmd_line->HasSwitch(switches::kMd5))
    hash_md5 = true;

  int skip = 0;
  if (cmd_line->HasSwitch(switches::kSkip)) {
    std::string skip_opt(cmd_line->GetSwitchValueASCII(switches::kSkip));
    if (!base::StringToInt(skip_opt, &skip)) {
      skip = 0;
    }
  }

  std::ostream* log_out = &std::cout;
#if defined(ENABLE_WINDOWS_EXCEPTIONS)
  // Catch exceptions so this tool can be used in automated testing.
  __try {
#endif

  base::MemoryMappedFile file_data;
  file_data.Initialize(in_path);
  media::InMemoryUrlProtocol protocol(
      file_data.data(), file_data.length(), false);

  // Register FFmpeg and attempt to open file.
  media::FFmpegGlue glue(&protocol);
  if (!glue.OpenContext()) {
    std::cerr << "Error: Could not open input for "
              << in_path.value() << std::endl;
    return 1;
  }

  AVFormatContext* format_context = glue.format_context();

  // Open output file.
  FILE *output = NULL;
  if (!out_path.empty()) {
    // TODO(fbarchard): Add pipe:1 for piping to stderr.
    if (out_path.value().substr(0, 5) == FILE_PATH_LITERAL("pipe:") ||
        out_path.value() == FILE_PATH_LITERAL("-")) {
      output = stdout;
      log_out = &std::cerr;
#if defined(OS_WIN)
      _setmode(_fileno(stdout), _O_BINARY);
#endif
    } else {
      output = file_util::OpenFile(out_path, "wb");
    }
    if (!output) {
      std::cerr << "Error: Could not open output "
                << out_path.value() << std::endl;
      return 1;
    }
  }

  // Parse a little bit of the stream to fill out the format context.
  if (avformat_find_stream_info(format_context, NULL) < 0) {
    std::cerr << "Error: Could not find stream info for "
              << in_path.value() << std::endl;
    return 1;
  }

  // Find our target stream.
  int target_stream = -1;
  for (size_t i = 0; i < format_context->nb_streams; ++i) {
    AVCodecContext* codec_context = format_context->streams[i]->codec;
    AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

    // See if we found our target codec.
    if (codec_context->codec_type == target_codec && target_stream < 0) {
      *log_out << "* ";
      target_stream = i;
    } else {
      *log_out << "  ";
    }

    if (!codec || (codec_context->codec_type == AVMEDIA_TYPE_UNKNOWN)) {
      *log_out << "Stream #" << i << ": Unknown" << std::endl;
    } else {
      // Print out stream information
      *log_out << "Stream #" << i << ": " << codec->name << " ("
               << codec->long_name << ")" << std::endl;
    }
  }

  // Only continue if we found our target stream.
  if (target_stream < 0) {
    std::cerr << "Error: Could not find target stream "
              << target_stream << " for " << in_path.value() << std::endl;
    return 1;
  }

  // Prepare FFmpeg structures.
  AVPacket packet;
  AVCodecContext* codec_context = format_context->streams[target_stream]->codec;
  AVCodec* codec = avcodec_find_decoder(codec_context->codec_id);

  // Only continue if we found our codec.
  if (!codec) {
    std::cerr << "Error: Could not find codec for "
              << in_path.value() << std::endl;
    return 1;
  }

  if (skip == 1) {
    codec_context->skip_loop_filter = AVDISCARD_NONREF;
  } else if (skip == 2) {
    codec_context->skip_loop_filter = AVDISCARD_ALL;
  } else if (skip == 3) {
    codec_context->skip_loop_filter = AVDISCARD_ALL;
    codec_context->skip_frame = AVDISCARD_NONREF;
  }
  if (fast2) {
    // Note this flag is no longer necessary for H264 multithreading.
    codec_context->flags2 |= CODEC_FLAG2_FAST;
  }
  if (error_correction) {
    codec_context->error_concealment = FF_EC_GUESS_MVS | FF_EC_DEBLOCK;
  }

  // Initialize threaded decode.
  if (target_codec == AVMEDIA_TYPE_VIDEO && video_threads > 0) {
    codec_context->thread_count = video_threads;
  }

  // Initialize our codec.
  if (avcodec_open2(codec_context, codec, NULL) < 0) {
    std::cerr << "Error: Could not open codec "
              << (codec_context->codec ? codec_context->codec->name : "(NULL)")
              << " for " << in_path.value() << std::endl;
    return 1;
  }

  // Buffer used for audio decoding.
  scoped_ptr_malloc<AVFrame, media::ScopedPtrAVFree> audio_frame(
      avcodec_alloc_frame());
  if (!audio_frame.get()) {
    std::cerr << "Error: avcodec_alloc_frame for "
              << in_path.value() << std::endl;
    return 1;
  }

  // Buffer used for video decoding.
  scoped_ptr_malloc<AVFrame, media::ScopedPtrAVFree> video_frame(
      avcodec_alloc_frame());
  if (!video_frame.get()) {
    std::cerr << "Error: avcodec_alloc_frame for "
              << in_path.value() << std::endl;
    return 1;
  }

  // Remember size of video.
  int video_width = codec_context->width;
  int video_height = codec_context->height;

  // Stats collector.
  EnterTimingSection();
  std::vector<double> decode_times;
  decode_times.reserve(4096);
  // Parse through the entire stream until we hit EOF.
  base::TimeTicks start = base::TimeTicks::HighResNow();
  int frames = 0;
  int read_result = 0;
  do {
    read_result = av_read_frame(format_context, &packet);

    if (read_result < 0) {
      if (max_loops) {
        --max_loops;
      }
      if (max_loops > 0) {
        av_seek_frame(format_context, -1, 0, AVSEEK_FLAG_BACKWARD);
        read_result = 0;
        continue;
      }
      if (flush) {
        packet.stream_index = target_stream;
        packet.size = 0;
      } else {
        break;
      }
    }

    // Only decode packets from our target stream.
    if (packet.stream_index == target_stream) {
      int result = -1;
      if (target_codec == AVMEDIA_TYPE_AUDIO) {
        int size_out = 0;
        int got_audio = 0;

        avcodec_get_frame_defaults(audio_frame.get());

        base::TimeTicks decode_start = base::TimeTicks::HighResNow();
        result = avcodec_decode_audio4(codec_context, audio_frame.get(),
                                       &got_audio, &packet);
        base::TimeDelta delta = base::TimeTicks::HighResNow() - decode_start;

        if (got_audio) {
          size_out = av_samples_get_buffer_size(
              NULL, codec_context->channels, audio_frame->nb_samples,
              codec_context->sample_fmt, 1);
        }

        if (got_audio && size_out) {
          decode_times.push_back(delta.InMillisecondsF());
          ++frames;
          read_result = 0;  // Force continuation.

          if (output) {
            if (fwrite(audio_frame->data[0], 1, size_out, output) !=
                static_cast<size_t>(size_out)) {
              std::cerr << "Error: Could not write "
                        << size_out << " bytes for " << in_path.value()
                        << std::endl;
              return 1;
            }
          }

          const uint8* u8_samples =
              reinterpret_cast<const uint8*>(audio_frame->data[0]);
          if (hash_djb2) {
            hash_value = DJB2Hash(u8_samples, size_out, hash_value);
          }
          if (hash_md5) {
            base::MD5Update(
                &ctx,
                base::StringPiece(reinterpret_cast<const char*>(u8_samples),
                                                                size_out));
          }
        }
      } else if (target_codec == AVMEDIA_TYPE_VIDEO) {
        int got_picture = 0;

        avcodec_get_frame_defaults(video_frame.get());

        base::TimeTicks decode_start = base::TimeTicks::HighResNow();
        result = avcodec_decode_video2(codec_context, video_frame.get(),
                                       &got_picture, &packet);
        base::TimeDelta delta = base::TimeTicks::HighResNow() - decode_start;

        if (got_picture) {
          decode_times.push_back(delta.InMillisecondsF());
          ++frames;
          read_result = 0;  // Force continuation.

          for (int plane = 0; plane < 3; ++plane) {
            const uint8* source = video_frame->data[plane];
            const size_t source_stride = video_frame->linesize[plane];
            size_t bytes_per_line = codec_context->width;
            size_t copy_lines = codec_context->height;
            if (plane != 0) {
              switch (codec_context->pix_fmt) {
                case PIX_FMT_YUV420P:
                case PIX_FMT_YUVJ420P:
                  bytes_per_line /= 2;
                  copy_lines = (copy_lines + 1) / 2;
                  break;
                case PIX_FMT_YUV422P:
                case PIX_FMT_YUVJ422P:
                  bytes_per_line /= 2;
                  break;
                case PIX_FMT_YUV444P:
                case PIX_FMT_YUVJ444P:
                  break;
                default:
                  std::cerr << "Error: Unknown video format "
                            << codec_context->pix_fmt;
                  return 1;
              }
            }
            if (output) {
              for (size_t i = 0; i < copy_lines; ++i) {
                if (fwrite(source, 1, bytes_per_line, output) !=
                           bytes_per_line) {
                  std::cerr << "Error: Could not write data after "
                            << copy_lines << " lines for "
                            << in_path.value() << std::endl;
                  return 1;
                }
                source += source_stride;
              }
            }
            if (hash_djb2) {
              for (size_t i = 0; i < copy_lines; ++i) {
                hash_value = DJB2Hash(source, bytes_per_line, hash_value);
                source += source_stride;
              }
            }
            if (hash_md5) {
              for (size_t i = 0; i < copy_lines; ++i) {
                base::MD5Update(
                    &ctx,
                    base::StringPiece(reinterpret_cast<const char*>(source),
                                      bytes_per_line));
                source += source_stride;
              }
            }
          }
        }
      } else {
        NOTREACHED();
      }

      // Make sure our decoding went OK.
      if (result < 0) {
        std::cerr << "Error: avcodec_decode returned "
                  << result << " for " << in_path.value() << std::endl;
        return 1;
      }
    }
    // Free our packet.
    av_free_packet(&packet);

    if (max_frames && (frames >= max_frames))
      break;
  } while (read_result >= 0);
  base::TimeDelta total = base::TimeTicks::HighResNow() - start;
  LeaveTimingSection();

  // Clean up.
  if (output)
    file_util::CloseFile(output);

  // Calculate the sum of times.  Note that some of these may be zero.
  double sum = 0;
  for (size_t i = 0; i < decode_times.size(); ++i) {
    sum += decode_times[i];
  }

  double average = 0;
  double stddev = 0;
  double fps = 0;
  if (frames > 0) {
    // Calculate the average time per frame.
    average = sum / frames;

    // Calculate the sum of the squared differences.
    // Standard deviation will only be accurate if no threads are used.
    // TODO(fbarchard): Rethink standard deviation calculation.
    double squared_sum = 0;
    for (int i = 0; i < frames; ++i) {
      double difference = decode_times[i] - average;
      squared_sum += difference * difference;
    }

    // Calculate the standard deviation (jitter).
    stddev = sqrt(squared_sum / frames);

    // Calculate frames per second.
    fps = frames * 1000.0 / sum;
  }

  // Print our results.
  log_out->setf(std::ios::fixed);
  log_out->precision(2);
  *log_out << std::endl;
  *log_out << "     Frames:" << std::setw(11) << frames << std::endl;
  *log_out << "      Width:" << std::setw(11) << video_width << std::endl;
  *log_out << "     Height:" << std::setw(11) << video_height << std::endl;
  *log_out << "      Total:" << std::setw(11) << total.InMillisecondsF()
           << " ms" << std::endl;
  *log_out << "  Summation:" << std::setw(11) << sum
           << " ms" << std::endl;
  *log_out << "    Average:" << std::setw(11) << average
           << " ms" << std::endl;
  *log_out << "     StdDev:" << std::setw(11) << stddev
           << " ms" << std::endl;
  *log_out << "        FPS:" << std::setw(11) << fps
           << std::endl;
  if (hash_djb2) {
    *log_out << "  DJB2 Hash:" << std::setw(11) << hash_value
             << "  " << in_path.value() << std::endl;
  }
  if (hash_md5) {
    base::MD5Digest digest;  // The result of the computation.
    base::MD5Final(&digest, &ctx);
    *log_out << "   MD5 Hash: " << base::MD5DigestToBase16(digest)
             << " " << in_path.value() << std::endl;
  }
#if defined(ENABLE_WINDOWS_EXCEPTIONS)
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    *log_out << "  Exception:" << std::setw(11) << GetExceptionCode()
             << " " << in_path.value() << std::endl;
    return 1;
  }
#endif
  CommandLine::Reset();
  return 0;
}
