// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Simulate end to end streaming.
//
// Input:
// --source=
//   WebM used as the source of video and audio frames.
// --output=
//   File path to writing out the raw event log of the simulation session.
// --sim-id=
//   Unique simulation ID.
//
// Output:
// - Raw event log of the simulation session tagged with the unique test ID,
//   written out to the specified file path.

#include "base/at_exit.h"
#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/memory_mapped_file.h"
#include "base/files/scoped_file.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "media/base/audio_bus.h"
#include "media/base/media.h"
#include "media/base/video_frame.h"
#include "media/cast/cast_config.h"
#include "media/cast/cast_environment.h"
#include "media/cast/cast_receiver.h"
#include "media/cast/cast_sender.h"
#include "media/cast/logging/encoding_event_subscriber.h"
#include "media/cast/logging/log_serializer.h"
#include "media/cast/logging/logging_defines.h"
#include "media/cast/logging/proto/raw_events.pb.h"
#include "media/cast/logging/raw_event_subscriber_bundle.h"
#include "media/cast/logging/simple_event_subscriber.h"
#include "media/cast/net/cast_transport_config.h"
#include "media/cast/net/cast_transport_defines.h"
#include "media/cast/net/cast_transport_sender.h"
#include "media/cast/net/cast_transport_sender_impl.h"
#include "media/cast/test/fake_media_source.h"
#include "media/cast/test/fake_single_thread_task_runner.h"
#include "media/cast/test/loopback_transport.h"
#include "media/cast/test/proto/network_simulation_model.pb.h"
#include "media/cast/test/skewed_tick_clock.h"
#include "media/cast/test/utility/audio_utility.h"
#include "media/cast/test/utility/default_config.h"
#include "media/cast/test/utility/test_util.h"
#include "media/cast/test/utility/udp_proxy.h"
#include "media/cast/test/utility/video_utility.h"

using media::cast::proto::IPPModel;
using media::cast::proto::NetworkSimulationModel;
using media::cast::proto::NetworkSimulationModelType;

namespace media {
namespace cast {
namespace {
const int kTargetDelay = 300;
const char kSourcePath[] = "source";
const char kModelPath[] = "model";
const char kOutputPath[] = "output";
const char kSimulationId[] = "sim-id";
const char kLibDir[] = "lib-dir";

void UpdateCastTransportStatus(CastTransportStatus status) {
  LOG(INFO) << "Cast transport status: " << status;
}

void AudioInitializationStatus(CastInitializationStatus status) {
  LOG(INFO) << "Audio status: " << status;
}

void VideoInitializationStatus(CastInitializationStatus status) {
  LOG(INFO) << "Video status: " << status;
}

void LogTransportEvents(const scoped_refptr<CastEnvironment>& env,
                        const std::vector<PacketEvent>& packet_events,
                        const std::vector<FrameEvent>& frame_events) {
  for (std::vector<media::cast::PacketEvent>::const_iterator it =
           packet_events.begin();
       it != packet_events.end();
       ++it) {
    env->Logging()->InsertPacketEvent(it->timestamp,
                                      it->type,
                                      it->media_type,
                                      it->rtp_timestamp,
                                      it->frame_id,
                                      it->packet_id,
                                      it->max_packet_id,
                                      it->size);
  }
  for (std::vector<media::cast::FrameEvent>::const_iterator it =
           frame_events.begin();
       it != frame_events.end();
       ++it) {
    if (it->type == FRAME_PLAYOUT) {
      env->Logging()->InsertFrameEventWithDelay(
          it->timestamp,
          it->type,
          it->media_type,
          it->rtp_timestamp,
          it->frame_id,
          it->delay_delta);
    } else {
      env->Logging()->InsertFrameEvent(
          it->timestamp,
          it->type,
          it->media_type,
          it->rtp_timestamp,
          it->frame_id);
    }
  }
}

void GotVideoFrame(
    int* counter,
    CastReceiver* cast_receiver,
    const scoped_refptr<media::VideoFrame>& video_frame,
    const base::TimeTicks& render_time,
    bool continuous) {
  ++*counter;
  cast_receiver->RequestDecodedVideoFrame(
      base::Bind(&GotVideoFrame, counter, cast_receiver));
}

void GotAudioFrame(
    int* counter,
    CastReceiver* cast_receiver,
    scoped_ptr<AudioBus> audio_bus,
    const base::TimeTicks& playout_time,
    bool is_continuous) {
  ++*counter;
  cast_receiver->RequestDecodedAudioFrame(
      base::Bind(&GotAudioFrame, counter, cast_receiver));
}

// Serialize |frame_events| and |packet_events| and append to the file
// located at |output_path|.
void AppendLogToFile(media::cast::proto::LogMetadata* metadata,
                     const media::cast::FrameEventList& frame_events,
                     const media::cast::PacketEventList& packet_events,
                     const base::FilePath& output_path) {
  media::cast::proto::GeneralDescription* gen_desc =
      metadata->mutable_general_description();
  gen_desc->set_product("Cast Simulator");
  gen_desc->set_product_version("0.1");

  scoped_ptr<char[]> serialized_log(new char[media::cast::kMaxSerializedBytes]);
  int output_bytes;
  bool success = media::cast::SerializeEvents(*metadata,
                                              frame_events,
                                              packet_events,
                                              true,
                                              media::cast::kMaxSerializedBytes,
                                              serialized_log.get(),
                                              &output_bytes);

  if (!success) {
    LOG(ERROR) << "Failed to serialize log.";
    return;
  }

  if (AppendToFile(output_path, serialized_log.get(), output_bytes) == -1) {
    LOG(ERROR) << "Failed to append to log.";
  }
}

// Run simulation once.
//
// |output_path| is the path to write serialized log.
// |extra_data| is extra tagging information to write to log.
void RunSimulation(const base::FilePath& source_path,
                   const base::FilePath& output_path,
                   const std::string& extra_data,
                   const NetworkSimulationModel& model) {
  // Fake clock. Make sure start time is non zero.
  base::SimpleTestTickClock testing_clock;
  testing_clock.Advance(base::TimeDelta::FromSeconds(1));

  // Task runner.
  scoped_refptr<test::FakeSingleThreadTaskRunner> task_runner =
      new test::FakeSingleThreadTaskRunner(&testing_clock);
  base::ThreadTaskRunnerHandle task_runner_handle(task_runner);

  // CastEnvironments.
  scoped_refptr<CastEnvironment> sender_env =
      new CastEnvironment(
          scoped_ptr<base::TickClock>(
              new test::SkewedTickClock(&testing_clock)).Pass(),
          task_runner,
          task_runner,
          task_runner);
  scoped_refptr<CastEnvironment> receiver_env =
      new CastEnvironment(
          scoped_ptr<base::TickClock>(
              new test::SkewedTickClock(&testing_clock)).Pass(),
          task_runner,
          task_runner,
          task_runner);

  // Event subscriber. Store at most 1 hour of events.
  EncodingEventSubscriber audio_event_subscriber(AUDIO_EVENT,
                                                 100 * 60 * 60);
  EncodingEventSubscriber video_event_subscriber(VIDEO_EVENT,
                                                 30 * 60 * 60);
  sender_env->Logging()->AddRawEventSubscriber(&audio_event_subscriber);
  sender_env->Logging()->AddRawEventSubscriber(&video_event_subscriber);

  // Audio sender config.
  AudioSenderConfig audio_sender_config = GetDefaultAudioSenderConfig();
  audio_sender_config.target_playout_delay =
      base::TimeDelta::FromMilliseconds(kTargetDelay);

  // Audio receiver config.
  FrameReceiverConfig audio_receiver_config =
      GetDefaultAudioReceiverConfig();
  audio_receiver_config.rtp_max_delay_ms =
      audio_sender_config.target_playout_delay.InMilliseconds();

  // Video sender config.
  VideoSenderConfig video_sender_config = GetDefaultVideoSenderConfig();
  video_sender_config.max_bitrate = 4000000;
  video_sender_config.min_bitrate = 2000000;
  video_sender_config.start_bitrate = 4000000;
  video_sender_config.target_playout_delay =
      base::TimeDelta::FromMilliseconds(kTargetDelay);

  // Video receiver config.
  FrameReceiverConfig video_receiver_config =
      GetDefaultVideoReceiverConfig();
  video_receiver_config.rtp_max_delay_ms =
      video_sender_config.target_playout_delay.InMilliseconds();

  // Loopback transport.
  LoopBackTransport receiver_to_sender(receiver_env);
  LoopBackTransport sender_to_receiver(sender_env);

  // Cast receiver.
  scoped_ptr<CastReceiver> cast_receiver(
      CastReceiver::Create(receiver_env,
                           audio_receiver_config,
                           video_receiver_config,
                           &receiver_to_sender));

  // Cast sender and transport sender.
  scoped_ptr<CastTransportSender> transport_sender(
      new CastTransportSenderImpl(
          NULL,
          &testing_clock,
          net::IPEndPoint(),
          base::Bind(&UpdateCastTransportStatus),
          base::Bind(&LogTransportEvents, sender_env),
          base::TimeDelta::FromSeconds(1),
          task_runner,
          &sender_to_receiver));
  scoped_ptr<CastSender> cast_sender(
      CastSender::Create(sender_env, transport_sender.get()));

  // Build packet pipe.
  if (model.type() != media::cast::proto::INTERRUPTED_POISSON_PROCESS) {
    LOG(ERROR) << "Unknown model type " << model.type() << ".";
    return;
  }

  const IPPModel& ipp_model = model.ipp();

  std::vector<double> average_rates(ipp_model.average_rate_size());
  std::copy(ipp_model.average_rate().begin(), ipp_model.average_rate().end(),
      average_rates.begin());
  test::InterruptedPoissonProcess ipp(average_rates,
      ipp_model.coef_burstiness(), ipp_model.coef_variance(), 0);

  // Connect sender to receiver. This initializes the pipe.
  receiver_to_sender.Initialize(
      ipp.NewBuffer(128 * 1024).Pass(),
      transport_sender->PacketReceiverForTesting(),
      task_runner, &testing_clock);
  sender_to_receiver.Initialize(
      ipp.NewBuffer(128 * 1024).Pass(),
      cast_receiver->packet_receiver(), task_runner,
      &testing_clock);

  // Start receiver.
  int audio_frame_count = 0;
  int video_frame_count = 0;
  cast_receiver->RequestDecodedVideoFrame(
      base::Bind(&GotVideoFrame, &video_frame_count, cast_receiver.get()));
  cast_receiver->RequestDecodedAudioFrame(
      base::Bind(&GotAudioFrame, &audio_frame_count, cast_receiver.get()));

  FakeMediaSource media_source(task_runner,
                               &testing_clock,
                               video_sender_config);

  // Initializing audio and video senders.
  cast_sender->InitializeAudio(audio_sender_config,
                               base::Bind(&AudioInitializationStatus));
  cast_sender->InitializeVideo(media_source.get_video_config(),
                               base::Bind(&VideoInitializationStatus),
                               CreateDefaultVideoEncodeAcceleratorCallback(),
                               CreateDefaultVideoEncodeMemoryCallback());

  // Start sending.
  if (!source_path.empty()) {
    // 0 means using the FPS from the file.
    media_source.SetSourceFile(source_path, 0);
  }
  media_source.Start(cast_sender->audio_frame_input(),
                     cast_sender->video_frame_input());

  // Run for 3 minutes.
  base::TimeDelta elapsed_time;
  while (elapsed_time.InMinutes() < 3) {
    // Each step is 100us.
    base::TimeDelta step = base::TimeDelta::FromMicroseconds(100);
    task_runner->Sleep(step);
    elapsed_time += step;
  }

  // Get event logs for audio and video.
  media::cast::proto::LogMetadata audio_metadata, video_metadata;
  media::cast::FrameEventList audio_frame_events, video_frame_events;
  media::cast::PacketEventList audio_packet_events, video_packet_events;
  audio_metadata.set_extra_data(extra_data);
  video_metadata.set_extra_data(extra_data);
  audio_event_subscriber.GetEventsAndReset(
      &audio_metadata, &audio_frame_events, &audio_packet_events);
  video_event_subscriber.GetEventsAndReset(
      &video_metadata, &video_frame_events, &video_packet_events);

  // Print simulation results.

  // Compute and print statistics for video:
  //
  // * Total video frames captured.
  // * Total video frames encoded.
  // * Total video frames dropped.
  // * Total video frames received late.
  // * Average target bitrate.
  // * Average encoded bitrate.
  int total_video_frames = 0;
  int encoded_video_frames = 0;
  int dropped_video_frames = 0;
  int late_video_frames = 0;
  int64 encoded_size = 0;
  int64 target_bitrate = 0;
  for (size_t i = 0; i < video_frame_events.size(); ++i) {
    const media::cast::proto::AggregatedFrameEvent& event =
        *video_frame_events[i];
    ++total_video_frames;
    if (event.has_encoded_frame_size()) {
      ++encoded_video_frames;
      encoded_size += event.encoded_frame_size();
      target_bitrate += event.target_bitrate();
    } else {
      ++dropped_video_frames;
    }
    if (event.has_delay_millis() && event.delay_millis() < 0)
      ++late_video_frames;
  }

  double avg_encoded_bitrate =
      !encoded_video_frames ? 0 :
      8.0 * encoded_size * video_sender_config.max_frame_rate /
      encoded_video_frames / 1000;
  double avg_target_bitrate =
      !encoded_video_frames ? 0 : target_bitrate / encoded_video_frames / 1000;

  LOG(INFO) << "Audio frame count: " << audio_frame_count;
  LOG(INFO) << "Total video frames: " << total_video_frames;
  LOG(INFO) << "Dropped video frames " << dropped_video_frames;
  LOG(INFO) << "Late video frames: " << late_video_frames;
  LOG(INFO) << "Average encoded bitrate (kbps): " << avg_encoded_bitrate;
  LOG(INFO) << "Average target bitrate (kbps): " << avg_target_bitrate;
  LOG(INFO) << "Writing log: " << output_path.value();

  // Truncate file and then write serialized log.
  {
    base::ScopedFILE file(base::OpenFile(output_path, "wb"));
    if (!file.get()) {
      LOG(INFO) << "Cannot write to log.";
      return;
    }
  }
  AppendLogToFile(&video_metadata, video_frame_events, video_packet_events,
                  output_path);
  AppendLogToFile(&audio_metadata, audio_frame_events, audio_packet_events,
                  output_path);
}

NetworkSimulationModel DefaultModel() {
  NetworkSimulationModel model;
  model.set_type(cast::proto::INTERRUPTED_POISSON_PROCESS);
  IPPModel* ipp = model.mutable_ipp();
  ipp->set_coef_burstiness(0.609);
  ipp->set_coef_variance(4.1);

  ipp->add_average_rate(0.609);
  ipp->add_average_rate(0.495);
  ipp->add_average_rate(0.561);
  ipp->add_average_rate(0.458);
  ipp->add_average_rate(0.538);
  ipp->add_average_rate(0.513);
  ipp->add_average_rate(0.585);
  ipp->add_average_rate(0.592);
  ipp->add_average_rate(0.658);
  ipp->add_average_rate(0.556);
  ipp->add_average_rate(0.371);
  ipp->add_average_rate(0.595);
  ipp->add_average_rate(0.490);
  ipp->add_average_rate(0.980);
  ipp->add_average_rate(0.781);
  ipp->add_average_rate(0.463);

  return model;
}

bool IsModelValid(const NetworkSimulationModel& model) {
  if (!model.has_type())
    return false;
  NetworkSimulationModelType type = model.type();
  if (type == media::cast::proto::INTERRUPTED_POISSON_PROCESS) {
    if (!model.has_ipp())
      return false;
    const IPPModel& ipp = model.ipp();
    if (ipp.coef_burstiness() <= 0.0 || ipp.coef_variance() <= 0.0)
      return false;
    if (ipp.average_rate_size() == 0)
      return false;
    for (int i = 0; i < ipp.average_rate_size(); i++) {
      if (ipp.average_rate(i) <= 0.0)
        return false;
    }
  }

  return true;
}

NetworkSimulationModel LoadModel(const base::FilePath& model_path) {
  if (model_path.empty()) {
    LOG(ERROR) << "Model path not set.";
    return DefaultModel();
  }
  std::string model_str;
  if (!base::ReadFileToString(model_path, &model_str)) {
    LOG(ERROR) << "Failed to read model file.";
    return DefaultModel();
  }

  NetworkSimulationModel model;
  if (!model.ParseFromString(model_str)) {
    LOG(ERROR) << "Failed to parse model.";
    return DefaultModel();
  }
  if (!IsModelValid(model)) {
    LOG(ERROR) << "Invalid model.";
    return DefaultModel();
  }

  return model;
}

}  // namespace
}  // namespace cast
}  // namespace media

int main(int argc, char** argv) {
  base::AtExitManager at_exit;
  CommandLine::Init(argc, argv);
  InitLogging(logging::LoggingSettings());

  const CommandLine* cmd = CommandLine::ForCurrentProcess();
  base::FilePath media_path = cmd->GetSwitchValuePath(media::cast::kLibDir);
  if (media_path.empty()) {
    if (!PathService::Get(base::DIR_MODULE, &media_path)) {
      LOG(ERROR) << "Failed to load FFmpeg.";
      return 1;
    }
  }

  if (!media::InitializeMediaLibrary(media_path)) {
    LOG(ERROR) << "Failed to initialize FFmpeg.";
    return 1;
  }

  base::FilePath source_path = cmd->GetSwitchValuePath(
      media::cast::kSourcePath);
  base::FilePath output_path = cmd->GetSwitchValuePath(
      media::cast::kOutputPath);
  if (output_path.empty()) {
    base::GetTempDir(&output_path);
    output_path = output_path.AppendASCII("sim-events.gz");
  }
  std::string sim_id = cmd->GetSwitchValueASCII(media::cast::kSimulationId);

  NetworkSimulationModel model = media::cast::LoadModel(
      cmd->GetSwitchValuePath(media::cast::kModelPath));

  base::DictionaryValue values;
  values.SetBoolean("sim", true);
  values.SetString("sim-id", sim_id);

  std::string extra_data;
  base::JSONWriter::Write(&values, &extra_data);

  // Run.
  media::cast::RunSimulation(source_path, output_path, extra_data, model);
  return 0;
}
