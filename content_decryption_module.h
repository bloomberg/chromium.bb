// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CDM_CONTENT_DECRYPTION_MODULE_H_
#define CDM_CONTENT_DECRYPTION_MODULE_H_

#if defined(_MSC_VER)
typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef int int32_t;
typedef __int64 int64_t;
#else
#include <stdint.h>
#endif

// The version number must be rolled when this file is updated!
// If the CDM and the plugin use different versions of this file, the plugin
// will fail to load or crash!
#define INITIALIZE_CDM_MODULE InitializeCdmModule_3


// Define CDM_EXPORT so that functionality implemented by the CDM module
// can be exported to consumers.
#if defined(WIN32)

#if defined(CDM_IMPLEMENTATION)
#define CDM_EXPORT __declspec(dllexport)
#else
#define CDM_EXPORT __declspec(dllimport)
#endif  // defined(CDM_IMPLEMENTATION)

#else  // defined(WIN32)

#if defined(CDM_IMPLEMENTATION)
#define CDM_EXPORT __attribute__((visibility("default")))
#else
#define CDM_EXPORT
#endif

#endif  // defined(WIN32)


namespace cdm {
class Allocator;
class ContentDecryptionModule;
class Host;
}

extern "C" {
CDM_EXPORT void INITIALIZE_CDM_MODULE();
CDM_EXPORT void DeinitializeCdmModule();
// Caller retains ownership of arguments, which must outlive the call to
// DestroyCdmInstance below.
CDM_EXPORT cdm::ContentDecryptionModule* CreateCdmInstance(
    const char* key_system,
    int key_system_size,
    cdm::Allocator* allocator,
    cdm::Host* host);
CDM_EXPORT void DestroyCdmInstance(cdm::ContentDecryptionModule* instance);
CDM_EXPORT const char* GetCdmVersion();
}

namespace cdm {

class AudioFrames;
class Buffer;
class DecryptedBlock;
class VideoFrame;

enum Status {
  kSuccess = 0,
  kNeedMoreData,  // Decoder needs more data to produce a decoded frame/sample.
  kNoKey,  // The required decryption key is not available.
  kSessionError,  // Session management error.
  kDecryptError,  // Decryption failed.
  kDecodeError  // Error decoding audio or video.
};

// This must be consistent with MediaKeyError defined in the spec:
// http://goo.gl/rbdnR
enum MediaKeyError {
  kUnknownError = 1,
  kClientError,
  kServiceError,
  kOutputError,
  kHardwareChangeError,
  kDomainError
};

// An input buffer can be split into several continuous subsamples.
// A SubsampleEntry specifies the number of clear and cipher bytes in each
// subsample. For example, the following buffer has three subsamples:
//
// |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
// |   clear1   |  cipher1  |  clear2  |   cipher2   | clear3 |    cipher3    |
//
// For decryption, all of the cipher bytes in a buffer should be concatenated
// (in the subsample order) into a single logical stream. The clear bytes should
// not be considered as part of decryption.
//
// Stream to decrypt:   |  cipher1  |   cipher2   |    cipher3    |
// Decrypted stream:    | decrypted1|  decrypted2 |   decrypted3  |
//
// After decryption, the decrypted bytes should be copied over the position
// of the corresponding cipher bytes in the original buffer to form the output
// buffer. Following the above example, the decrypted buffer should be:
//
// |<----- subsample1 ----->|<----- subsample2 ----->|<----- subsample3 ----->|
// |   clear1   | decrypted1|  clear2  |  decrypted2 | clear3 |   decrypted3  |
//
// TODO(xhwang): Add checks to make sure these structs have fixed layout.
struct SubsampleEntry {
  SubsampleEntry(int32_t clear_bytes, int32_t cipher_bytes)
      : clear_bytes(clear_bytes), cipher_bytes(cipher_bytes) {}

  int32_t clear_bytes;
  int32_t cipher_bytes;
};

// Represents an input buffer to be decrypted (and possibly decoded). It does
// own any pointers in this struct.
struct InputBuffer {
  InputBuffer()
      : data(NULL),
        data_size(0),
        data_offset(0),
        key_id(NULL),
        key_id_size(0),
        iv(NULL),
        iv_size(0),
        subsamples(NULL),
        num_subsamples(0),
        timestamp(0) {}

  const uint8_t* data;  // Pointer to the beginning of the input data.
  int32_t data_size;  // Size (in bytes) of |data|.

  int32_t data_offset;  // Number of bytes to be discarded before decryption.

  const uint8_t* key_id;  // Key ID to identify the decryption key.
  int32_t key_id_size;  // Size (in bytes) of |key_id|.

  const uint8_t* iv;  // Initialization vector.
  int32_t iv_size;  // Size (in bytes) of |iv|.

  const struct SubsampleEntry* subsamples;
  int32_t num_subsamples;  // Number of subsamples in |subsamples|.

  int64_t timestamp;  // Presentation timestamp in microseconds.
};

struct AudioDecoderConfig {
  enum AudioCodec {
    kUnknownAudioCodec = 0,
    kCodecVorbis,
    kCodecAac
  };

  AudioDecoderConfig()
      : codec(kUnknownAudioCodec),
        channel_count(0),
        bits_per_channel(0),
        samples_per_second(0),
        extra_data(NULL),
        extra_data_size(0) {}

  AudioCodec codec;
  int32_t channel_count;
  int32_t bits_per_channel;
  int32_t samples_per_second;

  // Optional byte data required to initialize audio decoders, such as the
  // vorbis setup header.
  uint8_t* extra_data;
  int32_t extra_data_size;
};

// Surface formats based on FOURCC labels, see: http://www.fourcc.org/yuv.php
enum VideoFormat {
  kUnknownVideoFormat = 0,  // Unknown format value.  Used for error reporting.
  kYv12,  // 12bpp YVU planar 1x1 Y, 2x2 VU samples.
  kI420  // 12bpp YVU planar 1x1 Y, 2x2 UV samples.
};

struct Size {
  Size() : width(0), height(0) {}
  Size(int32_t width, int32_t height) : width(width), height(height) {}

  int32_t width;
  int32_t height;
};

struct VideoDecoderConfig {
  enum VideoCodec {
    kUnknownVideoCodec = 0,
    kCodecVp8,
    kCodecH264
  };

  enum VideoCodecProfile {
    kUnknownVideoCodecProfile = 0,
    kVp8ProfileMain,
    kH264ProfileBaseline,
    kH264ProfileMain,
    kH264ProfileExtended,
    kH264ProfileHigh,
    kH264ProfileHigh10,
    kH264ProfileHigh422,
    kH264ProfileHigh444Predictive
  };

  VideoDecoderConfig()
      : codec(kUnknownVideoCodec),
        profile(kUnknownVideoCodecProfile),
        format(kUnknownVideoFormat),
        extra_data(NULL),
        extra_data_size(0) {}

  VideoCodec codec;
  VideoCodecProfile profile;
  VideoFormat format;

  // Width and height of video frame immediately post-decode. Not all pixels
  // in this region are valid.
  Size coded_size;

  // Optional byte data required to initialize video decoders, such as H.264
  // AAVC data.
  uint8_t* extra_data;
  int32_t extra_data_size;
};

enum StreamType {
  kStreamTypeAudio = 0,
  kStreamTypeVideo = 1
};

// ContentDecryptionModule interface that all CDMs need to implement.
// Note: ContentDecryptionModule implementations must use the allocator
// provided in CreateCdmInstance() to allocate any Buffer that needs to
// be passed back to the caller. Implementations must call Buffer::Destroy()
// when a Buffer is created that will never be returned to the caller.
class ContentDecryptionModule {
 public:
  // Generates a |key_request| given |type| and |init_data|.
  //
  // Returns kSuccess if the key request was successfully generated, in which
  // case the CDM must send the key message by calling Host::SendKeyMessage().
  // Returns kSessionError if any error happened, in which case the CDM must
  // send a key error by calling Host::SendKeyError().
  // TODO(xhwang): A CDM may support multiple key systems. Pass in key system
  // in this and other calls to support that.
  virtual Status GenerateKeyRequest(
      const char* type, int type_size,
      const uint8_t* init_data, int init_data_size) = 0;

  // Adds the |key| to the CDM to be associated with |key_id|.
  //
  // Returns kSuccess if the key was successfully added, kSessionError
  // otherwise.
  virtual Status AddKey(const char* session_id, int session_id_size,
                        const uint8_t* key, int key_size,
                        const uint8_t* key_id, int key_id_size) = 0;

  // Cancels any pending key request made to the CDM for |session_id|.
  //
  // Returns kSuccess if all pending key requests for |session_id| were
  // successfully canceled or there was no key request to be canceled,
  // kSessionError otherwise.
  virtual Status CancelKeyRequest(
      const char* session_id, int session_id_size) = 0;

  // Performs scheduled operation with |context| when the timer fires.
  virtual void TimerExpired(void* context) = 0;

  // Decrypts the |encrypted_buffer|.
  //
  // Returns kSuccess if decryption succeeded, in which case the callee
  // should have filled the |decrypted_buffer| and passed the ownership of
  // |data| in |decrypted_buffer| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kDecryptError if any other error happened.
  // If the return value is not kSuccess, |decrypted_buffer| should be ignored
  // by the caller.
  virtual Status Decrypt(const InputBuffer& encrypted_buffer,
                         DecryptedBlock* decrypted_buffer) = 0;

  // Initializes the CDM audio decoder with |audio_decoder_config|. This
  // function must be called before DecryptAndDecodeSamples() is called.
  //
  // Returns kSuccess if the |audio_decoder_config| is supported and the CDM
  // audio decoder is successfully initialized.
  // Returns kSessionError if |audio_decoder_config| is not supported. The CDM
  // may still be able to do Decrypt().
  //
  // TODO(xhwang): Add stream ID here and in the following audio decoder
  // functions when we need to support multiple audio streams in one CDM.
  virtual Status InitializeAudioDecoder(
      const AudioDecoderConfig& audio_decoder_config) = 0;

  // Initializes the CDM video decoder with |video_decoder_config|. This
  // function must be called before DecryptAndDecodeFrame() is called.
  //
  // Returns kSuccess if the |video_decoder_config| is supported and the CDM
  // video decoder is successfully initialized.
  // Returns kSessionError if |video_decoder_config| is not supported. The CDM
  // may still be able to do Decrypt().
  //
  // TODO(xhwang): Add stream ID here and in the following video decoder
  // functions when we need to support multiple video streams in one CDM.
  virtual Status InitializeVideoDecoder(
      const VideoDecoderConfig& video_decoder_config) = 0;

  // De-initializes the CDM decoder and sets it to an uninitialized state. The
  // caller can initialize the decoder again after this call to re-initialize
  // it. This can be used to reconfigure the decoder if the configuration
  // changes.
  virtual void DeinitializeDecoder(StreamType decoder_type) = 0;

  // Resets the CDM decoder to an initialized clean state. All internal buffers
  // MUST be flushed.
  virtual void ResetDecoder(StreamType decoder_type) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into a
  // |video_frame|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |video_frame| (|format| == kEmptyVideoFrame) is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled the |video_frame| and passed the ownership of
  // |frame_buffer| in |video_frame| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // a decoded frame (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |video_frame| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeFrame(const InputBuffer& encrypted_buffer,
                                       VideoFrame* video_frame) = 0;

  // Decrypts the |encrypted_buffer| and decodes the decrypted buffer into
  // |audio_frames|. Upon end-of-stream, the caller should call this function
  // repeatedly with empty |encrypted_buffer| (|data| == NULL) until only empty
  // |audio_frames| is produced.
  //
  // Returns kSuccess if decryption and decoding both succeeded, in which case
  // the callee will have filled |audio_frames| and passed the ownership of
  // |data| in |audio_frames| to the caller.
  // Returns kNoKey if the CDM did not have the necessary decryption key
  // to decrypt.
  // Returns kNeedMoreData if more data was needed by the decoder to generate
  // audio samples (e.g. during initialization and end-of-stream).
  // Returns kDecryptError if any decryption error happened.
  // Returns kDecodeError if any decoding error happened.
  // If the return value is not kSuccess, |audio_frames| should be ignored by
  // the caller.
  virtual Status DecryptAndDecodeSamples(const InputBuffer& encrypted_buffer,
                                         AudioFrames* audio_frames) = 0;

  virtual ~ContentDecryptionModule() {}
};

// Represents a buffer created by Allocator implementations.
class Buffer {
 public:
  // Destroys the buffer in the same context as it was created.
  virtual void Destroy() = 0;

  virtual int32_t Capacity() const = 0;
  virtual uint8_t* Data() = 0;
  virtual void SetSize(int32_t size) = 0;
  virtual int32_t Size() const = 0;

 protected:
  Buffer() {}
  virtual ~Buffer() {}

 private:
  Buffer(const Buffer&);
  void operator=(const Buffer&);
};

// Interface class that hides cross object memory allocation details from CDMs.
class Allocator {
 public:
  // Returns a Buffer* containing non-zero members upon success, or NULL on
  // failure. The caller owns the Buffer* after this call. The buffer is not
  // guaranteed to be zero initialized. The capacity of the allocated Buffer
  // is guaranteed to be not less than |capacity|.
  virtual Buffer* Allocate(int32_t capacity) = 0;

 protected:
  Allocator() {}
  virtual ~Allocator() {}
};

class Host {
 public:
  Host() {}
  virtual ~Host() {}

  // Requests the host to call ContentDecryptionModule::TimerFired() |delay_ms|
  // from now with |context|.
  virtual void SetTimer(int64_t delay_ms, void* context) = 0;

  // Returns the current epoch wall time in seconds.
  virtual double GetCurrentWallTimeInSeconds() = 0;

  virtual void SendKeyMessage(
      const char* session_id, int32_t session_id_length,
      const char* message, int32_t message_length,
      const char* default_url, int32_t default_url_length) = 0;

  virtual void SendKeyError(const char* session_id,
                            int32_t session_id_length,
                            MediaKeyError error_code,
                            uint32_t system_code) = 0;
};

// Represents a decrypted block that has not been decoded.
class DecryptedBlock {
 public:
  virtual void SetDecryptedBuffer(Buffer* buffer) = 0;
  virtual Buffer* DecryptedBuffer() = 0;

  // TODO(tomfinegan): Figure out if timestamp is really needed. If it is not,
  // we can just pass Buffer pointers around.
  virtual void SetTimestamp(int64_t timestamp) = 0;
  virtual int64_t Timestamp() const = 0;

 protected:
  DecryptedBlock() {}
  virtual ~DecryptedBlock() {}
};

class VideoFrame {
 public:
  enum VideoPlane {
    kYPlane = 0,
    kUPlane = 1,
    kVPlane = 2,
    kMaxPlanes = 3,
  };

  virtual void SetFormat(VideoFormat format) = 0;
  virtual VideoFormat Format() const = 0;

  virtual void SetSize(cdm::Size size) = 0;
  virtual cdm::Size Size() const = 0;

  virtual void SetFrameBuffer(Buffer* frame_buffer) = 0;
  virtual Buffer* FrameBuffer() = 0;

  virtual void SetPlaneOffset(VideoPlane plane, int32_t offset) = 0;
  virtual int32_t PlaneOffset(VideoPlane plane) = 0;

  virtual void SetStride(VideoPlane plane, int32_t stride) = 0;
  virtual int32_t Stride(VideoPlane plane) = 0;

  virtual void SetTimestamp(int64_t timestamp) = 0;
  virtual int64_t Timestamp() const = 0;

 protected:
  VideoFrame() {}
  virtual ~VideoFrame() {}
};

// Represents decrypted and decoded audio frames. AudioFrames can contain
// multiple audio output buffers, which are serialized into this format:
//
// |<------------------- serialized audio buffer ------------------->|
// | int64_t timestamp | int64_t length | length bytes of audio data |
//
// For example, with three audio output buffers, the AudioFrames will look
// like this:
//
// |<----------------- AudioFrames ------------------>|
// | audio buffer 0 | audio buffer 1 | audio buffer 2 |
class AudioFrames {
 public:
  virtual void SetFrameBuffer(Buffer* buffer) = 0;
  virtual Buffer* FrameBuffer() = 0;

 protected:
  AudioFrames() {}
  virtual ~AudioFrames() {}
};

}  // namespace cdm

#endif  // CDM_CONTENT_DECRYPTION_MODULE_H_
