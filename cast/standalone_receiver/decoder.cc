#include "cast/standalone_receiver/decoder.h"

#include <sstream>

#include "util/logging.h"

namespace cast {
namespace streaming {

Decoder::Buffer::Buffer() {
  Resize(0);
}

Decoder::Buffer::~Buffer() = default;

void Decoder::Buffer::Resize(int new_size) {
  const int padded_size = new_size + AV_INPUT_BUFFER_PADDING_SIZE;
  if (static_cast<int>(buffer_.size()) == padded_size) {
    return;
  }
  buffer_.resize(padded_size);
  // libavcodec requires zero-padding the region at the end, as some decoders
  // will treat this as a stop marker.
  memset(buffer_.data() + new_size, 0, AV_INPUT_BUFFER_PADDING_SIZE);
}

absl::Span<const uint8_t> Decoder::Buffer::GetSpan() const {
  return const_cast<Buffer*>(this)->GetSpan();
}

absl::Span<uint8_t> Decoder::Buffer::GetSpan() {
  return absl::Span<uint8_t>(buffer_.data(),
                             buffer_.size() - AV_INPUT_BUFFER_PADDING_SIZE);
}

Decoder::Client::Client() = default;
Decoder::Client::~Client() = default;

Decoder::Decoder() = default;
Decoder::~Decoder() = default;

void Decoder::Decode(FrameId frame_id, const Decoder::Buffer& buffer) {
  if (!codec_) {
    InitFromFirstBuffer(buffer);
    if (!codec_) {
      OnError("unable to detect codec", AVERROR(EINVAL), frame_id);
      return;
    }
  }

  // Parse the buffer for the required metadata and the packet to send to the
  // decoder.
  const absl::Span<const uint8_t> input = buffer.GetSpan();
  const int bytes_consumed = av_parser_parse2(
      parser_.get(), context_.get(), &packet_->data, &packet_->size,
      input.data(), input.size(), AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
  if (bytes_consumed < 0) {
    OnError("av_parser_parse2", bytes_consumed, frame_id);
    return;
  }
  if (!packet_->data) {
    OnError("av_parser_parse2 found no packet", AVERROR_BUFFER_TOO_SMALL,
            frame_id);
    return;
  }

  // Send the packet to the decoder.
  const int send_packet_result =
      avcodec_send_packet(context_.get(), packet_.get());
  if (send_packet_result < 0) {
    // The result should not be EAGAIN because this code always pulls out all
    // the decoded frames after feeding-in each AVPacket.
    OSP_DCHECK_NE(send_packet_result, AVERROR(EAGAIN));
    OnError("avcodec_send_packet", send_packet_result, frame_id);
    return;
  }
  frames_decoding_.push_back(frame_id);

  // Receive zero or more frames from the decoder.
  for (;;) {
    const int receive_frame_result =
        avcodec_receive_frame(context_.get(), decoded_frame_.get());
    if (receive_frame_result == AVERROR(EAGAIN)) {
      break;  // Decoder needs more input to produce another frame.
    }
    const FrameId decoded_frame_id = DidReceiveFrameFromDecoder();
    if (receive_frame_result < 0) {
      OnError("avcodec_receive_frame", receive_frame_result, decoded_frame_id);
      return;
    }
    if (client_) {
      client_->OnFrameDecoded(decoded_frame_id, *decoded_frame_);
    }
    av_frame_unref(decoded_frame_.get());
  }
}

void Decoder::InitFromFirstBuffer(const Buffer& buffer) {
  const AVCodecID codec_id = Detect(buffer);
  if (codec_id == AV_CODEC_ID_NONE) {
    return;
  }

  codec_ = avcodec_find_decoder(codec_id);
  OSP_CHECK(codec_);
  parser_ = MakeUniqueAVCodecParserContext(codec_id);
  OSP_CHECK(parser_);
  context_ = MakeUniqueAVCodecContext(codec_);
  OSP_CHECK(context_);
  const int open_result = avcodec_open2(context_.get(), codec_, nullptr);
  OSP_CHECK_EQ(open_result, 0);
  packet_ = MakeUniqueAVPacket();
  OSP_CHECK(packet_);
  decoded_frame_ = MakeUniqueAVFrame();
  OSP_CHECK(decoded_frame_);
}

FrameId Decoder::DidReceiveFrameFromDecoder() {
  const auto it = frames_decoding_.begin();
  OSP_DCHECK(it != frames_decoding_.end());
  const auto frame_id = *it;
  frames_decoding_.erase(it);
  return frame_id;
}

void Decoder::OnError(const char* what, int av_errnum, FrameId frame_id) {
  if (!client_) {
    return;
  }

  // Make a human-readable string from the libavcodec error.
  std::ostringstream error;
  if (!frame_id.is_null()) {
    error << "frame: " << frame_id << "; ";
  }
  error << "what: " << what << "; error: " << av_err2str(av_errnum);

  // Dispatch to either the fatal error handler, or the one for decode errors,
  // as appropriate.
  switch (av_errnum) {
    case AVERROR_EOF:
    case AVERROR(EINVAL):
    case AVERROR(ENOMEM):
      client_->OnFatalError(error.str());
      break;
    default:
      client_->OnDecodeError(frame_id, error.str());
      break;
  }
}

// static
AVCodecID Decoder::Detect(const Buffer& buffer) {
  static constexpr AVCodecID kCodecsToTry[] = {
      AV_CODEC_ID_VP8,  AV_CODEC_ID_VP9,  AV_CODEC_ID_H264,
      AV_CODEC_ID_H265, AV_CODEC_ID_OPUS, AV_CODEC_ID_FLAC,
  };

  const absl::Span<const uint8_t> input = buffer.GetSpan();
  for (AVCodecID codec_id : kCodecsToTry) {
    AVCodec* const codec = avcodec_find_decoder(codec_id);
    if (!codec) {
      OSP_LOG_WARN << "Video codec not available to try: "
                   << avcodec_get_name(codec_id);
      continue;
    }
    const auto parser = MakeUniqueAVCodecParserContext(codec_id);
    if (!parser) {
      OSP_LOG_ERROR << "Failed to init parser for codec: "
                    << avcodec_get_name(codec_id);
      continue;
    }
    const auto context = MakeUniqueAVCodecContext(codec);
    if (!context || avcodec_open2(context.get(), codec, nullptr) != 0) {
      OSP_LOG_ERROR << "Failed to open codec: " << avcodec_get_name(codec_id);
      continue;
    }
    const auto packet = MakeUniqueAVPacket();
    OSP_CHECK(packet);
    if (av_parser_parse2(parser.get(), context.get(), &packet->data,
                         &packet->size, input.data(), input.size(),
                         AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0) < 0 ||
        !packet->data || packet->size == 0) {
      OSP_VLOG << "Does not parse as codec: " << avcodec_get_name(codec_id);
      continue;
    }
    if (avcodec_send_packet(context.get(), packet.get()) < 0) {
      OSP_VLOG << "avcodec_send_packet() failed, probably wrong codec version: "
               << avcodec_get_name(codec_id);
      continue;
    }
    OSP_VLOG << "Detected codec: " << avcodec_get_name(codec_id);
    return codec_id;
  }

  return AV_CODEC_ID_NONE;
}

}  // namespace streaming
}  // namespace cast
