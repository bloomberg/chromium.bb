// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/fuchsia/cdm/fuchsia_stream_decryptor.h"

#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/media/drm/cpp/fidl.h>

#include "base/bind.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decrypt_config.h"
#include "media/base/encryption_pattern.h"
#include "media/base/subsample_entry.h"
#include "media/fuchsia/common/sysmem_buffer_reader.h"
#include "media/fuchsia/common/sysmem_buffer_writer.h"

namespace media {
namespace {

// FuchsiaClearStreamDecryptor copies decrypted data immediately once it's
// available, so it doesn't need more than one output buffer.
const size_t kMinClearStreamOutputFrames = 1;

std::string GetEncryptionMode(EncryptionMode mode) {
  switch (mode) {
    case EncryptionMode::kCenc:
      return fuchsia::media::drm::ENCRYPTION_MODE_CENC;
    case EncryptionMode::kCbcs:
      return fuchsia::media::drm::ENCRYPTION_MODE_CBCS;
    default:
      NOTREACHED() << "unknown encryption mode " << static_cast<int>(mode);
      return "";
  }
}

fuchsia::media::KeyId GetKeyId(const std::string& key_id) {
  fuchsia::media::KeyId fuchsia_key_id;
  DCHECK_EQ(key_id.size(), fuchsia_key_id.data.size());
  std::copy(key_id.begin(), key_id.end(), fuchsia_key_id.data.begin());
  return fuchsia_key_id;
}

std::vector<fuchsia::media::SubsampleEntry> GetSubsamples(
    const std::vector<SubsampleEntry>& subsamples) {
  std::vector<fuchsia::media::SubsampleEntry> fuchsia_subsamples(
      subsamples.size());

  for (size_t i = 0; i < subsamples.size(); i++) {
    fuchsia_subsamples[i].clear_bytes = subsamples[i].clear_bytes;
    fuchsia_subsamples[i].encrypted_bytes = subsamples[i].cypher_bytes;
  }

  return fuchsia_subsamples;
}

fuchsia::media::EncryptionPattern GetEncryptionPattern(
    EncryptionPattern pattern) {
  fuchsia::media::EncryptionPattern fuchsia_pattern;
  fuchsia_pattern.clear_blocks = pattern.skip_byte_block();
  fuchsia_pattern.encrypted_blocks = pattern.crypt_byte_block();
  return fuchsia_pattern;
}

fuchsia::media::FormatDetails GetFormatDetails(const DecryptConfig* config) {
  DCHECK(config);

  fuchsia::media::EncryptedFormat encrypted_format;
  encrypted_format.set_mode(GetEncryptionMode(config->encryption_mode()))
      .set_key_id(GetKeyId(config->key_id()))
      .set_init_vector(
          std::vector<uint8_t>(config->iv().begin(), config->iv().end()))
      .set_subsamples(GetSubsamples(config->subsamples()));
  if (config->encryption_mode() == EncryptionMode::kCbcs) {
    DCHECK(config->encryption_pattern().has_value());
    encrypted_format.set_pattern(
        GetEncryptionPattern(config->encryption_pattern().value()));
  }

  fuchsia::media::FormatDetails format;
  format.set_format_details_version_ordinal(0);
  format.mutable_domain()->crypto().set_encrypted(std::move(encrypted_format));
  return format;
}

}  // namespace

FuchsiaStreamDecryptorBase::FuchsiaStreamDecryptorBase(
    fuchsia::media::StreamProcessorPtr processor)
    : processor_(std::move(processor), this) {}

FuchsiaStreamDecryptorBase::~FuchsiaStreamDecryptorBase() = default;

void FuchsiaStreamDecryptorBase::DecryptInternal(
    scoped_refptr<DecoderBuffer> encrypted) {
  DCHECK(!encrypted->end_of_stream()) << "EOS frame is always clear.";
  input_writer_queue_.EnqueueBuffer(std::move(encrypted));
}

void FuchsiaStreamDecryptorBase::ResetStream() {
  // Close current stream and drop all the cached decoder buffers.
  // Keep input and output buffers to avoid buffer re-allocation.
  processor_.Reset();
  input_writer_queue_.ResetQueue();
}

// StreamProcessorHelper::Client implementation:
void FuchsiaStreamDecryptorBase::AllocateInputBuffers(
    const fuchsia::media::StreamBufferConstraints& stream_constraints) {
  base::Optional<fuchsia::sysmem::BufferCollectionConstraints>
      buffer_constraints =
          SysmemBufferWriter::GetRecommendedConstraints(stream_constraints);

  if (!buffer_constraints.has_value()) {
    OnError();
    return;
  }

  input_pool_creator_ =
      allocator_.MakeBufferPoolCreator(1 /* num_shared_token */);

  input_pool_creator_->Create(
      std::move(buffer_constraints).value(),
      base::BindOnce(&FuchsiaStreamDecryptorBase::OnInputBufferPoolCreated,
                     base::Unretained(this)));
}

void FuchsiaStreamDecryptorBase::OnInputBufferPoolCreated(
    std::unique_ptr<SysmemBufferPool> pool) {
  if (!pool) {
    DLOG(ERROR) << "Fail to allocate input buffer.";
    OnError();
    return;
  }

  input_pool_ = std::move(pool);

  // Provide token before enabling writer. Tokens must be provided to
  // StreamProcessor before getting the allocated buffers.
  processor_.CompleteInputBuffersAllocation(input_pool_->TakeToken());

  input_pool_->CreateWriter(base::BindOnce(
      &FuchsiaStreamDecryptorBase::OnWriterCreated, base::Unretained(this)));
}

void FuchsiaStreamDecryptorBase::OnWriterCreated(
    std::unique_ptr<SysmemBufferWriter> writer) {
  if (!writer) {
    OnError();
    return;
  }

  input_writer_queue_.Start(
      std::move(writer),
      base::BindRepeating(&FuchsiaStreamDecryptorBase::SendInputPacket,
                          base::Unretained(this)),
      SysmemBufferWriterQueue::EndOfStreamCB());
}

void FuchsiaStreamDecryptorBase::SendInputPacket(
    const DecoderBuffer* buffer,
    StreamProcessorHelper::IoPacket packet) {
  if (!packet.unit_end()) {
    // The encrypted data size is too big. Decryptor should consider
    // splitting the buffer and update the IV and subsample entries.
    // TODO(crbug.com/1003651): Handle large encrypted buffer correctly. For
    // now, just reject the decryption.
    LOG(ERROR) << "DecoderBuffer doesn't fit in one packet.";
    OnError();
    return;
  }

  packet.set_format(GetFormatDetails(buffer->decrypt_config()));
  processor_.Process(std::move(packet));
}

std::unique_ptr<FuchsiaClearStreamDecryptor>
FuchsiaClearStreamDecryptor::Create(
    fuchsia::media::drm::ContentDecryptionModule* cdm) {
  DCHECK(cdm);

  fuchsia::media::drm::DecryptorParams params;
  params.set_require_secure_mode(false);
  params.mutable_input_details()->set_format_details_version_ordinal(0);
  fuchsia::media::StreamProcessorPtr stream_processor;
  cdm->CreateDecryptor(std::move(params), stream_processor.NewRequest());

  return std::make_unique<FuchsiaClearStreamDecryptor>(
      std::move(stream_processor));
}

FuchsiaClearStreamDecryptor::FuchsiaClearStreamDecryptor(
    fuchsia::media::StreamProcessorPtr processor)
    : FuchsiaStreamDecryptorBase(std::move(processor)) {}

FuchsiaClearStreamDecryptor::~FuchsiaClearStreamDecryptor() = default;

void FuchsiaClearStreamDecryptor::Decrypt(
    scoped_refptr<DecoderBuffer> encrypted,
    Decryptor::DecryptCB decrypt_cb) {
  DCHECK(!decrypt_cb_);
  decrypt_cb_ = std::move(decrypt_cb);
  current_status_ = Decryptor::kSuccess;
  DecryptInternal(std::move(encrypted));
}

void FuchsiaClearStreamDecryptor::CancelDecrypt() {
  ResetStream();

  // Fire |decrypt_cb_| immediately as required by Decryptor::CancelDecrypt.
  if (decrypt_cb_)
    std::move(decrypt_cb_).Run(Decryptor::kSuccess, nullptr);
}

void FuchsiaClearStreamDecryptor::AllocateOutputBuffers(
    const fuchsia::media::StreamBufferConstraints& stream_constraints) {
  if (!stream_constraints.has_packet_count_for_client_max() ||
      !stream_constraints.has_packet_count_for_client_min()) {
    DLOG(ERROR) << "StreamBufferConstraints doesn't contain required fields.";
    OnError();
    return;
  }

  size_t max_used_output_buffers = std::max(
      kMinClearStreamOutputFrames,
      static_cast<size_t>(stream_constraints.packet_count_for_client_min()));

  output_pool_creator_ =
      allocator_.MakeBufferPoolCreator(1 /* num_shared_token */);

  output_pool_creator_->Create(
      SysmemBufferReader::GetRecommendedConstraints(max_used_output_buffers),
      base::BindOnce(&FuchsiaClearStreamDecryptor::OnOutputBufferPoolCreated,
                     base::Unretained(this), max_used_output_buffers));
}

void FuchsiaClearStreamDecryptor::OnProcessEos() {
  // Decryptor never pushes EOS frame.
  NOTREACHED();
}

void FuchsiaClearStreamDecryptor::OnOutputFormat(
    fuchsia::media::StreamOutputFormat format) {}

void FuchsiaClearStreamDecryptor::OnOutputPacket(
    StreamProcessorHelper::IoPacket packet) {
  DCHECK(decrypt_cb_);

  DCHECK(output_reader_);
  if (!output_pool_->is_live()) {
    DLOG(ERROR) << "Output buffer pool is dead.";
    return;
  }

  // If that's not the last packet for the current Decrypt() request then just
  // store the output and wait for the next packet.
  if (!packet.unit_end()) {
    size_t pos = output_data_.size();
    output_data_.resize(pos + packet.size());

    bool read_success = output_reader_->Read(
        packet.index(), packet.offset(),
        base::make_span(output_data_.data() + pos, packet.size()));

    if (!read_success) {
      // If we've failed to read a partial packet then delay reporting the error
      // until we've received the last packet to make sure we consume all output
      // packets generated by the last Decrypt() call.
      DLOG(ERROR) << "Fail to get decrypted result.";
      current_status_ = Decryptor::kError;
      output_data_.clear();
    }

    return;
  }

  // We've received the last packet. Assemble DecoderBuffer and pass it to the
  // DecryptCB.
  auto clear_buffer =
      base::MakeRefCounted<DecoderBuffer>(output_data_.size() + packet.size());
  clear_buffer->set_timestamp(packet.timestamp());

  // Copy data received in the previous packets.
  memcpy(clear_buffer->writable_data(), output_data_.data(),
         output_data_.size());
  output_data_.clear();

  // Copy data received in the last packet
  bool read_success = output_reader_->Read(
      packet.index(), packet.offset(),
      base::make_span(clear_buffer->writable_data() + output_data_.size(),
                      packet.size()));

  if (!read_success) {
    DLOG(ERROR) << "Fail to get decrypted result.";
    current_status_ = Decryptor::kError;
  }

  std::move(decrypt_cb_)
      .Run(current_status_, current_status_ == Decryptor::kSuccess
                                ? std::move(clear_buffer)
                                : nullptr);
}

void FuchsiaClearStreamDecryptor::OnNoKey() {
  if (decrypt_cb_)
    std::move(decrypt_cb_).Run(Decryptor::kNoKey, nullptr);
}

void FuchsiaClearStreamDecryptor::OnError() {
  ResetStream();
  if (decrypt_cb_)
    std::move(decrypt_cb_).Run(Decryptor::kError, nullptr);
}

void FuchsiaClearStreamDecryptor::OnOutputBufferPoolCreated(
    size_t max_used_output_buffers,
    std::unique_ptr<SysmemBufferPool> pool) {
  if (!pool) {
    LOG(ERROR) << "Fail to allocate output buffer.";
    OnError();
    return;
  }

  output_pool_ = std::move(pool);

  // Provide token before enabling reader. Tokens must be provided to
  // StreamProcessor before getting the allocated buffers.
  processor_.CompleteOutputBuffersAllocation(max_used_output_buffers,
                                             output_pool_->TakeToken());

  output_pool_->CreateReader(base::BindOnce(
      &FuchsiaClearStreamDecryptor::OnOutputBufferPoolReaderCreated,
      base::Unretained(this)));
}

void FuchsiaClearStreamDecryptor::OnOutputBufferPoolReaderCreated(
    std::unique_ptr<SysmemBufferReader> reader) {
  if (!reader) {
    LOG(ERROR) << "Fail to enable output buffer reader.";
    OnError();
    return;
  }

  DCHECK(!output_reader_);
  output_reader_ = std::move(reader);
}

}  // namespace media
