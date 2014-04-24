// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc_browsertest_perf.h"

#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/perf/perf_test.h"

static std::string Statistic(const std::string& statistic,
                             const std::string& bucket) {
  // A ssrc stats key will be on the form stats.<bucket>-<key>.values.
  // This will give a json "path" which will dig into the time series for the
  // specified statistic. Buckets can be for instance ssrc_1212344, bweforvideo,
  // and will each contain a bunch of statistics relevant to their nature.
  // Each peer connection has a number of such buckets.
  return base::StringPrintf("stats.%s-%s.values", bucket.c_str(),
                            statistic.c_str());
}

static bool MaybePrintResultsForAudioReceive(
    const std::string& ssrc, const base::DictionaryValue& pc_dict) {
  std::string value;
  if (!pc_dict.GetString(Statistic("audioOutputLevel", ssrc), &value)) {
    // Not an audio receive stream.
    return false;
  }

  EXPECT_TRUE(pc_dict.GetString(Statistic("bytesReceived", ssrc), &value));
  perf_test::PrintResult(
      "audio_bytes", "", "bytes_recv", value, "bytes", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("packetsLost", ssrc), &value));
  perf_test::PrintResult(
      "audio_misc", "", "packets_lost", value, "", false);

  return true;
}

static bool MaybePrintResultsForAudioSend(
    const std::string& ssrc, const base::DictionaryValue& pc_dict) {
  std::string value;
  if (!pc_dict.GetString(Statistic("audioInputLevel", ssrc), &value)) {
    // Not an audio send stream.
    return false;
  }

  EXPECT_TRUE(pc_dict.GetString(Statistic("bytesSent", ssrc), &value));
  perf_test::PrintResult(
      "audio_bytes", "", "bytes_sent", value, "bytes", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googJitterReceived", ssrc), &value));
  perf_test::PrintResult(
      "audio_tx", "", "goog_jitter_recv", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googRtt", ssrc), &value));
  perf_test::PrintResult(
      "audio_tx", "", "goog_rtt", value, "ms", false);
  return true;
}

static bool MaybePrintResultsForVideoSend(
    const std::string& ssrc, const base::DictionaryValue& pc_dict) {
  std::string value;
  if (!pc_dict.GetString(Statistic("googFrameRateSent", ssrc), &value)) {
    // Not a video send stream.
    return false;
  }

  // Graph these by unit: the dashboard expects all stats in one graph to have
  // the same unit (e.g. ms, fps, etc). Most graphs, like video_fps, will also
  // be populated by the counterparts on the video receiving side.
  perf_test::PrintResult(
      "video_fps", "", "goog_frame_rate_sent", value, "fps", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googFrameRateInput", ssrc), &value));
  perf_test::PrintResult(
      "video_fps", "", "goog_frame_rate_input", value, "fps", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("bytesSent", ssrc), &value));
  perf_test::PrintResult(
      "video_total_bytes", "", "bytes_sent", value, "bytes", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googFirsReceived", ssrc), &value));
  perf_test::PrintResult(
      "video_misc", "", "goog_firs_recv", value, "", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googNacksReceived", ssrc), &value));
  perf_test::PrintResult(
      "video_misc", "", "goog_nacks_recv", value, "", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googFrameWidthSent", ssrc), &value));
  perf_test::PrintResult(
      "video_resolution", "", "goog_frame_width_sent", value, "pixels", false);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameHeightSent", ssrc), &value));
  perf_test::PrintResult(
      "video_resolution", "", "goog_frame_height_sent", value, "pixels", false);

  EXPECT_TRUE(pc_dict.GetString(
      Statistic("googCaptureJitterMs", ssrc), &value));
  perf_test::PrintResult(
      "video_tx", "", "goog_capture_jitter_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(
      Statistic("googCaptureQueueDelayMsPerS", ssrc), &value));
  perf_test::PrintResult(
      "video_tx", "", "goog_capture_queue_delay_ms_per_s",
       value, "ms/s", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googAvgEncodeMs", ssrc), &value));
  perf_test::PrintResult(
      "video_tx", "", "goog_avg_encode_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googRtt", ssrc), &value));
  perf_test::PrintResult("video_tx", "", "goog_rtt", value, "ms", false);

  EXPECT_TRUE(pc_dict.GetString(
      Statistic("googEncodeUsagePercent", ssrc), &value));
  perf_test::PrintResult(
      "video_cpu_usage", "", "goog_encode_usage_percent", value, "%", false);
  return true;
}

static bool MaybePrintResultsForVideoReceive(
    const std::string& ssrc, const base::DictionaryValue& pc_dict) {
  std::string value;
  if (!pc_dict.GetString(Statistic("googFrameRateReceived", ssrc), &value)) {
    // Not a video receive stream.
    return false;
  }

  perf_test::PrintResult(
      "video_fps", "", "goog_frame_rate_recv", value, "fps", false);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameRateOutput", ssrc), &value));
  perf_test::PrintResult(
      "video_fps", "", "goog_frame_rate_output", value, "fps", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("packetsLost", ssrc), &value));
  perf_test::PrintResult("video_misc", "", "packets_lost", value, "", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("bytesReceived", ssrc), &value));
  perf_test::PrintResult(
      "video_total_bytes", "", "bytes_recv", value, "bytes", false);

  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameWidthReceived", ssrc), &value));
  perf_test::PrintResult(
      "video_resolution", "", "goog_frame_width_recv", value, "pixels", false);
  EXPECT_TRUE(
      pc_dict.GetString(Statistic("googFrameHeightReceived", ssrc), &value));
  perf_test::PrintResult(
      "video_resolution", "", "goog_frame_height_recv", value, "pixels", false);

  EXPECT_TRUE(pc_dict.GetString(Statistic("googCurrentDelayMs", ssrc), &value));
  perf_test::PrintResult(
      "video_rx", "", "goog_current_delay_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googTargetDelayMs", ssrc), &value));
  perf_test::PrintResult(
      "video_rx", "", "goog_target_delay_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googDecodeMs", ssrc), &value));
  perf_test::PrintResult("video_rx", "", "goog_decode_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googMaxDecodeMs", ssrc), &value));
  perf_test::PrintResult(
      "video_rx", "", "goog_max_decode_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googJitterBufferMs", ssrc), &value));
  perf_test::PrintResult(
      "video_rx", "", "goog_jitter_buffer_ms", value, "ms", false);
  EXPECT_TRUE(pc_dict.GetString(Statistic("googRenderDelayMs", ssrc), &value));
  perf_test::PrintResult(
      "video_rx", "", "goog_render_delay_ms", value, "ms", false);

  return true;
}

static std::string ExtractSsrcIdentifier(const std::string& key) {
  // Example key: ssrc_1234-someStatName. Grab the part before the dash.
  size_t key_start_pos = 0;
  size_t key_end_pos = key.find("-");
  CHECK(key_end_pos != std::string::npos) << "Could not parse key " << key;
  return key.substr(key_start_pos, key_end_pos - key_start_pos);
}

// Returns the set of unique ssrc identifiers in the call (e.g. ssrc_1234,
// ssrc_12356, etc). |stats_dict| is the .stats dict from one peer connection.
static std::set<std::string> FindAllSsrcIdentifiers(
    const base::DictionaryValue& stats_dict) {
  std::set<std::string> result;
  base::DictionaryValue::Iterator stats_iterator(stats_dict);

  while (!stats_iterator.IsAtEnd()) {
    if (stats_iterator.key().find("ssrc_") != std::string::npos)
      result.insert(ExtractSsrcIdentifier(stats_iterator.key()));
    stats_iterator.Advance();
  }
  return result;
}

namespace test {

void PrintBweForVideoMetrics(const base::DictionaryValue& pc_dict) {
  const std::string kBweStatsKey = "bweforvideo";
  std::string value;
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googAvailableSendBandwidth", kBweStatsKey), &value));
  perf_test::PrintResult(
      "bwe_stats", "", "available_send_bw", value, "bit/s", false);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googAvailableReceiveBandwidth", kBweStatsKey), &value));
  perf_test::PrintResult(
      "bwe_stats", "", "available_recv_bw", value, "bit/s", false);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googTargetEncBitrate", kBweStatsKey), &value));
  perf_test::PrintResult(
      "bwe_stats", "", "target_enc_bitrate", value, "bit/s", false);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googActualEncBitrate", kBweStatsKey), &value));
  perf_test::PrintResult(
      "bwe_stats", "", "actual_enc_bitrate", value, "bit/s", false);
  ASSERT_TRUE(pc_dict.GetString(
      Statistic("googTransmitBitrate", kBweStatsKey), &value));
  perf_test::PrintResult(
      "bwe_stats", "", "transmit_bitrate", value, "bit/s",false);
}

void PrintMetricsForAllStreams(const base::DictionaryValue& pc_dict) {
  const base::DictionaryValue* stats_dict;
  ASSERT_TRUE(pc_dict.GetDictionary("stats", &stats_dict));
  std::set<std::string> ssrc_identifiers = FindAllSsrcIdentifiers(*stats_dict);

  std::set<std::string>::const_iterator ssrc_iterator =
      ssrc_identifiers.begin();
  for (; ssrc_iterator != ssrc_identifiers.end(); ++ssrc_iterator) {
    // Figure out which stream type this ssrc represents and print all the
    // interesting metrics for it.
    const std::string& ssrc = *ssrc_iterator;
    bool did_recognize_stream_type =
        MaybePrintResultsForAudioReceive(ssrc, pc_dict) ||
        MaybePrintResultsForAudioSend(ssrc, pc_dict) ||
        MaybePrintResultsForVideoReceive(ssrc, pc_dict) ||
        MaybePrintResultsForVideoSend(ssrc, pc_dict);
    ASSERT_TRUE(did_recognize_stream_type) << "Failed to figure out which "
                                              "kind of stream SSRC " << ssrc
                                           << " is. ";
  }
}

}  // namespace test
