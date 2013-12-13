// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/android/build_info.h"
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/audio/android/audio_manager_android.h"
#include "media/audio/audio_io.h"
#include "media/audio/audio_manager_base.h"
#include "media/base/decoder_buffer.h"
#include "media/base/seekable_buffer.h"
#include "media/base/test_data_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;

namespace media {

ACTION_P3(CheckCountAndPostQuitTask, count, limit, loop) {
  if (++*count >= limit) {
    loop->PostTask(FROM_HERE, base::MessageLoop::QuitClosure());
  }
}

static const char kSpeechFile_16b_s_48k[] = "speech_16b_stereo_48kHz.raw";
static const char kSpeechFile_16b_m_48k[] = "speech_16b_mono_48kHz.raw";
static const char kSpeechFile_16b_s_44k[] = "speech_16b_stereo_44kHz.raw";
static const char kSpeechFile_16b_m_44k[] = "speech_16b_mono_44kHz.raw";

static const float kCallbackTestTimeMs = 2000.0;
static const int kBitsPerSample = 16;
static const int kBytesPerSample = kBitsPerSample / 8;

// Converts AudioParameters::Format enumerator to readable string.
static std::string FormatToString(AudioParameters::Format format) {
  switch (format) {
    case AudioParameters::AUDIO_PCM_LINEAR:
      return std::string("AUDIO_PCM_LINEAR");
    case AudioParameters::AUDIO_PCM_LOW_LATENCY:
      return std::string("AUDIO_PCM_LOW_LATENCY");
    case AudioParameters::AUDIO_FAKE:
      return std::string("AUDIO_FAKE");
    case AudioParameters::AUDIO_LAST_FORMAT:
      return std::string("AUDIO_LAST_FORMAT");
    default:
      return std::string();
  }
}

// Converts ChannelLayout enumerator to readable string. Does not include
// multi-channel cases since these layouts are not supported on Android.
static std::string LayoutToString(ChannelLayout channel_layout) {
  switch (channel_layout) {
    case CHANNEL_LAYOUT_NONE:
      return std::string("CHANNEL_LAYOUT_NONE");
    case CHANNEL_LAYOUT_MONO:
      return std::string("CHANNEL_LAYOUT_MONO");
    case CHANNEL_LAYOUT_STEREO:
      return std::string("CHANNEL_LAYOUT_STEREO");
    case CHANNEL_LAYOUT_UNSUPPORTED:
    default:
      return std::string("CHANNEL_LAYOUT_UNSUPPORTED");
  }
}

static double ExpectedTimeBetweenCallbacks(AudioParameters params) {
  return (base::TimeDelta::FromMicroseconds(
              params.frames_per_buffer() * base::Time::kMicrosecondsPerSecond /
              static_cast<double>(params.sample_rate()))).InMillisecondsF();
}

std::ostream& operator<<(std::ostream& os, const AudioParameters& params) {
  using namespace std;
  os << endl << "format: " << FormatToString(params.format()) << endl
     << "channel layout: " << LayoutToString(params.channel_layout()) << endl
     << "sample rate: " << params.sample_rate() << endl
     << "bits per sample: " << params.bits_per_sample() << endl
     << "frames per buffer: " << params.frames_per_buffer() << endl
     << "channels: " << params.channels() << endl
     << "bytes per buffer: " << params.GetBytesPerBuffer() << endl
     << "bytes per second: " << params.GetBytesPerSecond() << endl
     << "bytes per frame: " << params.GetBytesPerFrame() << endl
     << "chunk size in ms: " << ExpectedTimeBetweenCallbacks(params) << endl
     << "echo_canceller: "
     << (params.effects() & AudioParameters::ECHO_CANCELLER);
  return os;
}

// Gmock implementation of AudioInputStream::AudioInputCallback.
class MockAudioInputCallback : public AudioInputStream::AudioInputCallback {
 public:
  MOCK_METHOD5(OnData,
               void(AudioInputStream* stream,
                    const uint8* src,
                    uint32 size,
                    uint32 hardware_delay_bytes,
                    double volume));
  MOCK_METHOD1(OnClose, void(AudioInputStream* stream));
  MOCK_METHOD1(OnError, void(AudioInputStream* stream));
};

// Gmock implementation of AudioOutputStream::AudioSourceCallback.
class MockAudioOutputCallback : public AudioOutputStream::AudioSourceCallback {
 public:
  MOCK_METHOD2(OnMoreData,
               int(AudioBus* dest, AudioBuffersState buffers_state));
  MOCK_METHOD3(OnMoreIOData,
               int(AudioBus* source,
                   AudioBus* dest,
                   AudioBuffersState buffers_state));
  MOCK_METHOD1(OnError, void(AudioOutputStream* stream));

  // We clear the data bus to ensure that the test does not cause noise.
  int RealOnMoreData(AudioBus* dest, AudioBuffersState buffers_state) {
    dest->Zero();
    return dest->frames();
  }
};

// Implements AudioOutputStream::AudioSourceCallback and provides audio data
// by reading from a data file.
class FileAudioSource : public AudioOutputStream::AudioSourceCallback {
 public:
  explicit FileAudioSource(base::WaitableEvent* event, const std::string& name)
      : event_(event), pos_(0) {
    // Reads a test file from media/test/data directory and stores it in
    // a DecoderBuffer.
    file_ = ReadTestDataFile(name);

    // Log the name of the file which is used as input for this test.
    base::FilePath file_path = GetTestDataFilePath(name);
    VLOG(0) << "Reading from file: " << file_path.value().c_str();
  }

  virtual ~FileAudioSource() {}

  // AudioOutputStream::AudioSourceCallback implementation.

  // Use samples read from a data file and fill up the audio buffer
  // provided to us in the callback.
  virtual int OnMoreData(AudioBus* audio_bus,
                         AudioBuffersState buffers_state) OVERRIDE {
    bool stop_playing = false;
    int max_size =
        audio_bus->frames() * audio_bus->channels() * kBytesPerSample;

    // Adjust data size and prepare for end signal if file has ended.
    if (pos_ + max_size > file_size()) {
      stop_playing = true;
      max_size = file_size() - pos_;
    }

    // File data is stored as interleaved 16-bit values. Copy data samples from
    // the file and deinterleave to match the audio bus format.
    // FromInterleaved() will zero out any unfilled frames when there is not
    // sufficient data remaining in the file to fill up the complete frame.
    int frames = max_size / (audio_bus->channels() * kBytesPerSample);
    if (max_size) {
      audio_bus->FromInterleaved(file_->data() + pos_, frames, kBytesPerSample);
      pos_ += max_size;
    }

    // Set event to ensure that the test can stop when the file has ended.
    if (stop_playing)
      event_->Signal();

    return frames;
  }

  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState buffers_state) OVERRIDE {
    NOTREACHED();
    return 0;
  }

  virtual void OnError(AudioOutputStream* stream) OVERRIDE {}

  int file_size() { return file_->data_size(); }

 private:
  base::WaitableEvent* event_;
  int pos_;
  scoped_refptr<DecoderBuffer> file_;

  DISALLOW_COPY_AND_ASSIGN(FileAudioSource);
};

// Implements AudioInputStream::AudioInputCallback and writes the recorded
// audio data to a local output file. Note that this implementation should
// only be used for manually invoked and evaluated tests, hence the created
// file will not be destroyed after the test is done since the intention is
// that it shall be available for off-line analysis.
class FileAudioSink : public AudioInputStream::AudioInputCallback {
 public:
  explicit FileAudioSink(base::WaitableEvent* event,
                         const AudioParameters& params,
                         const std::string& file_name)
      : event_(event), params_(params) {
    // Allocate space for ~10 seconds of data.
    const int kMaxBufferSize = 10 * params.GetBytesPerSecond();
    buffer_.reset(new media::SeekableBuffer(0, kMaxBufferSize));

    // Open up the binary file which will be written to in the destructor.
    base::FilePath file_path;
    EXPECT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &file_path));
    file_path = file_path.AppendASCII(file_name.c_str());
    binary_file_ = base::OpenFile(file_path, "wb");
    DLOG_IF(ERROR, !binary_file_) << "Failed to open binary PCM data file.";
    VLOG(0) << "Writing to file: " << file_path.value().c_str();
  }

  virtual ~FileAudioSink() {
    int bytes_written = 0;
    while (bytes_written < buffer_->forward_capacity()) {
      const uint8* chunk;
      int chunk_size;

      // Stop writing if no more data is available.
      if (!buffer_->GetCurrentChunk(&chunk, &chunk_size))
        break;

      // Write recorded data chunk to the file and prepare for next chunk.
      // TODO(henrika): use file_util:: instead.
      fwrite(chunk, 1, chunk_size, binary_file_);
      buffer_->Seek(chunk_size);
      bytes_written += chunk_size;
    }
    base::CloseFile(binary_file_);
  }

  // AudioInputStream::AudioInputCallback implementation.
  virtual void OnData(AudioInputStream* stream,
                      const uint8* src,
                      uint32 size,
                      uint32 hardware_delay_bytes,
                      double volume) OVERRIDE {
    // Store data data in a temporary buffer to avoid making blocking
    // fwrite() calls in the audio callback. The complete buffer will be
    // written to file in the destructor.
    if (!buffer_->Append(src, size))
      event_->Signal();
  }

  virtual void OnClose(AudioInputStream* stream) OVERRIDE {}
  virtual void OnError(AudioInputStream* stream) OVERRIDE {}

 private:
  base::WaitableEvent* event_;
  AudioParameters params_;
  scoped_ptr<media::SeekableBuffer> buffer_;
  FILE* binary_file_;

  DISALLOW_COPY_AND_ASSIGN(FileAudioSink);
};

// Implements AudioInputCallback and AudioSourceCallback to support full
// duplex audio where captured samples are played out in loopback after
// reading from a temporary FIFO storage.
class FullDuplexAudioSinkSource
    : public AudioInputStream::AudioInputCallback,
      public AudioOutputStream::AudioSourceCallback {
 public:
  explicit FullDuplexAudioSinkSource(const AudioParameters& params)
      : params_(params),
        previous_time_(base::TimeTicks::Now()),
        started_(false) {
    // Start with a reasonably small FIFO size. It will be increased
    // dynamically during the test if required.
    fifo_.reset(new media::SeekableBuffer(0, 2 * params.GetBytesPerBuffer()));
    buffer_.reset(new uint8[params_.GetBytesPerBuffer()]);
  }

  virtual ~FullDuplexAudioSinkSource() {}

  // AudioInputStream::AudioInputCallback implementation
  virtual void OnData(AudioInputStream* stream,
                      const uint8* src,
                      uint32 size,
                      uint32 hardware_delay_bytes,
                      double volume) OVERRIDE {
    const base::TimeTicks now_time = base::TimeTicks::Now();
    const int diff = (now_time - previous_time_).InMilliseconds();

    base::AutoLock lock(lock_);
    if (diff > 1000) {
      started_ = true;
      previous_time_ = now_time;

      // Log out the extra delay added by the FIFO. This is a best effort
      // estimate. We might be +- 10ms off here.
      int extra_fifo_delay =
          static_cast<int>(BytesToMilliseconds(fifo_->forward_bytes() + size));
      DVLOG(1) << extra_fifo_delay;
    }

    // We add an initial delay of ~1 second before loopback starts to ensure
    // a stable callback sequence and to avoid initial bursts which might add
    // to the extra FIFO delay.
    if (!started_)
      return;

    // Append new data to the FIFO and extend the size if the max capacity
    // was exceeded. Flush the FIFO when extended just in case.
    if (!fifo_->Append(src, size)) {
      fifo_->set_forward_capacity(2 * fifo_->forward_capacity());
      fifo_->Clear();
    }
  }

  virtual void OnClose(AudioInputStream* stream) OVERRIDE {}
  virtual void OnError(AudioInputStream* stream) OVERRIDE {}

  // AudioOutputStream::AudioSourceCallback implementation
  virtual int OnMoreData(AudioBus* dest,
                         AudioBuffersState buffers_state) OVERRIDE {
    const int size_in_bytes =
        (params_.bits_per_sample() / 8) * dest->frames() * dest->channels();
    EXPECT_EQ(size_in_bytes, params_.GetBytesPerBuffer());

    base::AutoLock lock(lock_);

    // We add an initial delay of ~1 second before loopback starts to ensure
    // a stable callback sequences and to avoid initial bursts which might add
    // to the extra FIFO delay.
    if (!started_) {
      dest->Zero();
      return dest->frames();
    }

    // Fill up destination with zeros if the FIFO does not contain enough
    // data to fulfill the request.
    if (fifo_->forward_bytes() < size_in_bytes) {
      dest->Zero();
    } else {
      fifo_->Read(buffer_.get(), size_in_bytes);
      dest->FromInterleaved(
          buffer_.get(), dest->frames(), params_.bits_per_sample() / 8);
    }

    return dest->frames();
  }

  virtual int OnMoreIOData(AudioBus* source,
                           AudioBus* dest,
                           AudioBuffersState buffers_state) OVERRIDE {
    NOTREACHED();
    return 0;
  }

  virtual void OnError(AudioOutputStream* stream) OVERRIDE {}

 private:
  // Converts from bytes to milliseconds given number of bytes and existing
  // audio parameters.
  double BytesToMilliseconds(int bytes) const {
    const int frames = bytes / params_.GetBytesPerFrame();
    return (base::TimeDelta::FromMicroseconds(
                frames * base::Time::kMicrosecondsPerSecond /
                static_cast<double>(params_.sample_rate()))).InMillisecondsF();
  }

  AudioParameters params_;
  base::TimeTicks previous_time_;
  base::Lock lock_;
  scoped_ptr<media::SeekableBuffer> fifo_;
  scoped_ptr<uint8[]> buffer_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(FullDuplexAudioSinkSource);
};

// Test fixture class for tests which only exercise the output path.
class AudioAndroidOutputTest : public testing::Test {
 public:
  AudioAndroidOutputTest() {}

 protected:
  virtual void SetUp() {
    audio_manager_.reset(AudioManager::CreateForTesting());
    loop_.reset(new base::MessageLoopForUI());
  }

  virtual void TearDown() {}

  AudioManager* audio_manager() { return audio_manager_.get(); }
  base::MessageLoopForUI* loop() { return loop_.get(); }

  AudioParameters GetDefaultOutputStreamParameters() {
    return audio_manager()->GetDefaultOutputStreamParameters();
  }

  double AverageTimeBetweenCallbacks(int num_callbacks) const {
    return ((end_time_ - start_time_) / static_cast<double>(num_callbacks - 1))
        .InMillisecondsF();
  }

  void StartOutputStreamCallbacks(const AudioParameters& params) {
    double expected_time_between_callbacks_ms =
        ExpectedTimeBetweenCallbacks(params);
    const int num_callbacks =
        (kCallbackTestTimeMs / expected_time_between_callbacks_ms);
    AudioOutputStream* stream = audio_manager()->MakeAudioOutputStream(
        params, std::string(), std::string());
    EXPECT_TRUE(stream);

    int count = 0;
    MockAudioOutputCallback source;

    EXPECT_CALL(source, OnMoreData(NotNull(), _))
        .Times(AtLeast(num_callbacks))
        .WillRepeatedly(
             DoAll(CheckCountAndPostQuitTask(&count, num_callbacks, loop()),
                   Invoke(&source, &MockAudioOutputCallback::RealOnMoreData)));
    EXPECT_CALL(source, OnError(stream)).Times(0);
    EXPECT_CALL(source, OnMoreIOData(_, _, _)).Times(0);

    EXPECT_TRUE(stream->Open());
    stream->Start(&source);
    start_time_ = base::TimeTicks::Now();
    loop()->Run();
    end_time_ = base::TimeTicks::Now();
    stream->Stop();
    stream->Close();

    double average_time_between_callbacks_ms =
        AverageTimeBetweenCallbacks(num_callbacks);
    VLOG(0) << "expected time between callbacks: "
            << expected_time_between_callbacks_ms << " ms";
    VLOG(0) << "average time between callbacks: "
            << average_time_between_callbacks_ms << " ms";
    EXPECT_GE(average_time_between_callbacks_ms,
              0.70 * expected_time_between_callbacks_ms);
    EXPECT_LE(average_time_between_callbacks_ms,
              1.30 * expected_time_between_callbacks_ms);
  }

  scoped_ptr<base::MessageLoopForUI> loop_;
  scoped_ptr<AudioManager> audio_manager_;
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioAndroidOutputTest);
};

// AudioRecordInputStream should only be created on Jelly Bean and higher. This
// ensures we only test against the AudioRecord path when that is satisfied.
std::vector<bool> RunAudioRecordInputPathTests() {
  std::vector<bool> tests;
  tests.push_back(false);
  if (base::android::BuildInfo::GetInstance()->sdk_int() >= 16)
    tests.push_back(true);
  return tests;
}

// Test fixture class for tests which exercise the input path, or both input and
// output paths. It is value-parameterized to test against both the Java
// AudioRecord (when true) and native OpenSLES (when false) input paths.
class AudioAndroidInputTest : public AudioAndroidOutputTest,
                              public testing::WithParamInterface<bool> {
 public:
  AudioAndroidInputTest() {}

 protected:
  AudioParameters GetInputStreamParameters() {
    AudioParameters input_params = audio_manager()->GetInputStreamParameters(
        AudioManagerBase::kDefaultDeviceId);
    // Override the platform effects setting to use the AudioRecord or OpenSLES
    // path as requested.
    int effects = GetParam() ? AudioParameters::ECHO_CANCELLER :
                               AudioParameters::NO_EFFECTS;
    AudioParameters params(input_params.format(),
                           input_params.channel_layout(),
                           input_params.input_channels(),
                           input_params.sample_rate(),
                           input_params.bits_per_sample(),
                           input_params.frames_per_buffer(),
                           effects);
    return params;
  }

  void StartInputStreamCallbacks(const AudioParameters& params) {
    double expected_time_between_callbacks_ms =
        ExpectedTimeBetweenCallbacks(params);
    const int num_callbacks =
        (kCallbackTestTimeMs / expected_time_between_callbacks_ms);
    AudioInputStream* stream = audio_manager()->MakeAudioInputStream(
        params, AudioManagerBase::kDefaultDeviceId);
    EXPECT_TRUE(stream);

    int count = 0;
    MockAudioInputCallback sink;

    EXPECT_CALL(sink,
                OnData(stream, NotNull(), params.GetBytesPerBuffer(), _, _))
        .Times(AtLeast(num_callbacks))
        .WillRepeatedly(
             CheckCountAndPostQuitTask(&count, num_callbacks, loop()));
    EXPECT_CALL(sink, OnError(stream)).Times(0);
    EXPECT_CALL(sink, OnClose(stream)).Times(1);

    EXPECT_TRUE(stream->Open());
    stream->Start(&sink);
    start_time_ = base::TimeTicks::Now();
    loop()->Run();
    end_time_ = base::TimeTicks::Now();
    stream->Stop();
    stream->Close();

    double average_time_between_callbacks_ms =
        AverageTimeBetweenCallbacks(num_callbacks);
    VLOG(0) << "expected time between callbacks: "
            << expected_time_between_callbacks_ms << " ms";
    VLOG(0) << "average time between callbacks: "
            << average_time_between_callbacks_ms << " ms";
    EXPECT_GE(average_time_between_callbacks_ms,
              0.70 * expected_time_between_callbacks_ms);
    EXPECT_LE(average_time_between_callbacks_ms,
              1.30 * expected_time_between_callbacks_ms);
  }


 private:
  DISALLOW_COPY_AND_ASSIGN(AudioAndroidInputTest);
};

// Get the default audio input parameters and log the result.
TEST_P(AudioAndroidInputTest, GetDefaultInputStreamParameters) {
  // We don't go through AudioAndroidInputTest::GetInputStreamParameters() here
  // so that we can log the real (non-overridden) values of the effects.
  AudioParameters params = audio_manager()->GetInputStreamParameters(
      AudioManagerBase::kDefaultDeviceId);
  EXPECT_TRUE(params.IsValid());
  VLOG(1) << params;
}

// Get the default audio output parameters and log the result.
TEST_F(AudioAndroidOutputTest, GetDefaultOutputStreamParameters) {
  AudioParameters params = GetDefaultOutputStreamParameters();
  EXPECT_TRUE(params.IsValid());
  VLOG(1) << params;
}

// Check if low-latency output is supported and log the result as output.
TEST_F(AudioAndroidOutputTest, IsAudioLowLatencySupported) {
  AudioManagerAndroid* manager =
      static_cast<AudioManagerAndroid*>(audio_manager());
  bool low_latency = manager->IsAudioLowLatencySupported();
  low_latency ? VLOG(0) << "Low latency output is supported"
              : VLOG(0) << "Low latency output is *not* supported";
}

// Ensure that a default input stream can be created and closed.
TEST_P(AudioAndroidInputTest, CreateAndCloseInputStream) {
  AudioParameters params = GetInputStreamParameters();
  AudioInputStream* ais = audio_manager()->MakeAudioInputStream(
      params, AudioManagerBase::kDefaultDeviceId);
  EXPECT_TRUE(ais);
  ais->Close();
}

// Ensure that a default output stream can be created and closed.
// TODO(henrika): should we also verify that this API changes the audio mode
// to communication mode, and calls RegisterHeadsetReceiver, the first time
// it is called?
TEST_F(AudioAndroidOutputTest, CreateAndCloseOutputStream) {
  AudioParameters params = GetDefaultOutputStreamParameters();
  AudioOutputStream* aos = audio_manager()->MakeAudioOutputStream(
      params, std::string(), std::string());
  EXPECT_TRUE(aos);
  aos->Close();
}

// Ensure that a default input stream can be opened and closed.
TEST_P(AudioAndroidInputTest, OpenAndCloseInputStream) {
  AudioParameters params = GetInputStreamParameters();
  AudioInputStream* ais = audio_manager()->MakeAudioInputStream(
      params, AudioManagerBase::kDefaultDeviceId);
  EXPECT_TRUE(ais);
  EXPECT_TRUE(ais->Open());
  ais->Close();
}

// Ensure that a default output stream can be opened and closed.
TEST_F(AudioAndroidOutputTest, OpenAndCloseOutputStream) {
  AudioParameters params = GetDefaultOutputStreamParameters();
  AudioOutputStream* aos = audio_manager()->MakeAudioOutputStream(
      params, std::string(), std::string());
  EXPECT_TRUE(aos);
  EXPECT_TRUE(aos->Open());
  aos->Close();
}

// Start input streaming using default input parameters and ensure that the
// callback sequence is sane.
TEST_P(AudioAndroidInputTest, StartInputStreamCallbacks) {
  AudioParameters params = GetInputStreamParameters();
  StartInputStreamCallbacks(params);
}

// Start input streaming using non default input parameters and ensure that the
// callback sequence is sane. The only change we make in this test is to select
// a 10ms buffer size instead of the default size.
// TODO(henrika): possibly add support for more variations.
TEST_P(AudioAndroidInputTest, StartInputStreamCallbacksNonDefaultParameters) {
  AudioParameters native_params = GetInputStreamParameters();
  AudioParameters params(native_params.format(),
                         native_params.channel_layout(),
                         native_params.input_channels(),
                         native_params.sample_rate(),
                         native_params.bits_per_sample(),
                         native_params.sample_rate() / 100,
                         native_params.effects());
  StartInputStreamCallbacks(params);
}

// Start output streaming using default output parameters and ensure that the
// callback sequence is sane.
TEST_F(AudioAndroidOutputTest, StartOutputStreamCallbacks) {
  AudioParameters params = GetDefaultOutputStreamParameters();
  StartOutputStreamCallbacks(params);
}

// Start output streaming using non default output parameters and ensure that
// the callback sequence is sane. The only change we make in this test is to
// select a 10ms buffer size instead of the default size and to open up the
// device in mono.
// TODO(henrika): possibly add support for more variations.
TEST_F(AudioAndroidOutputTest, StartOutputStreamCallbacksNonDefaultParameters) {
  AudioParameters native_params = GetDefaultOutputStreamParameters();
  AudioParameters params(native_params.format(),
                         CHANNEL_LAYOUT_MONO,
                         native_params.sample_rate(),
                         native_params.bits_per_sample(),
                         native_params.sample_rate() / 100);
  StartOutputStreamCallbacks(params);
}

// Play out a PCM file segment in real time and allow the user to verify that
// the rendered audio sounds OK.
// NOTE: this test requires user interaction and is not designed to run as an
// automatized test on bots.
TEST_F(AudioAndroidOutputTest, DISABLED_RunOutputStreamWithFileAsSource) {
  AudioParameters params = GetDefaultOutputStreamParameters();
  VLOG(1) << params;
  AudioOutputStream* aos = audio_manager()->MakeAudioOutputStream(
      params, std::string(), std::string());
  EXPECT_TRUE(aos);

  std::string file_name;
  if (params.sample_rate() == 48000 && params.channels() == 2) {
    file_name = kSpeechFile_16b_s_48k;
  } else if (params.sample_rate() == 48000 && params.channels() == 1) {
    file_name = kSpeechFile_16b_m_48k;
  } else if (params.sample_rate() == 44100 && params.channels() == 2) {
    file_name = kSpeechFile_16b_s_44k;
  } else if (params.sample_rate() == 44100 && params.channels() == 1) {
    file_name = kSpeechFile_16b_m_44k;
  } else {
    FAIL() << "This test supports 44.1kHz and 48kHz mono/stereo only.";
    return;
  }

  base::WaitableEvent event(false, false);
  FileAudioSource source(&event, file_name);

  EXPECT_TRUE(aos->Open());
  aos->SetVolume(1.0);
  aos->Start(&source);
  VLOG(0) << ">> Verify that the file is played out correctly...";
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_max_timeout()));
  aos->Stop();
  aos->Close();
}

// Start input streaming and run it for ten seconds while recording to a
// local audio file.
// NOTE: this test requires user interaction and is not designed to run as an
// automatized test on bots.
TEST_P(AudioAndroidInputTest, DISABLED_RunSimplexInputStreamWithFileAsSink) {
  AudioParameters params = GetInputStreamParameters();
  VLOG(1) << params;
  AudioInputStream* ais = audio_manager()->MakeAudioInputStream(
      params, AudioManagerBase::kDefaultDeviceId);
  EXPECT_TRUE(ais);

  std::string file_name = base::StringPrintf("out_simplex_%d_%d_%d.pcm",
                                             params.sample_rate(),
                                             params.frames_per_buffer(),
                                             params.channels());

  base::WaitableEvent event(false, false);
  FileAudioSink sink(&event, params, file_name);

  EXPECT_TRUE(ais->Open());
  ais->Start(&sink);
  VLOG(0) << ">> Speak into the microphone to record audio...";
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_max_timeout()));
  ais->Stop();
  ais->Close();
}

// Same test as RunSimplexInputStreamWithFileAsSink but this time output
// streaming is active as well (reads zeros only).
// NOTE: this test requires user interaction and is not designed to run as an
// automatized test on bots.
TEST_P(AudioAndroidInputTest, DISABLED_RunDuplexInputStreamWithFileAsSink) {
  AudioParameters in_params = GetInputStreamParameters();
  AudioInputStream* ais = audio_manager()->MakeAudioInputStream(
      in_params, AudioManagerBase::kDefaultDeviceId);
  EXPECT_TRUE(ais);

  AudioParameters out_params =
      audio_manager()->GetDefaultOutputStreamParameters();
  AudioOutputStream* aos = audio_manager()->MakeAudioOutputStream(
      out_params, std::string(), std::string());
  EXPECT_TRUE(aos);

  std::string file_name = base::StringPrintf("out_duplex_%d_%d_%d.pcm",
                                             in_params.sample_rate(),
                                             in_params.frames_per_buffer(),
                                             in_params.channels());

  base::WaitableEvent event(false, false);
  FileAudioSink sink(&event, in_params, file_name);
  MockAudioOutputCallback source;

  EXPECT_CALL(source, OnMoreData(NotNull(), _)).WillRepeatedly(
      Invoke(&source, &MockAudioOutputCallback::RealOnMoreData));
  EXPECT_CALL(source, OnError(aos)).Times(0);
  EXPECT_CALL(source, OnMoreIOData(_, _, _)).Times(0);

  EXPECT_TRUE(ais->Open());
  EXPECT_TRUE(aos->Open());
  ais->Start(&sink);
  aos->Start(&source);
  VLOG(0) << ">> Speak into the microphone to record audio";
  EXPECT_TRUE(event.TimedWait(TestTimeouts::action_max_timeout()));
  aos->Stop();
  ais->Stop();
  aos->Close();
  ais->Close();
}

// Start audio in both directions while feeding captured data into a FIFO so
// it can be read directly (in loopback) by the render side. A small extra
// delay will be added by the FIFO and an estimate of this delay will be
// printed out during the test.
// NOTE: this test requires user interaction and is not designed to run as an
// automatized test on bots.
TEST_P(AudioAndroidInputTest,
       DISABLED_RunSymmetricInputAndOutputStreamsInFullDuplex) {
  // Get native audio parameters for the input side.
  AudioParameters default_input_params = GetInputStreamParameters();

  // Modify the parameters so that both input and output can use the same
  // parameters by selecting 10ms as buffer size. This will also ensure that
  // the output stream will be a mono stream since mono is default for input
  // audio on Android.
  AudioParameters io_params(default_input_params.format(),
                            default_input_params.channel_layout(),
                            default_input_params.sample_rate(),
                            default_input_params.bits_per_sample(),
                            default_input_params.sample_rate() / 100);
  VLOG(1) << io_params;

  // Create input and output streams using the common audio parameters.
  AudioInputStream* ais = audio_manager()->MakeAudioInputStream(
      io_params, AudioManagerBase::kDefaultDeviceId);
  EXPECT_TRUE(ais);
  AudioOutputStream* aos = audio_manager()->MakeAudioOutputStream(
      io_params, std::string(), std::string());
  EXPECT_TRUE(aos);

  FullDuplexAudioSinkSource full_duplex(io_params);

  // Start a full duplex audio session and print out estimates of the extra
  // delay we should expect from the FIFO. If real-time delay measurements are
  // performed, the result should be reduced by this extra delay since it is
  // something that has been added by the test.
  EXPECT_TRUE(ais->Open());
  EXPECT_TRUE(aos->Open());
  ais->Start(&full_duplex);
  aos->Start(&full_duplex);
  VLOG(1) << "HINT: an estimate of the extra FIFO delay will be updated "
          << "once per second during this test.";
  VLOG(0) << ">> Speak into the mic and listen to the audio in loopback...";
  fflush(stdout);
  base::PlatformThread::Sleep(base::TimeDelta::FromSeconds(20));
  printf("\n");
  aos->Stop();
  ais->Stop();
  aos->Close();
  ais->Close();
}

INSTANTIATE_TEST_CASE_P(AudioAndroidInputTest, AudioAndroidInputTest,
    testing::ValuesIn(RunAudioRecordInputPathTests()));

}  // namespace media
