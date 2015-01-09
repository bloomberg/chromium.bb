// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fake_audio_input_stream.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/lazy_instance.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/audio_bus.h"
#include "media/base/media_switches.h"

using base::TimeTicks;
using base::TimeDelta;

namespace media {

namespace {

// These values are based on experiments for local-to-local
// PeerConnection to demonstrate audio/video synchronization.
const int kBeepDurationMilliseconds = 20;
const int kBeepFrequency = 400;

// Intervals between two automatic beeps.
const int kAutomaticBeepIntervalInMs = 500;

// Automatic beep will be triggered every |kAutomaticBeepIntervalInMs| unless
// users explicitly call BeepOnce(), which will disable the automatic beep.
class BeepContext {
 public:
  BeepContext() : beep_once_(false), automatic_beep_(true) {}

  void SetBeepOnce(bool enable) {
    base::AutoLock auto_lock(lock_);
    beep_once_ = enable;

    // Disable the automatic beep if users explicit set |beep_once_| to true.
    if (enable)
      automatic_beep_ = false;
  }
  bool beep_once() const {
    base::AutoLock auto_lock(lock_);
    return beep_once_;
  }
  bool automatic_beep() const {
    base::AutoLock auto_lock(lock_);
    return automatic_beep_;
  }

 private:
  mutable base::Lock lock_;
  bool beep_once_;
  bool automatic_beep_;
};

// Opens |wav_filename|, reads it and loads it as a wav file. This function will
// bluntly trigger CHECKs if we can't read the file or if it's malformed. The
// caller takes ownership of the returned data. The size of the data is stored
// in |read_length|.
scoped_ptr<uint8[]> ReadWavFile(const base::FilePath& wav_filename,
                                size_t* file_length) {
  base::File wav_file(
      wav_filename, base::File::FLAG_OPEN | base::File::FLAG_READ);
  if (!wav_file.IsValid()) {
    CHECK(false) << "Failed to read " << wav_filename.value() << " as input to "
                 << "the fake device.";
    return nullptr;
  }

  size_t wav_file_length = wav_file.GetLength();
  CHECK_GT(wav_file_length, 0u)
      << "Input file to fake device is empty: " << wav_filename.value();

  uint8* wav_file_data = new uint8[wav_file_length];
  size_t read_bytes = wav_file.Read(0, reinterpret_cast<char*>(wav_file_data),
                                    wav_file_length);
  if (read_bytes != wav_file_length) {
    CHECK(false) << "Failed to read all bytes of " << wav_filename.value();
    return nullptr;
  }
  *file_length = wav_file_length;
  return scoped_ptr<uint8[]>(wav_file_data);
}

// Opens |wav_filename|, reads it and loads it as a Wav file. This function will
// bluntly trigger CHECKs if we can't read the file or if it's malformed.
scoped_ptr<media::WavAudioHandler> CreateWavAudioHandler(
    const base::FilePath& wav_filename, const uint8* wav_file_data,
    size_t wav_file_length, const AudioParameters& expected_params) {
  base::StringPiece wav_data(reinterpret_cast<const char*>(wav_file_data),
                             wav_file_length);
  scoped_ptr<media::WavAudioHandler> wav_audio_handler(
      new media::WavAudioHandler(wav_data));
  return wav_audio_handler.Pass();
}

static base::LazyInstance<BeepContext> g_beep_context =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

AudioInputStream* FakeAudioInputStream::MakeFakeStream(
    AudioManagerBase* manager,
    const AudioParameters& params) {
  return new FakeAudioInputStream(manager, params);
}

FakeAudioInputStream::FakeAudioInputStream(AudioManagerBase* manager,
                                           const AudioParameters& params)
    : audio_manager_(manager),
      callback_(NULL),
      buffer_size_((params.channels() * params.bits_per_sample() *
                    params.frames_per_buffer()) /
                   8),
      params_(params),
      task_runner_(manager->GetTaskRunner()),
      callback_interval_(base::TimeDelta::FromMilliseconds(
          (params.frames_per_buffer() * 1000) / params.sample_rate())),
      beep_duration_in_buffers_(kBeepDurationMilliseconds *
                                params.sample_rate() /
                                params.frames_per_buffer() /
                                1000),
      beep_generated_in_buffers_(0),
      beep_period_in_frames_(params.sample_rate() / kBeepFrequency),
      audio_bus_(AudioBus::Create(params)),
      wav_file_read_pos_(0),
      weak_factory_(this) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
}

FakeAudioInputStream::~FakeAudioInputStream() {}

bool FakeAudioInputStream::Open() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  buffer_.reset(new uint8[buffer_size_]);
  memset(buffer_.get(), 0, buffer_size_);
  audio_bus_->Zero();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFileForFakeAudioCapture)) {
    OpenInFileMode(base::CommandLine::ForCurrentProcess()->GetSwitchValuePath(
        switches::kUseFileForFakeAudioCapture));
  }

  return true;
}

void FakeAudioInputStream::Start(AudioInputCallback* callback)  {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  DCHECK(!callback_);
  callback_ = callback;
  last_callback_time_ = TimeTicks::Now();

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeAudioInputStream::DoCallback, weak_factory_.GetWeakPtr()),
      callback_interval_);
}

void FakeAudioInputStream::DoCallback() {
  DCHECK(callback_);

  const TimeTicks now = TimeTicks::Now();
  base::TimeDelta next_callback_time =
      last_callback_time_ + callback_interval_ * 2 - now;

  // If we are falling behind, try to catch up as much as we can in the next
  // callback.
  if (next_callback_time < base::TimeDelta())
    next_callback_time = base::TimeDelta();

  if (PlayingFromFile()) {
    PlayFile();
  } else {
    PlayBeep();
  }

  last_callback_time_ = now;

  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::Bind(&FakeAudioInputStream::DoCallback, weak_factory_.GetWeakPtr()),
      next_callback_time);
}

void FakeAudioInputStream::OpenInFileMode(const base::FilePath& wav_filename) {
  CHECK(!wav_filename.empty())
      << "You must pass the file to use as argument to --"
      << switches::kUseFileForFakeAudioCapture << ".";

  // Read the file, and put its data in a scoped_ptr so it gets deleted later.
  size_t file_length = 0;
  wav_file_data_ = ReadWavFile(wav_filename, &file_length);
  wav_audio_handler_ = CreateWavAudioHandler(
      wav_filename, wav_file_data_.get(), file_length, params_);

  // Hook us up so we pull in data from the file into the converter. We need to
  // modify the wav file's audio parameters since we'll be reading small slices
  // of it at a time and not the whole thing (like 10 ms at a time).
  AudioParameters file_audio_slice(
      wav_audio_handler_->params().format(),
      wav_audio_handler_->params().channel_layout(),
      wav_audio_handler_->params().sample_rate(),
      wav_audio_handler_->params().bits_per_sample(),
      params_.frames_per_buffer());

  file_audio_converter_.reset(
      new AudioConverter(file_audio_slice, params_, false));
  file_audio_converter_->AddInput(this);
}

bool FakeAudioInputStream::PlayingFromFile() {
  return wav_audio_handler_.get() != nullptr;
}

void FakeAudioInputStream::PlayFile() {
  // Stop playing if we've played out the whole file.
  if (wav_audio_handler_->AtEnd(wav_file_read_pos_))
    return;

  file_audio_converter_->Convert(audio_bus_.get());
  callback_->OnData(this, audio_bus_.get(), buffer_size_, 1.0);
}

void FakeAudioInputStream::PlayBeep() {
  // Accumulate the time from the last beep.
  interval_from_last_beep_ += TimeTicks::Now() - last_callback_time_;

  memset(buffer_.get(), 0, buffer_size_);
  bool should_beep = false;
  {
    BeepContext* beep_context = g_beep_context.Pointer();
    if (beep_context->automatic_beep()) {
      base::TimeDelta delta = interval_from_last_beep_ -
          TimeDelta::FromMilliseconds(kAutomaticBeepIntervalInMs);
      if (delta > base::TimeDelta()) {
        should_beep = true;
        interval_from_last_beep_ = delta;
      }
    } else {
      should_beep = beep_context->beep_once();
      beep_context->SetBeepOnce(false);
    }
  }

  // If this object was instructed to generate a beep or has started to
  // generate a beep sound.
  if (should_beep || beep_generated_in_buffers_) {
    // Compute the number of frames to output high value. Then compute the
    // number of bytes based on channels and bits per channel.
    int high_frames = beep_period_in_frames_ / 2;
    int high_bytes = high_frames * params_.bits_per_sample() *
        params_.channels() / 8;

    // Separate high and low with the same number of bytes to generate a
    // square wave.
    int position = 0;
    while (position + high_bytes <= buffer_size_) {
      // Write high values first.
      memset(buffer_.get() + position, 128, high_bytes);
      // Then leave low values in the buffer with |high_bytes|.
      position += high_bytes * 2;
    }

    ++beep_generated_in_buffers_;
    if (beep_generated_in_buffers_ >= beep_duration_in_buffers_)
      beep_generated_in_buffers_ = 0;
  }

  audio_bus_->FromInterleaved(
      buffer_.get(), audio_bus_->frames(), params_.bits_per_sample() / 8);
  callback_->OnData(this, audio_bus_.get(), buffer_size_, 1.0);
}

void FakeAudioInputStream::Stop() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  weak_factory_.InvalidateWeakPtrs();
  callback_ = NULL;
}

void FakeAudioInputStream::Close() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  audio_manager_->ReleaseInputStream(this);
}

double FakeAudioInputStream::GetMaxVolume() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return 1.0;
}

void FakeAudioInputStream::SetVolume(double volume) {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
}

double FakeAudioInputStream::GetVolume() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return 1.0;
}

bool FakeAudioInputStream::IsMuted() {
  DCHECK(audio_manager_->GetTaskRunner()->BelongsToCurrentThread());
  return false;
}

bool FakeAudioInputStream::SetAutomaticGainControl(bool enabled) {
  return false;
}

bool FakeAudioInputStream::GetAutomaticGainControl() {
  return false;
}

// static
void FakeAudioInputStream::BeepOnce() {
  BeepContext* beep_context = g_beep_context.Pointer();
  beep_context->SetBeepOnce(true);
}

double FakeAudioInputStream::ProvideInput(AudioBus* audio_bus_into_converter,
                                          base::TimeDelta buffer_delay) {
  // Unfilled frames will be zeroed by CopyTo.
  size_t bytes_written;
  wav_audio_handler_->CopyTo(audio_bus_into_converter, wav_file_read_pos_,
                             &bytes_written);
  wav_file_read_pos_ += bytes_written;
  return 1.0;
};

}  // namespace media
