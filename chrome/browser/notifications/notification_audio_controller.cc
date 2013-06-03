// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_audio_controller.h"

#include <cstring>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/cancelable_callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sys_byteorder.h"
#include "base/task_runner_util.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_parameters.h"
#include "media/base/audio_bus.h"
#include "media/base/channel_layout.h"

namespace {

// The size of the header of a wav file. The header consists of 'RIFF', 4 bytes
// of total data length, and 'WAVE'.
const size_t kWavFileHeaderSize = 12;

// The size of a chunk header in wav file format. A chunk header consists of a
// tag ('fmt ' or 'data') and 4 bytes of chunk length.
const size_t kChunkHeaderSize = 8;

// The minimum size of 'fmt' chunk.
const size_t kFmtChunkMinimumSize = 16;

// The offsets of 'fmt' fields.
const size_t kAudioFormatOffset = 0;
const size_t kChunnelOffset = 2;
const size_t kSampleRateOffset = 4;
const size_t kBitsPerSampleOffset = 14;

// Some constants for audio format.
const int kAudioFormatPCM = 1;

// The frame-per-buffer parameter for wav data format reader. Since wav format
// doesn't have this parameter in its header, just choose some value.
const int kFramesPerBuffer = 4096;

///////////////////////////////////////////////////////////////////////////
// WavAudioHandler
//
// This class provides the input from wav file format.
// See https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
class WavAudioHandler {
 public:
  explicit WavAudioHandler(const base::StringPiece& data);
  virtual ~WavAudioHandler();

  // Returns true when it plays to the end of the track.
  bool AtEnd();

  // Copies the audio data to |dest|.
  void CopyTo(media::AudioBus* dest);

  uint16 num_channels() const { return num_channels_; }
  uint32 sample_rate() const { return sample_rate_; }
  uint16 bits_per_sample() const { return bits_per_sample_; }

 private:
  // Parses a chunk of wav format data. Returns the length of the chunk.
  int ParseSubChunk(const base::StringPiece& data);

  // Parses the 'fmt' section chunk and stores |params_|.
  void ParseFmtChunk(const base::StringPiece& data);

  // Parses the 'data' section chunk and stores |data_|.
  void ParseDataChunk(const base::StringPiece& data);

  // Reads an integer from |data| with |offset|.
  template<typename T> T ReadInt(const base::StringPiece& data, size_t offset);

  base::StringPiece data_;
  size_t cursor_;
  uint16 num_channels_;
  uint32 sample_rate_;
  uint16 bits_per_sample_;

  DISALLOW_COPY_AND_ASSIGN(WavAudioHandler);
};

WavAudioHandler::WavAudioHandler(const base::StringPiece& data)
    : cursor_(0),
      num_channels_(0),
      sample_rate_(0),
      bits_per_sample_(0) {
  if (data.size() < kWavFileHeaderSize)
    return;

  if (!data.starts_with("RIFF") || ::memcmp(data.data() + 8, "WAVE", 4) != 0)
    return;

  uint32 total_length =
      std::min(ReadInt<uint32>(data, 4), static_cast<uint32>(data.size()));

  uint32 offset = kWavFileHeaderSize;
  while (offset < total_length)
    offset += ParseSubChunk(data.substr(offset));
}

WavAudioHandler::~WavAudioHandler() {
}

bool WavAudioHandler::AtEnd() {
  return data_.size() <= cursor_;
}

void WavAudioHandler::CopyTo(media::AudioBus* dest) {
  DCHECK_EQ(dest->channels(), num_channels_);
  if (cursor_ >= data_.size()) {
    dest->Zero();
    return;
  }

  const int bytes_per_sample = bits_per_sample_ / 8;
  const int bytes_per_frame = num_channels_ * bytes_per_sample;
  const int remaining_frames = (data_.size() - cursor_) / bytes_per_frame;
  const int frames = std::min(dest->frames(), remaining_frames);
  dest->FromInterleaved(data_.data() + cursor_, frames, bytes_per_sample);
  cursor_ += frames * bytes_per_frame;
  dest->ZeroFramesPartial(frames, dest->frames() - frames);
}

int WavAudioHandler::ParseSubChunk(const base::StringPiece& data) {
  if (data.size() < kChunkHeaderSize)
    return data.size();
  uint32 chunk_length = ReadInt<uint32>(data, 4);

  if (data.starts_with("fmt "))
    ParseFmtChunk(data.substr(kChunkHeaderSize, chunk_length));
  else if (data.starts_with("data"))
    ParseDataChunk(data.substr(kChunkHeaderSize, chunk_length));
  else
    DLOG(WARNING) << "Unknown data chunk: " << data.substr(0, 4);
  return chunk_length + kChunkHeaderSize;
}

void WavAudioHandler::ParseFmtChunk(const base::StringPiece& data) {
  if (data.size() < kFmtChunkMinimumSize) {
    DLOG(ERROR) << "data size " << data.size() << " is too short.";
    return;
  }

  DCHECK_EQ(ReadInt<uint16>(data, kAudioFormatOffset), kAudioFormatPCM);

  num_channels_ = ReadInt<uint16>(data, kChunnelOffset);
  sample_rate_ = ReadInt<uint32>(data, kSampleRateOffset);
  bits_per_sample_ = ReadInt<uint16>(data, kBitsPerSampleOffset);
}

void WavAudioHandler::ParseDataChunk(const base::StringPiece& data) {
  data_ = data;
}

template<typename T> T WavAudioHandler::ReadInt(const base::StringPiece& data,
                                                size_t offset) {
  DCHECK_LE(offset + sizeof(T), data.size());
  T result;
  ::memcpy(&result, data.data() + offset, sizeof(T));
#if !defined(ARCH_CPU_LITTLE_ENDIAN)
  result = base::ByteSwap(result);
#endif

  return result;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////
// NotificationAudioController::AudioHandler
//
// This class connects a sound for a notification to the audio manager. It
// will be released by its owner when the sound ends.
class NotificationAudioController::AudioHandler
    : public media::AudioOutputStream::AudioSourceCallback {
 public:
  typedef base::Callback<void(AudioHandler*)> OnFinishedCB;

  AudioHandler(const OnFinishedCB& on_finished_callback,
               const std::string& notification_id,
               const Profile* profile,
               const base::StringPiece& data);
  virtual ~AudioHandler();

  const std::string& notification_id() const { return notification_id_; }
  const Profile* profile() const { return profile_; }

  // Start playing the sound with the specified format. Returns true when it's
  // successfully scheduled.
  bool StartPlayingSound(media::AudioParameters::Format format);

  // Stops the playing sound request.
  void StopPlayingSound();

 private:
  // media::AudioOutputStream::AudioSourceCallback overrides:
  virtual int OnMoreData(media::AudioBus* dest,
                         media::AudioBuffersState state) OVERRIDE;
  virtual int OnMoreIOData(media::AudioBus* source,
                           media::AudioBus* dest,
                           media::AudioBuffersState state) OVERRIDE;
  virtual void OnError(media::AudioOutputStream* stream) OVERRIDE;

  OnFinishedCB on_finished_callback_;
  base::CancelableClosure stop_playing_sound_;
  std::string notification_id_;
  const Profile* profile_;

  media::AudioManager* audio_manager_;
  WavAudioHandler wav_audio_;
  media::AudioOutputStream* stream_;

  DISALLOW_COPY_AND_ASSIGN(AudioHandler);
};

NotificationAudioController::AudioHandler::AudioHandler(
    const OnFinishedCB& on_finished_callback,
    const std::string& notification_id,
    const Profile* profile,
    const base::StringPiece& data)
    : on_finished_callback_(on_finished_callback),
      notification_id_(notification_id),
      profile_(profile),
      audio_manager_(media::AudioManager::Get()),
      wav_audio_(data) {
}

NotificationAudioController::AudioHandler::~AudioHandler() {
}

bool NotificationAudioController::AudioHandler::StartPlayingSound(
    media::AudioParameters::Format format) {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());

  media::AudioParameters params = media::AudioParameters(
      format,
      media::GuessChannelLayout(wav_audio_.num_channels()),
      wav_audio_.num_channels(),
      wav_audio_.sample_rate(),
      wav_audio_.bits_per_sample(),
      kFramesPerBuffer);

  if (!params.IsValid())
    return false;

  stream_ = audio_manager_->MakeAudioOutputStreamProxy(params, std::string());
  if (!stream_->Open()) {
    DLOG(ERROR) << "Failed to open the output stream";
    stream_->Close();
    return false;
  }

  stop_playing_sound_.Reset(base::Bind(
      &AudioHandler::StopPlayingSound, base::Unretained(this)));
  stream_->Start(this);
  return true;
}

// Stops actually the audio output stream.
void NotificationAudioController::AudioHandler::StopPlayingSound() {
  DCHECK(audio_manager_->GetMessageLoop()->BelongsToCurrentThread());
  stop_playing_sound_.Cancel();

  // Close deletes |stream| too.
  stream_->Stop();
  stream_->Close();
  on_finished_callback_.Run(this);
}

int NotificationAudioController::AudioHandler::OnMoreData(
    media::AudioBus* dest,
    media::AudioBuffersState state) {
  wav_audio_.CopyTo(dest);
  if (wav_audio_.AtEnd()) {
    audio_manager_->GetMessageLoop()->PostTask(
        FROM_HERE, stop_playing_sound_.callback());
  }
  return dest->frames();
}

int NotificationAudioController::AudioHandler::OnMoreIOData(
    media::AudioBus* source,
    media::AudioBus* dest,
    media::AudioBuffersState state) {
  return OnMoreData(dest, state);
}

void NotificationAudioController::AudioHandler::OnError(
    media::AudioOutputStream* stream) {
}

///////////////////////////////////////////////////////////////////////////
// NotificationAudioController
//

NotificationAudioController::NotificationAudioController()
    : message_loop_(media::AudioManager::Get()->GetMessageLoop()),
      output_format_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY) {
}

NotificationAudioController::~NotificationAudioController() {
  DCHECK(message_loop_->BelongsToCurrentThread());
}

void NotificationAudioController::RequestPlayNotificationSound(
    const std::string& notification_id,
    const Profile* profile,
    const base::StringPiece& data) {
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &NotificationAudioController::PlayNotificationSound,
          base::Unretained(this),
          notification_id,
          base::Unretained(profile),
          data));
}

void NotificationAudioController::RequestShutdown() {
  message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &NotificationAudioController::Shutdown, base::Unretained(this)));
}

void NotificationAudioController::OnNotificationSoundFinished(
    AudioHandler* audio_handler) {
  DCHECK(message_loop_->BelongsToCurrentThread());
  std::vector<AudioHandler*>::iterator it =
      std::find(audio_handlers_.begin(), audio_handlers_.end(), audio_handler);
  DCHECK(it != audio_handlers_.end());
  audio_handlers_.erase(it);
}

void NotificationAudioController::UseFakeAudioOutputForTest() {
  output_format_ = media::AudioParameters::AUDIO_FAKE;
}

void NotificationAudioController::GetAudioHandlersSizeForTest(
    const base::Callback<void(size_t)>& on_get) {
  base::PostTaskAndReplyWithResult(
      message_loop_.get(),
      FROM_HERE,
      base::Bind(&NotificationAudioController::GetAudioHandlersSizeCallback,
                 base::Unretained(this)),
      on_get);
}

void NotificationAudioController::PlayNotificationSound(
    const std::string& notification_id,
    const Profile* profile,
    const base::StringPiece& data) {
  DCHECK(message_loop_->BelongsToCurrentThread());

  for (size_t i = 0; i < audio_handlers_.size(); ++i) {
    if (audio_handlers_[i]->notification_id() == notification_id &&
        audio_handlers_[i]->profile() == profile) {
      audio_handlers_[i]->StopPlayingSound();
      break;
    }
  }

  scoped_ptr<AudioHandler> new_handler(new AudioHandler(
      base::Bind(&NotificationAudioController::OnNotificationSoundFinished,
                 base::Unretained(this)),
      notification_id,
      profile,
      data));
  if (new_handler->StartPlayingSound(output_format_))
    audio_handlers_.push_back(new_handler.release());
}

void NotificationAudioController::Shutdown() {
  DCHECK(message_loop_->BelongsToCurrentThread());

  while (!audio_handlers_.empty())
    audio_handlers_[0]->StopPlayingSound();

  delete this;
}

size_t NotificationAudioController::GetAudioHandlersSizeCallback() {
  DCHECK(message_loop_->BelongsToCurrentThread());
  return audio_handlers_.size();
}
