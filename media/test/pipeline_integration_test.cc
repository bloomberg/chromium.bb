// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/cdm_callback_promise.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_key_information.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media.h"
#include "media/base/media_keys.h"
#include "media/base/media_switches.h"
#include "media/base/test_data_util.h"
#include "media/cdm/aes_decryptor.h"
#include "media/cdm/json_web_key.h"
#include "media/filters/chunk_demuxer.h"
#include "media/renderers/renderer_impl.h"
#include "media/test/pipeline_integration_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"

#if defined(MOJO_RENDERER)
#include "media/mojo/services/mojo_renderer_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_impl.h"
#include "third_party/mojo/src/mojo/public/cpp/application/application_test_base.h"
#include "third_party/mojo/src/mojo/public/cpp/application/connect.h"

// TODO(dalecurtis): The mojo renderer is in another process, so we have no way
// currently to get hashes for video and audio samples.  This also means that
// real audio plays out for each test.
#define EXPECT_HASH_EQ(a, b)
#define EXPECT_VIDEO_FORMAT_EQ(a, b)

// TODO(xhwang): EME support is not complete for the mojo renderer, so all
// encrypted tests are currently disabled.
#define DISABLE_EME_TESTS 1

// TODO(xhwang,dalecurtis): Text tracks are not currently supported by the mojo
// renderer.
#define DISABLE_TEXT_TRACK_TESTS 1
#else
#define EXPECT_HASH_EQ(a, b) EXPECT_EQ(a, b)
#define EXPECT_VIDEO_FORMAT_EQ(a, b) EXPECT_EQ(a, b)
#endif

using testing::_;
using testing::AnyNumber;
using testing::AtLeast;
using testing::AtMost;
using testing::SaveArg;

namespace media {

const char kSourceId[] = "SourceId";
const char kCencInitDataType[] = "cenc";

const char kWebM[] = "video/webm; codecs=\"vp8,vorbis\"";
const char kWebMVP9[] = "video/webm; codecs=\"vp9\"";
const char kAudioOnlyWebM[] = "video/webm; codecs=\"vorbis\"";
const char kOpusAudioOnlyWebM[] = "video/webm; codecs=\"opus\"";
const char kVideoOnlyWebM[] = "video/webm; codecs=\"vp8\"";
#if defined(USE_PROPRIETARY_CODECS)
const char kADTS[] = "audio/aac";
const char kMP4[] = "video/mp4; codecs=\"avc1.4D4041,mp4a.40.2\"";
const char kMP4VideoAVC3[] = "video/mp4; codecs=\"avc3.64001f\"";
#if !defined(DISABLE_EME_TESTS)
const char kMP4Video[] = "video/mp4; codecs=\"avc1.4D4041\"";
const char kMP4Audio[] = "audio/mp4; codecs=\"mp4a.40.2\"";
#endif  // !defined(DISABLE_EME_TESTS)
const char kMP3[] = "audio/mpeg";
#endif  // defined(USE_PROPRIETARY_CODECS)

// Key used to encrypt test files.
const uint8 kSecretKey[] = {
  0xeb, 0xdd, 0x62, 0xf1, 0x68, 0x14, 0xd2, 0x7b,
  0x68, 0xef, 0x12, 0x2a, 0xfc, 0xe4, 0xae, 0x3c
};

// The key ID for all encrypted files.
const uint8 kKeyId[] = {
  0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35
};

const int kAppendWholeFile = -1;

// Constants for the Media Source config change tests.
const int kAppendTimeSec = 1;
const int kAppendTimeMs = kAppendTimeSec * 1000;
const int k320WebMFileDurationMs = 2736;
const int k640WebMFileDurationMs = 2749;
const int kOpusEndTrimmingWebMFileDurationMs = 2741;
const int kVP9WebMFileDurationMs = 2736;
const int kVP8AWebMFileDurationMs = 2733;

#if defined(USE_PROPRIETARY_CODECS)
#if !defined(DISABLE_EME_TESTS)
const int k640IsoFileDurationMs = 2737;
const int k640IsoCencFileDurationMs = 2736;
#endif  // !defined(DISABLE_EME_TESTS)
const int k1280IsoFileDurationMs = 2736;
const int k1280IsoAVC3FileDurationMs = 2736;
#endif  // defined(USE_PROPRIETARY_CODECS)

// Return a timeline offset for bear-320x240-live.webm.
static base::Time kLiveTimelineOffset() {
  // The file contians the following UTC timeline offset:
  // 2012-11-10 12:34:56.789123456
  // Since base::Time only has a resolution of microseconds,
  // construct a base::Time for 2012-11-10 12:34:56.789123.
  base::Time::Exploded exploded_time;
  exploded_time.year = 2012;
  exploded_time.month = 11;
  exploded_time.day_of_month = 10;
  exploded_time.hour = 12;
  exploded_time.minute = 34;
  exploded_time.second = 56;
  exploded_time.millisecond = 789;
  base::Time timeline_offset = base::Time::FromUTCExploded(exploded_time);

  timeline_offset += base::TimeDelta::FromMicroseconds(123);

  return timeline_offset;
}

// FFmpeg only supports time a resolution of seconds so this
// helper function truncates a base::Time to seconds resolution.
static base::Time TruncateToFFmpegTimeResolution(base::Time t) {
  base::Time::Exploded exploded_time;
  t.UTCExplode(&exploded_time);
  exploded_time.millisecond = 0;

  return base::Time::FromUTCExploded(exploded_time);
}

// Note: Tests using this class only exercise the DecryptingDemuxerStream path.
// They do not exercise the Decrypting{Audio|Video}Decoder path.
class FakeEncryptedMedia {
 public:
  // Defines the behavior of the "app" that responds to EME events.
  class AppBase {
   public:
    virtual ~AppBase() {}

    virtual void OnSessionMessage(const std::string& session_id,
                                  MediaKeys::MessageType message_type,
                                  const std::vector<uint8>& message,
                                  const GURL& legacy_destination_url) = 0;

    virtual void OnSessionClosed(const std::string& session_id) = 0;

    virtual void OnSessionKeysChange(const std::string& session_id,
                                     bool has_additional_usable_key,
                                     CdmKeysInfo keys_info) = 0;

    // Errors are not expected unless overridden.
    virtual void OnSessionError(const std::string& session_id,
                                const std::string& error_name,
                                uint32 system_code,
                                const std::string& error_message) {
      FAIL() << "Unexpected Key Error";
    }

    virtual void OnEncryptedMediaInitData(const std::string& init_data_type,
                                          const std::vector<uint8>& init_data,
                                          AesDecryptor* decryptor) = 0;
  };

  FakeEncryptedMedia(AppBase* app)
      : decryptor_(base::Bind(&FakeEncryptedMedia::OnSessionMessage,
                              base::Unretained(this)),
                   base::Bind(&FakeEncryptedMedia::OnSessionClosed,
                              base::Unretained(this)),
                   base::Bind(&FakeEncryptedMedia::OnSessionKeysChange,
                              base::Unretained(this))),
        cdm_context_(&decryptor_),
        app_(app) {}

  CdmContext* GetCdmContext() { return &cdm_context_; }

  // Callbacks for firing session events. Delegate to |app_|.
  void OnSessionMessage(const std::string& session_id,
                        MediaKeys::MessageType message_type,
                        const std::vector<uint8>& message,
                        const GURL& legacy_destination_url) {
    app_->OnSessionMessage(session_id, message_type, message,
                           legacy_destination_url);
  }

  void OnSessionClosed(const std::string& session_id) {
    app_->OnSessionClosed(session_id);
  }

  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info) {
    app_->OnSessionKeysChange(session_id, has_additional_usable_key,
                              keys_info.Pass());
  }

  void OnSessionError(const std::string& session_id,
                      const std::string& error_name,
                      uint32 system_code,
                      const std::string& error_message) {
    app_->OnSessionError(session_id, error_name, system_code, error_message);
  }

  void OnEncryptedMediaInitData(const std::string& init_data_type,
                                const std::vector<uint8>& init_data) {
    app_->OnEncryptedMediaInitData(init_data_type, init_data, &decryptor_);
  }

 private:
  class TestCdmContext : public CdmContext {
   public:
    TestCdmContext(Decryptor* decryptor) : decryptor_(decryptor) {}

    Decryptor* GetDecryptor() final { return decryptor_; }

#if defined(ENABLE_BROWSER_CDMS)
    int GetCdmId() const final { return kInvalidCdmId; }
#endif

   private:
    Decryptor* decryptor_;
  };

  AesDecryptor decryptor_;
  TestCdmContext cdm_context_;
  scoped_ptr<AppBase> app_;
};

enum PromiseResult { RESOLVED, REJECTED };

// Provides |kSecretKey| in response to needkey.
class KeyProvidingApp : public FakeEncryptedMedia::AppBase {
 public:
  KeyProvidingApp() {}

  void OnResolveWithSession(PromiseResult expected,
                            const std::string& session_id) {
    EXPECT_EQ(expected, RESOLVED);
    EXPECT_GT(session_id.length(), 0ul);
    current_session_id_ = session_id;
  }

  void OnResolve(PromiseResult expected) {
    EXPECT_EQ(expected, RESOLVED);
  }

  void OnReject(PromiseResult expected,
                media::MediaKeys::Exception exception_code,
                uint32 system_code,
                const std::string& error_message) {
    EXPECT_EQ(expected, REJECTED) << error_message;
  }

  scoped_ptr<SimpleCdmPromise> CreatePromise(PromiseResult expected) {
    scoped_ptr<media::SimpleCdmPromise> promise(new media::CdmCallbackPromise<>(
        base::Bind(
            &KeyProvidingApp::OnResolve, base::Unretained(this), expected),
        base::Bind(
            &KeyProvidingApp::OnReject, base::Unretained(this), expected)));
    return promise.Pass();
  }

  scoped_ptr<NewSessionCdmPromise> CreateSessionPromise(
      PromiseResult expected) {
    scoped_ptr<media::NewSessionCdmPromise> promise(
        new media::CdmCallbackPromise<std::string>(
            base::Bind(&KeyProvidingApp::OnResolveWithSession,
                       base::Unretained(this),
                       expected),
            base::Bind(
                &KeyProvidingApp::OnReject, base::Unretained(this), expected)));
    return promise.Pass();
  }

  void OnSessionMessage(const std::string& session_id,
                        MediaKeys::MessageType message_type,
                        const std::vector<uint8>& message,
                        const GURL& legacy_destination_url) override {
    EXPECT_FALSE(session_id.empty());
    EXPECT_FALSE(message.empty());
    EXPECT_EQ(current_session_id_, session_id);
  }

  void OnSessionClosed(const std::string& session_id) override {
    EXPECT_EQ(current_session_id_, session_id);
  }

  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info) override {
    EXPECT_EQ(current_session_id_, session_id);
    EXPECT_EQ(has_additional_usable_key, true);
  }

  void OnEncryptedMediaInitData(const std::string& init_data_type,
                                const std::vector<uint8>& init_data,
                                AesDecryptor* decryptor) override {
    // Since only 1 session is created, skip the request if the |init_data|
    // has been seen before (no need to add the same key again).
    if (init_data == prev_init_data_)
      return;
    prev_init_data_ = init_data;

    if (current_session_id_.empty()) {
      if (init_data_type == kCencInitDataType) {
        // Since the 'cenc' files are not created with proper 'pssh' boxes,
        // simply pretend that this is a webm file and pass the expected
        // key ID as the init_data.
        // http://crbug.com/460308
        decryptor->CreateSessionAndGenerateRequest(
            MediaKeys::TEMPORARY_SESSION, "webm", kKeyId, arraysize(kKeyId),
            CreateSessionPromise(RESOLVED));
      } else {
        decryptor->CreateSessionAndGenerateRequest(
            MediaKeys::TEMPORARY_SESSION, init_data_type,
            vector_as_array(&init_data), init_data.size(),
            CreateSessionPromise(RESOLVED));
      }
      EXPECT_FALSE(current_session_id_.empty());
    }

    // Clear Key really needs the key ID from |init_data|. For WebM, they are
    // the same, but this is not the case for ISO CENC (key ID embedded in a
    // 'pssh' box). Therefore, provide the correct key ID.
    const uint8* key_id = init_data.empty() ? NULL : &init_data[0];
    size_t key_id_length = init_data.size();
    if (init_data_type == kCencInitDataType) {
      key_id = kKeyId;
      key_id_length = arraysize(kKeyId);
    }

    // Convert key into a JSON structure and then add it.
    std::string jwk = GenerateJWKSet(
        kSecretKey, arraysize(kSecretKey), key_id, key_id_length);
    decryptor->UpdateSession(current_session_id_,
                             reinterpret_cast<const uint8*>(jwk.data()),
                             jwk.size(),
                             CreatePromise(RESOLVED));
  }

  std::string current_session_id_;
  std::vector<uint8> prev_init_data_;
};

class RotatingKeyProvidingApp : public KeyProvidingApp {
 public:
  RotatingKeyProvidingApp() : num_distint_need_key_calls_(0) {}
  ~RotatingKeyProvidingApp() override {
    // Expect that OnEncryptedMediaInitData is fired multiple times with
    // different |init_data|.
    EXPECT_GT(num_distint_need_key_calls_, 1u);
  }

  void OnEncryptedMediaInitData(const std::string& init_data_type,
                                const std::vector<uint8>& init_data,
                                AesDecryptor* decryptor) override {
    // Skip the request if the |init_data| has been seen.
    if (init_data == prev_init_data_)
      return;
    prev_init_data_ = init_data;
    ++num_distint_need_key_calls_;

    std::vector<uint8> key_id;
    std::vector<uint8> key;
    EXPECT_TRUE(GetKeyAndKeyId(init_data, &key, &key_id));

    if (init_data_type == kCencInitDataType) {
      // Since the 'cenc' files are not created with proper 'pssh' boxes,
      // simply pretend that this is a webm file and pass the expected
      // key ID as the init_data.
      // http://crbug.com/460308
      decryptor->CreateSessionAndGenerateRequest(
          MediaKeys::TEMPORARY_SESSION, "webm", vector_as_array(&key_id),
          key_id.size(), CreateSessionPromise(RESOLVED));
    } else {
      decryptor->CreateSessionAndGenerateRequest(
          MediaKeys::TEMPORARY_SESSION, init_data_type,
          vector_as_array(&init_data), init_data.size(),
          CreateSessionPromise(RESOLVED));
    }

    // Convert key into a JSON structure and then add it.
    std::string jwk = GenerateJWKSet(vector_as_array(&key),
                                     key.size(),
                                     vector_as_array(&key_id),
                                     key_id.size());
    decryptor->UpdateSession(current_session_id_,
                             reinterpret_cast<const uint8*>(jwk.data()),
                             jwk.size(),
                             CreatePromise(RESOLVED));
  }

 private:
  bool GetKeyAndKeyId(std::vector<uint8> init_data,
                      std::vector<uint8>* key,
                      std::vector<uint8>* key_id) {
    // For WebM, init_data is key_id; for ISO CENC, init_data should contain
    // the key_id. We assume key_id is in the end of init_data here (that is
    // only a reasonable assumption for WebM and clear key ISO CENC).
    DCHECK_GE(init_data.size(), arraysize(kKeyId));
    std::vector<uint8> key_id_from_init_data(
        init_data.end() - arraysize(kKeyId), init_data.end());

    key->assign(kSecretKey, kSecretKey + arraysize(kSecretKey));
    key_id->assign(kKeyId, kKeyId + arraysize(kKeyId));

    // The Key and KeyId for this testing key provider are created by left
    // rotating kSecretKey and kKeyId. Note that this implementation is only
    // intended for testing purpose. The actual key rotation algorithm can be
    // much more complicated.
    // Find out the rotating position from |key_id_from_init_data| and apply on
    // |key|.
    for (size_t pos = 0; pos < arraysize(kKeyId); ++pos) {
      std::rotate(key_id->begin(), key_id->begin() + pos, key_id->end());
      if (*key_id == key_id_from_init_data) {
        std::rotate(key->begin(), key->begin() + pos, key->end());
        return true;
      }
    }
    return false;
  }

  std::vector<uint8> prev_init_data_;
  uint32 num_distint_need_key_calls_;
};

// Ignores needkey and does not perform a license request
class NoResponseApp : public FakeEncryptedMedia::AppBase {
 public:
  void OnSessionMessage(const std::string& session_id,
                        MediaKeys::MessageType message_type,
                        const std::vector<uint8>& message,
                        const GURL& legacy_destination_url) override {
    EXPECT_FALSE(session_id.empty());
    EXPECT_FALSE(message.empty());
    FAIL() << "Unexpected Message";
  }

  void OnSessionClosed(const std::string& session_id) override {
    EXPECT_FALSE(session_id.empty());
    FAIL() << "Unexpected Closed";
  }

  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info) override {
    EXPECT_FALSE(session_id.empty());
    EXPECT_EQ(has_additional_usable_key, true);
  }

  void OnEncryptedMediaInitData(const std::string& init_data_type,
                                const std::vector<uint8>& init_data,
                                AesDecryptor* decryptor) override {}
};

// Helper class that emulates calls made on the ChunkDemuxer by the
// Media Source API.
class MockMediaSource {
 public:
  MockMediaSource(const std::string& filename,
                  const std::string& mimetype,
                  int initial_append_size)
      : current_position_(0),
        initial_append_size_(initial_append_size),
        mimetype_(mimetype),
        chunk_demuxer_(new ChunkDemuxer(
            base::Bind(&MockMediaSource::DemuxerOpened, base::Unretained(this)),
            base::Bind(&MockMediaSource::OnEncryptedMediaInitData,
                       base::Unretained(this)),
            LogCB(),
            scoped_refptr<MediaLog>(new MediaLog()),
            true)),
        owned_chunk_demuxer_(chunk_demuxer_) {
    file_data_ = ReadTestDataFile(filename);

    if (initial_append_size_ == kAppendWholeFile)
      initial_append_size_ = file_data_->data_size();

    DCHECK_GT(initial_append_size_, 0);
    DCHECK_LE(initial_append_size_, file_data_->data_size());
  }

  virtual ~MockMediaSource() {}

  scoped_ptr<Demuxer> GetDemuxer() { return owned_chunk_demuxer_.Pass(); }

  void set_encrypted_media_init_data_cb(
      const Demuxer::EncryptedMediaInitDataCB& encrypted_media_init_data_cb) {
    encrypted_media_init_data_cb_ = encrypted_media_init_data_cb;
  }

  void Seek(base::TimeDelta seek_time, int new_position, int seek_append_size) {
    chunk_demuxer_->StartWaitingForSeek(seek_time);

    chunk_demuxer_->Abort(
        kSourceId,
        base::TimeDelta(), kInfiniteDuration(), &last_timestamp_offset_);

    DCHECK_GE(new_position, 0);
    DCHECK_LT(new_position, file_data_->data_size());
    current_position_ = new_position;

    AppendData(seek_append_size);
  }

  void AppendData(int size) {
    DCHECK(chunk_demuxer_);
    DCHECK_LT(current_position_, file_data_->data_size());
    DCHECK_LE(current_position_ + size, file_data_->data_size());

    chunk_demuxer_->AppendData(
        kSourceId, file_data_->data() + current_position_, size,
        base::TimeDelta(), kInfiniteDuration(), &last_timestamp_offset_,
        base::Bind(&MockMediaSource::InitSegmentReceived,
                   base::Unretained(this)));
    current_position_ += size;
  }

  void AppendAtTime(base::TimeDelta timestamp_offset,
                    const uint8* pData,
                    int size) {
    CHECK(!chunk_demuxer_->IsParsingMediaSegment(kSourceId));
    chunk_demuxer_->AppendData(kSourceId, pData, size,
                               base::TimeDelta(), kInfiniteDuration(),
                               &timestamp_offset,
                               base::Bind(&MockMediaSource::InitSegmentReceived,
                                          base::Unretained(this)));
    last_timestamp_offset_ = timestamp_offset;
  }

  void AppendAtTimeWithWindow(base::TimeDelta timestamp_offset,
                              base::TimeDelta append_window_start,
                              base::TimeDelta append_window_end,
                              const uint8* pData,
                              int size) {
    CHECK(!chunk_demuxer_->IsParsingMediaSegment(kSourceId));
    chunk_demuxer_->AppendData(kSourceId,
                               pData,
                               size,
                               append_window_start,
                               append_window_end,
                               &timestamp_offset,
                               base::Bind(&MockMediaSource::InitSegmentReceived,
                                          base::Unretained(this)));
    last_timestamp_offset_ = timestamp_offset;
  }

  void EndOfStream() {
    chunk_demuxer_->MarkEndOfStream(PIPELINE_OK);
  }

  void Abort() {
    if (!chunk_demuxer_)
      return;
    chunk_demuxer_->Shutdown();
    chunk_demuxer_ = NULL;
  }

  void DemuxerOpened() {
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&MockMediaSource::DemuxerOpenedTask,
                              base::Unretained(this)));
  }

  void DemuxerOpenedTask() {
    // This code assumes that |mimetype_| is one of the following forms.
    // 1. audio/mpeg
    // 2. video/webm;codec="vorbis,vp8".
    size_t semicolon = mimetype_.find(";");
    std::string type = mimetype_;
    std::vector<std::string> codecs;
    if (semicolon != std::string::npos) {
      type = mimetype_.substr(0, semicolon);
      size_t codecs_param_start = mimetype_.find("codecs=\"", semicolon);

      CHECK_NE(codecs_param_start, std::string::npos);

      codecs_param_start += 8; // Skip over the codecs=".

      size_t codecs_param_end = mimetype_.find("\"", codecs_param_start);

      CHECK_NE(codecs_param_end, std::string::npos);

      std::string codecs_param =
          mimetype_.substr(codecs_param_start,
                           codecs_param_end - codecs_param_start);
      Tokenize(codecs_param, ",", &codecs);
    }

    CHECK_EQ(chunk_demuxer_->AddId(kSourceId, type, codecs), ChunkDemuxer::kOk);

    AppendData(initial_append_size_);
  }

  void OnEncryptedMediaInitData(const std::string& init_data_type,
                                const std::vector<uint8>& init_data) {
    DCHECK(!init_data.empty());
    CHECK(!encrypted_media_init_data_cb_.is_null());
    encrypted_media_init_data_cb_.Run(init_data_type, init_data);
  }

  base::TimeDelta last_timestamp_offset() const {
    return last_timestamp_offset_;
  }

  MOCK_METHOD0(InitSegmentReceived, void(void));

 private:
  scoped_refptr<DecoderBuffer> file_data_;
  int current_position_;
  int initial_append_size_;
  std::string mimetype_;
  ChunkDemuxer* chunk_demuxer_;
  scoped_ptr<Demuxer> owned_chunk_demuxer_;
  Demuxer::EncryptedMediaInitDataCB encrypted_media_init_data_cb_;
  base::TimeDelta last_timestamp_offset_;
};

#if defined(MOJO_RENDERER)
class PipelineIntegrationTestHost : public mojo::test::ApplicationTestBase,
                                    public PipelineIntegrationTestBase {
 public:
  bool ShouldCreateDefaultRunLoop() override { return false; }

  void SetUp() override {
    ApplicationTestBase::SetUp();

    // TODO(dalecurtis): For some reason this isn't done...
    if (!base::CommandLine::InitializedForCurrentProcess()) {
      base::CommandLine::Init(0, NULL);
      InitializeMediaLibraryForTesting();
    }
  }

 protected:
  scoped_ptr<Renderer> CreateRenderer() override {
    mojo::ServiceProvider* service_provider =
        application_impl()
            ->ConnectToApplication("mojo://media")
            ->GetServiceProvider();

    mojo::MediaRendererPtr mojo_media_renderer;
    mojo::ConnectToService(service_provider, &mojo_media_renderer);
    return make_scoped_ptr(new MojoRendererImpl(message_loop_.task_runner(),
                                                mojo_media_renderer.Pass()));
  }
};
#else
class PipelineIntegrationTestHost : public testing::Test,
                                    public PipelineIntegrationTestBase {};
#endif

class PipelineIntegrationTest : public PipelineIntegrationTestHost {
 public:
  void StartPipelineWithMediaSource(MockMediaSource* source) {
    EXPECT_CALL(*source, InitSegmentReceived()).Times(AtLeast(1));
    EXPECT_CALL(*this, OnMetadata(_))
        .Times(AtMost(1))
        .WillRepeatedly(SaveArg<0>(&metadata_));
    EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH))
        .Times(AtMost(1));

    // Encrypted content not used, so this is never called.
    EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);

    demuxer_ = source->GetDemuxer().Pass();
    pipeline_->Start(
        demuxer_.get(), CreateRenderer(),
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnStatusCallback,
                   base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnMetadata,
                   base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnBufferingStateChanged,
                   base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnVideoFramePaint,
                   base::Unretained(this)),
        base::Closure(), base::Bind(&PipelineIntegrationTest::OnAddTextTrack,
                                    base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnWaitingForDecryptionKey,
                   base::Unretained(this)));
    message_loop_.Run();
    EXPECT_EQ(PIPELINE_OK, pipeline_status_);
  }

  void StartHashedPipelineWithMediaSource(MockMediaSource* source) {
    hashing_enabled_ = true;
    StartPipelineWithMediaSource(source);
  }

  void StartPipelineWithEncryptedMedia(
      MockMediaSource* source,
      FakeEncryptedMedia* encrypted_media) {
    EXPECT_CALL(*source, InitSegmentReceived()).Times(AtLeast(1));
    EXPECT_CALL(*this, OnMetadata(_))
        .Times(AtMost(1))
        .WillRepeatedly(SaveArg<0>(&metadata_));
    EXPECT_CALL(*this, OnBufferingStateChanged(BUFFERING_HAVE_ENOUGH))
        .Times(AtMost(1));
    EXPECT_CALL(*this, DecryptorAttached(true));

    // Encrypted content used but keys provided in advance, so this is
    // never called.
    EXPECT_CALL(*this, OnWaitingForDecryptionKey()).Times(0);

    demuxer_ = source->GetDemuxer().Pass();

    pipeline_->SetCdm(encrypted_media->GetCdmContext(),
                      base::Bind(&PipelineIntegrationTest::DecryptorAttached,
                                 base::Unretained(this)));

    pipeline_->Start(
        demuxer_.get(), CreateRenderer(),
        base::Bind(&PipelineIntegrationTest::OnEnded, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnError, base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnStatusCallback,
                   base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnMetadata,
                   base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnBufferingStateChanged,
                   base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnVideoFramePaint,
                   base::Unretained(this)),
        base::Closure(), base::Bind(&PipelineIntegrationTest::OnAddTextTrack,
                                    base::Unretained(this)),
        base::Bind(&PipelineIntegrationTest::OnWaitingForDecryptionKey,
                   base::Unretained(this)));

    source->set_encrypted_media_init_data_cb(
        base::Bind(&FakeEncryptedMedia::OnEncryptedMediaInitData,
                   base::Unretained(encrypted_media)));

    message_loop_.Run();
    EXPECT_EQ(PIPELINE_OK, pipeline_status_);
  }

  // Verifies that seeking works properly for ChunkDemuxer when the
  // seek happens while there is a pending read on the ChunkDemuxer
  // and no data is available.
  bool TestSeekDuringRead(const std::string& filename,
                          const std::string& mimetype,
                          int initial_append_size,
                          base::TimeDelta start_seek_time,
                          base::TimeDelta seek_time,
                          int seek_file_position,
                          int seek_append_size) {
    MockMediaSource source(filename, mimetype, initial_append_size);
    StartPipelineWithMediaSource(&source);

    if (pipeline_status_ != PIPELINE_OK)
      return false;

    Play();
    if (!WaitUntilCurrentTimeIsAfter(start_seek_time))
      return false;

    source.Seek(seek_time, seek_file_position, seek_append_size);
    if (!Seek(seek_time))
      return false;

    source.EndOfStream();

    source.Abort();
    Stop();
    return true;
  }
};

TEST_F(PipelineIntegrationTest, BasicPlayback) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240.webm"));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, BasicPlaybackOpusOgg) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-opus.ogg"));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, BasicPlaybackHashed) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240.webm", kHashed));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

  EXPECT_HASH_EQ("f0be120a90a811506777c99a2cdf7cc1", GetVideoHash());
  EXPECT_HASH_EQ("-3.59,-2.06,-0.43,2.15,0.77,-0.95,", GetAudioHash());
  EXPECT_TRUE(demuxer_->GetTimelineOffset().is_null());
}

TEST_F(PipelineIntegrationTest, BasicPlaybackLive) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240-live.webm", kHashed));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

  EXPECT_HASH_EQ("f0be120a90a811506777c99a2cdf7cc1", GetVideoHash());
  EXPECT_HASH_EQ("-3.59,-2.06,-0.43,2.15,0.77,-0.95,", GetAudioHash());

  // TODO: Fix FFmpeg code to return higher resolution time values so
  // we don't have to truncate our expectations here.
  EXPECT_EQ(TruncateToFFmpegTimeResolution(kLiveTimelineOffset()),
            demuxer_->GetTimelineOffset());
}

TEST_F(PipelineIntegrationTest, F32PlaybackHashed) {
  ASSERT_EQ(PIPELINE_OK, Start("sfx_f32le.wav", kHashed));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  EXPECT_HASH_EQ(std::string(kNullVideoHash), GetVideoHash());
  EXPECT_HASH_EQ("3.03,2.86,2.99,3.31,3.57,4.06,", GetAudioHash());
}

#if !defined(DISABLE_EME_TESTS)
TEST_F(PipelineIntegrationTest, BasicPlaybackEncrypted) {
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  set_encrypted_media_init_data_cb(
      base::Bind(&FakeEncryptedMedia::OnEncryptedMediaInitData,
                 base::Unretained(&encrypted_media)));

  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240-av_enc-av.webm",
                               encrypted_media.GetCdmContext()));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  Stop();
}
#endif  // !defined(DISABLE_EME_TESTS)

TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource) {
  MockMediaSource source("bear-320x240.webm", kWebM, 219229);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(k320WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

  EXPECT_TRUE(demuxer_->GetTimelineOffset().is_null());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource_Live) {
  MockMediaSource source("bear-320x240-live.webm", kWebM, 219221);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(k320WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

  EXPECT_EQ(kLiveTimelineOffset(),
            demuxer_->GetTimelineOffset());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource_VP9_WebM) {
  MockMediaSource source("bear-vp9.webm", kWebMVP9, 67504);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kVP9WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource_VP8A_WebM) {
  MockMediaSource source("bear-vp8a.webm", kVideoOnlyWebM, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kVP8AWebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource_Opus_WebM) {
  MockMediaSource source("bear-opus-end-trimming.webm", kOpusAudioOnlyWebM,
                         kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kOpusEndTrimmingWebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());
  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

// Flaky. http://crbug.com/304776
TEST_F(PipelineIntegrationTest, DISABLED_MediaSource_Opus_Seeking_WebM) {
  MockMediaSource source("bear-opus-end-trimming.webm", kOpusAudioOnlyWebM,
                         kAppendWholeFile);
  StartHashedPipelineWithMediaSource(&source);

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kOpusEndTrimmingWebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  base::TimeDelta start_seek_time = base::TimeDelta::FromMilliseconds(1000);
  base::TimeDelta seek_time = base::TimeDelta::FromMilliseconds(2000);

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(start_seek_time));
  source.Seek(seek_time, 0x1D5, 34017);
  source.EndOfStream();
  ASSERT_TRUE(Seek(seek_time));

  ASSERT_TRUE(WaitUntilOnEnded());

  EXPECT_HASH_EQ("0.76,0.20,-0.82,-0.58,-1.29,-0.29,", GetAudioHash());

  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, MediaSource_ConfigChange_WebM) {
  MockMediaSource source("bear-320x240-16x9-aspect.webm", kWebM,
                         kAppendWholeFile);
  StartPipelineWithMediaSource(&source);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-640x360.webm");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kAppendTimeMs + k640WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

#if !defined(DISABLE_EME_TESTS)
TEST_F(PipelineIntegrationTest, MediaSource_ConfigChange_Encrypted_WebM) {
  MockMediaSource source("bear-320x240-16x9-aspect-av_enc-av.webm", kWebM,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-640x360-av_enc-av.webm");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kAppendTimeMs + k640WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

// Config changes from encrypted to clear are not currently supported.
TEST_F(PipelineIntegrationTest,
       MediaSource_ConfigChange_ClearThenEncrypted_WebM) {
  MockMediaSource source("bear-320x240-16x9-aspect.webm", kWebM,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-640x360-av_enc-av.webm");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  message_loop_.Run();
  EXPECT_EQ(PIPELINE_ERROR_DECODE, pipeline_status_);

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  // The second video was not added, so its time has not been added.
  EXPECT_EQ(k320WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_EQ(PIPELINE_ERROR_DECODE, WaitUntilEndedOrError());
  source.Abort();
}

// Config changes from clear to encrypted are not currently supported.
TEST_F(PipelineIntegrationTest,
       MediaSource_ConfigChange_EncryptedThenClear_WebM) {
  MockMediaSource source("bear-320x240-16x9-aspect-av_enc-av.webm", kWebM,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-640x360.webm");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  // The second video was not added, so its time has not been added.
  EXPECT_EQ(k320WebMFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_EQ(PIPELINE_ERROR_DECODE, WaitUntilEndedOrError());
  source.Abort();
}
#endif  // !defined(DISABLE_EME_TESTS)

#if defined(USE_PROPRIETARY_CODECS)
TEST_F(PipelineIntegrationTest, MediaSource_ADTS) {
  MockMediaSource source("sfx.adts", kADTS, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(325, pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, MediaSource_ADTS_TimestampOffset) {
  MockMediaSource source("sfx.adts", kADTS, kAppendWholeFile);
  StartHashedPipelineWithMediaSource(&source);
  EXPECT_EQ(325, source.last_timestamp_offset().InMilliseconds());

  // Trim off multiple frames off the beginning of the segment which will cause
  // the first decoded frame to be incorrect if preroll isn't implemented.
  const base::TimeDelta adts_preroll_duration =
      base::TimeDelta::FromSecondsD(2.5 * 1024 / 44100);
  const base::TimeDelta append_time =
      source.last_timestamp_offset() - adts_preroll_duration;

  scoped_refptr<DecoderBuffer> second_file = ReadTestDataFile("sfx.adts");
  source.AppendAtTimeWithWindow(append_time,
                                append_time + adts_preroll_duration,
                                kInfiniteDuration(),
                                second_file->data(),
                                second_file->data_size());
  source.EndOfStream();

  EXPECT_EQ(592, source.last_timestamp_offset().InMilliseconds());
  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(592, pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());

  // Verify preroll is stripped.
  EXPECT_HASH_EQ("-0.06,0.97,-0.90,-0.70,-0.53,-0.34,", GetAudioHash());
}

TEST_F(PipelineIntegrationTest, BasicPlaybackHashed_MP3) {
  ASSERT_EQ(PIPELINE_OK, Start("sfx.mp3", kHashed));

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());

  // Verify codec delay and preroll are stripped.
  EXPECT_HASH_EQ("1.30,2.72,4.56,5.08,3.74,2.03,", GetAudioHash());
}

TEST_F(PipelineIntegrationTest, MediaSource_MP3) {
  MockMediaSource source("sfx.mp3", kMP3, kAppendWholeFile);
  StartHashedPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(313, pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());

  // Verify that codec delay was stripped.
  EXPECT_HASH_EQ("1.01,2.71,4.18,4.32,3.04,1.12,", GetAudioHash());
}

TEST_F(PipelineIntegrationTest, MediaSource_MP3_TimestampOffset) {
  MockMediaSource source("sfx.mp3", kMP3, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  EXPECT_EQ(313, source.last_timestamp_offset().InMilliseconds());

  // There are 576 silent frames at the start of this mp3.  The second append
  // should trim them off.
  const base::TimeDelta mp3_preroll_duration =
      base::TimeDelta::FromSecondsD(576.0 / 44100);
  const base::TimeDelta append_time =
      source.last_timestamp_offset() - mp3_preroll_duration;

  scoped_refptr<DecoderBuffer> second_file = ReadTestDataFile("sfx.mp3");
  source.AppendAtTimeWithWindow(append_time,
                                append_time + mp3_preroll_duration,
                                kInfiniteDuration(),
                                second_file->data(),
                                second_file->data_size());
  source.EndOfStream();

  EXPECT_EQ(613, source.last_timestamp_offset().InMilliseconds());
  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(613, pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, MediaSource_MP3_Icecast) {
  MockMediaSource source("icy_sfx.mp3", kMP3, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, MediaSource_ConfigChange_MP4) {
  MockMediaSource source("bear-640x360-av_frag.mp4", kMP4, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kAppendTimeMs + k1280IsoFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

#if !defined(DISABLE_EME_TESTS)
TEST_F(PipelineIntegrationTest,
       MediaSource_ConfigChange_Encrypted_MP4_CENC_VideoOnly) {
  MockMediaSource source("bear-640x360-v_frag-cenc.mp4", kMP4Video,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-1280x720-v_frag-cenc.mp4");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kAppendTimeMs + k1280IsoFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest,
       MediaSource_ConfigChange_Encrypted_MP4_CENC_KeyRotation_VideoOnly) {
  MockMediaSource source("bear-640x360-v_frag-cenc-key_rotation.mp4", kMP4Video,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new RotatingKeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-1280x720-v_frag-cenc-key_rotation.mp4");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(kAppendTimeMs + k1280IsoFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

// Config changes from clear to encrypted are not currently supported.
// TODO(ddorwin): Figure out why this CHECKs in AppendAtTime().
TEST_F(PipelineIntegrationTest,
       DISABLED_MediaSource_ConfigChange_ClearThenEncrypted_MP4_CENC) {
  MockMediaSource source("bear-640x360-av_frag.mp4", kMP4Video,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-1280x720-v_frag-cenc.mp4");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  message_loop_.Run();
  EXPECT_EQ(PIPELINE_ERROR_DECODE, pipeline_status_);

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  // The second video was not added, so its time has not been added.
  EXPECT_EQ(k640IsoFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_EQ(PIPELINE_ERROR_DECODE, WaitUntilEndedOrError());
  source.Abort();
}

// Config changes from encrypted to clear are not currently supported.
TEST_F(PipelineIntegrationTest,
       MediaSource_ConfigChange_EncryptedThenClear_MP4_CENC) {
  MockMediaSource source("bear-640x360-v_frag-cenc.mp4", kMP4Video,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  scoped_refptr<DecoderBuffer> second_file =
      ReadTestDataFile("bear-1280x720-av_frag.mp4");

  source.AppendAtTime(base::TimeDelta::FromSeconds(kAppendTimeSec),
                      second_file->data(), second_file->data_size());

  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  // The second video was not added, so its time has not been added.
  EXPECT_EQ(k640IsoCencFileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  EXPECT_EQ(PIPELINE_ERROR_DECODE, WaitUntilEndedOrError());
  source.Abort();
}
#endif  // !defined(DISABLE_EME_TESTS)

// Verify files which change configuration midstream fail gracefully.
TEST_F(PipelineIntegrationTest, MidStreamConfigChangesFail) {
  ASSERT_EQ(PIPELINE_OK, Start("midstream_config_change.mp3"));
  Play();
  ASSERT_EQ(WaitUntilEndedOrError(), PIPELINE_ERROR_DECODE);
}

#endif

TEST_F(PipelineIntegrationTest, BasicPlayback_16x9AspectRatio) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240-16x9-aspect.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

#if !defined(DISABLE_EME_TESTS)
TEST_F(PipelineIntegrationTest, EncryptedPlayback_WebM) {
  MockMediaSource source("bear-320x240-av_enc-av.webm", kWebM, 219816);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback_ClearStart_WebM) {
  MockMediaSource source("bear-320x240-av_enc-av_clear-1s.webm", kWebM,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback_NoEncryptedFrames_WebM) {
  MockMediaSource source("bear-320x240-av_enc-av_clear-all.webm", kWebM,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new NoResponseApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}
#endif  // !defined(DISABLE_EME_TESTS)

#if defined(USE_PROPRIETARY_CODECS)
#if !defined(DISABLE_EME_TESTS)
TEST_F(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_VideoOnly) {
  MockMediaSource source("bear-1280x720-v_frag-cenc.mp4", kMP4Video,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_AudioOnly) {
  MockMediaSource source("bear-1280x720-a_frag-cenc.mp4", kMP4Audio,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new KeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest,
       EncryptedPlayback_NoEncryptedFrames_MP4_CENC_VideoOnly) {
  MockMediaSource source("bear-1280x720-v_frag-cenc_clear-all.mp4", kMP4Video,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new NoResponseApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest,
       EncryptedPlayback_NoEncryptedFrames_MP4_CENC_AudioOnly) {
  MockMediaSource source("bear-1280x720-a_frag-cenc_clear-all.mp4", kMP4Audio,
                         kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new NoResponseApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Video) {
  MockMediaSource source("bear-1280x720-v_frag-cenc-key_rotation.mp4",
                         kMP4Video, kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new RotatingKeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}

TEST_F(PipelineIntegrationTest, EncryptedPlayback_MP4_CENC_KeyRotation_Audio) {
  MockMediaSource source("bear-1280x720-a_frag-cenc-key_rotation.mp4",
                         kMP4Audio, kAppendWholeFile);
  FakeEncryptedMedia encrypted_media(new RotatingKeyProvidingApp());
  StartPipelineWithEncryptedMedia(&source, &encrypted_media);

  source.EndOfStream();
  ASSERT_EQ(PIPELINE_OK, pipeline_status_);

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}
#endif  // !defined(DISABLE_EME_TESTS)

TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource_VideoOnly_MP4_AVC3) {
  MockMediaSource source("bear-1280x720-v_frag-avc3.mp4", kMP4VideoAVC3,
                         kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();

  EXPECT_EQ(1u, pipeline_->GetBufferedTimeRanges().size());
  EXPECT_EQ(0, pipeline_->GetBufferedTimeRanges().start(0).InMilliseconds());
  EXPECT_EQ(k1280IsoAVC3FileDurationMs,
            pipeline_->GetBufferedTimeRanges().end(0).InMilliseconds());

  Play();

  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
}
#endif  // defined(USE_PROPRIETARY_CODECS)

TEST_F(PipelineIntegrationTest, SeekWhilePaused) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240.webm"));

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(start_seek_time));
  Pause();
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_EQ(seek_time, pipeline_->GetMediaTime());
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  Pause();
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_EQ(seek_time, pipeline_->GetMediaTime());
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, SeekWhilePlaying) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240.webm"));

  base::TimeDelta duration(pipeline_->GetMediaDuration());
  base::TimeDelta start_seek_time(duration / 4);
  base::TimeDelta seek_time(duration * 3 / 4);

  Play();
  ASSERT_TRUE(WaitUntilCurrentTimeIsAfter(start_seek_time));
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetMediaTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());

  // Make sure seeking after reaching the end works as expected.
  ASSERT_TRUE(Seek(seek_time));
  EXPECT_GE(pipeline_->GetMediaTime(), seek_time);
  ASSERT_TRUE(WaitUntilOnEnded());
}

#if defined(USE_PROPRIETARY_CODECS)
TEST_F(PipelineIntegrationTest, Rotated_Metadata_0) {
  ASSERT_EQ(PIPELINE_OK, Start("bear_rotate_0.mp4"));
  ASSERT_EQ(VIDEO_ROTATION_0, metadata_.video_rotation);
}

TEST_F(PipelineIntegrationTest, Rotated_Metadata_90) {
  ASSERT_EQ(PIPELINE_OK, Start("bear_rotate_90.mp4"));
  ASSERT_EQ(VIDEO_ROTATION_90, metadata_.video_rotation);
}

TEST_F(PipelineIntegrationTest, Rotated_Metadata_180) {
  ASSERT_EQ(PIPELINE_OK, Start("bear_rotate_180.mp4"));
  ASSERT_EQ(VIDEO_ROTATION_180, metadata_.video_rotation);
}

TEST_F(PipelineIntegrationTest, Rotated_Metadata_270) {
  ASSERT_EQ(PIPELINE_OK, Start("bear_rotate_270.mp4"));
  ASSERT_EQ(VIDEO_ROTATION_270, metadata_.video_rotation);
}
#endif

// Verify audio decoder & renderer can handle aborted demuxer reads.
TEST_F(PipelineIntegrationTest, ChunkDemuxerAbortRead_AudioOnly) {
  ASSERT_TRUE(TestSeekDuringRead("bear-320x240-audio-only.webm", kAudioOnlyWebM,
                                 16384,
                                 base::TimeDelta::FromMilliseconds(464),
                                 base::TimeDelta::FromMilliseconds(617),
                                 0x10CA, 19730));
}

// Verify video decoder & renderer can handle aborted demuxer reads.
TEST_F(PipelineIntegrationTest, ChunkDemuxerAbortRead_VideoOnly) {
  ASSERT_TRUE(TestSeekDuringRead("bear-320x240-video-only.webm", kVideoOnlyWebM,
                                 32768,
                                 base::TimeDelta::FromMilliseconds(167),
                                 base::TimeDelta::FromMilliseconds(1668),
                                 0x1C896, 65536));
}

// Verify that Opus audio in WebM containers can be played back.
TEST_F(PipelineIntegrationTest, BasicPlayback_AudioOnly_Opus_WebM) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-opus-end-trimming.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Verify that VP9 video in WebM containers can be played back.
TEST_F(PipelineIntegrationTest, BasicPlayback_VideoOnly_VP9_WebM) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-vp9.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Verify that VP9 video and Opus audio in the same WebM container can be played
// back.
TEST_F(PipelineIntegrationTest, BasicPlayback_VP9_Opus_WebM) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-vp9-opus.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Verify that VP8 video with alpha channel can be played back.
TEST_F(PipelineIntegrationTest, BasicPlayback_VP8A_WebM) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-vp8a.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  EXPECT_VIDEO_FORMAT_EQ(last_video_frame_format_, VideoFrame::YV12A);
}

// Verify that VP8A video with odd width/height can be played back.
TEST_F(PipelineIntegrationTest, BasicPlayback_VP8A_Odd_WebM) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-vp8a-odd-dimensions.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  EXPECT_VIDEO_FORMAT_EQ(last_video_frame_format_, VideoFrame::YV12A);
}

// Verify that VP9 video with odd width/height can be played back.
TEST_F(PipelineIntegrationTest, BasicPlayback_VP9_Odd_WebM) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-vp9-odd-dimensions.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

#if !defined(DISABLE_TEXT_TRACK_TESTS)
// Verify that VP8 video with inband text track can be played back.
TEST_F(PipelineIntegrationTest, BasicPlayback_VP8_WebVTT_WebM) {
  EXPECT_CALL(*this, OnAddTextTrack(_, _));
  ASSERT_EQ(PIPELINE_OK, Start("bear-vp8-webvtt.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}
#endif  // !defined(DISABLE_TEXT_TRACK_TESTS)

// Verify that VP9 video with 4:4:4 subsampling can be played back.
TEST_F(PipelineIntegrationTest, P444_VP9_WebM) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-320x240-P444.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  EXPECT_VIDEO_FORMAT_EQ(last_video_frame_format_, VideoFrame::YV24);
}

// Verify that frames of VP9 video in the BT.709 color space have the YV12HD
// format.
TEST_F(PipelineIntegrationTest, BT709_VP9_WebM) {
  ASSERT_EQ(PIPELINE_OK, Start("bear-vp9-bt709.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  EXPECT_VIDEO_FORMAT_EQ(last_video_frame_format_, VideoFrame::YV12HD);
}

// Verify that videos with an odd frame size playback successfully.
TEST_F(PipelineIntegrationTest, BasicPlayback_OddVideoSize) {
  ASSERT_EQ(PIPELINE_OK, Start("butterfly-853x480.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Verify that OPUS audio in a webm which reports a 44.1kHz sample rate plays
// correctly at 48kHz
TEST_F(PipelineIntegrationTest, BasicPlayback_Opus441kHz) {
  ASSERT_EQ(PIPELINE_OK, Start("sfx-opus-441.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  EXPECT_EQ(48000,
            demuxer_->GetStream(DemuxerStream::AUDIO)
                ->audio_decoder_config()
                .samples_per_second());
}

// Same as above but using MediaSource.
TEST_F(PipelineIntegrationTest, BasicPlayback_MediaSource_Opus441kHz) {
  MockMediaSource source(
      "sfx-opus-441.webm", kOpusAudioOnlyWebM, kAppendWholeFile);
  StartPipelineWithMediaSource(&source);
  source.EndOfStream();
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  source.Abort();
  Stop();
  EXPECT_EQ(48000,
            demuxer_->GetStream(DemuxerStream::AUDIO)
                ->audio_decoder_config()
                .samples_per_second());
}

// Ensures audio-only playback with missing or negative timestamps works.  Tests
// the common live-streaming case for chained ogg.  See http://crbug.com/396864.
TEST_F(PipelineIntegrationTest, BasicPlaybackChainedOgg) {
  ASSERT_EQ(PIPELINE_OK, Start("double-sfx.ogg"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  ASSERT_EQ(base::TimeDelta(), demuxer_->GetStartTime());
}

// Ensures audio-video playback with missing or negative timestamps fails softly
// instead of crashing.  See http://crbug.com/396864.
TEST_F(PipelineIntegrationTest, BasicPlaybackChainedOggVideo) {
  ASSERT_EQ(PIPELINE_OK, Start("double-bear.ogv"));
  Play();
  EXPECT_EQ(PIPELINE_ERROR_DECODE, WaitUntilEndedOrError());
  ASSERT_EQ(base::TimeDelta(), demuxer_->GetStartTime());
}

// Tests that we signal ended even when audio runs longer than video track.
TEST_F(PipelineIntegrationTest, BasicPlaybackAudioLongerThanVideo) {
  ASSERT_EQ(PIPELINE_OK, Start("bear_audio_longer_than_video.ogv"));
  // Audio track is 2000ms. Video track is 1001ms. Duration should be higher
  // of the two.
  EXPECT_EQ(2000, pipeline_->GetMediaDuration().InMilliseconds());
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

// Tests that we signal ended even when audio runs shorter than video track.
TEST_F(PipelineIntegrationTest, BasicPlaybackAudioShorterThanVideo) {
  ASSERT_EQ(PIPELINE_OK, Start("bear_audio_shorter_than_video.ogv"));
  // Audio track is 500ms. Video track is 1001ms. Duration should be higher of
  // the two.
  EXPECT_EQ(1001, pipeline_->GetMediaDuration().InMilliseconds());
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
}

TEST_F(PipelineIntegrationTest, BasicPlaybackPositiveStartTime) {
  ASSERT_EQ(PIPELINE_OK, Start("nonzero-start-time.webm"));
  Play();
  ASSERT_TRUE(WaitUntilOnEnded());
  ASSERT_EQ(base::TimeDelta::FromMicroseconds(396000),
            demuxer_->GetStartTime());
}

}  // namespace media
