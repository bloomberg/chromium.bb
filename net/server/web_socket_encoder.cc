// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/server/web_socket_encoder.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/base/io_buffer.h"
#include "net/websockets/websocket_extension_parser.h"

namespace net {

const char WebSocketEncoder::kClientExtensions[] =
    "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits";

namespace {

const int kInflaterChunkSize = 16 * 1024;

// Constants for hybi-10 frame format.

typedef int OpCode;

const OpCode kOpCodeContinuation = 0x0;
const OpCode kOpCodeText = 0x1;
const OpCode kOpCodeBinary = 0x2;
const OpCode kOpCodeClose = 0x8;
const OpCode kOpCodePing = 0x9;
const OpCode kOpCodePong = 0xA;

const unsigned char kFinalBit = 0x80;
const unsigned char kReserved1Bit = 0x40;
const unsigned char kReserved2Bit = 0x20;
const unsigned char kReserved3Bit = 0x10;
const unsigned char kOpCodeMask = 0xF;
const unsigned char kMaskBit = 0x80;
const unsigned char kPayloadLengthMask = 0x7F;

const size_t kMaxSingleBytePayloadLength = 125;
const size_t kTwoBytePayloadLengthField = 126;
const size_t kEightBytePayloadLengthField = 127;
const size_t kMaskingKeyWidthInBytes = 4;

WebSocket::ParseResult DecodeFrameHybi17(const base::StringPiece& frame,
                                         bool client_frame,
                                         int* bytes_consumed,
                                         std::string* output,
                                         bool* compressed) {
  size_t data_length = frame.length();
  if (data_length < 2)
    return WebSocket::FRAME_INCOMPLETE;

  const char* buffer_begin = const_cast<char*>(frame.data());
  const char* p = buffer_begin;
  const char* buffer_end = p + data_length;

  unsigned char first_byte = *p++;
  unsigned char second_byte = *p++;

  bool final = (first_byte & kFinalBit) != 0;
  bool reserved1 = (first_byte & kReserved1Bit) != 0;
  bool reserved2 = (first_byte & kReserved2Bit) != 0;
  bool reserved3 = (first_byte & kReserved3Bit) != 0;
  int op_code = first_byte & kOpCodeMask;
  bool masked = (second_byte & kMaskBit) != 0;
  *compressed = reserved1;
  if (!final || reserved2 || reserved3)
    return WebSocket::FRAME_ERROR;  // Only compression extension is supported.

  bool closed = false;
  switch (op_code) {
    case kOpCodeClose:
      closed = true;
      break;
    case kOpCodeText:
      break;
    case kOpCodeBinary:        // We don't support binary frames yet.
    case kOpCodeContinuation:  // We don't support binary frames yet.
    case kOpCodePing:          // We don't support binary frames yet.
    case kOpCodePong:          // We don't support binary frames yet.
    default:
      return WebSocket::FRAME_ERROR;
  }

  if (client_frame && !masked)  // In Hybi-17 spec client MUST mask his frame.
    return WebSocket::FRAME_ERROR;

  uint64 payload_length64 = second_byte & kPayloadLengthMask;
  if (payload_length64 > kMaxSingleBytePayloadLength) {
    int extended_payload_length_size;
    if (payload_length64 == kTwoBytePayloadLengthField)
      extended_payload_length_size = 2;
    else {
      DCHECK(payload_length64 == kEightBytePayloadLengthField);
      extended_payload_length_size = 8;
    }
    if (buffer_end - p < extended_payload_length_size)
      return WebSocket::FRAME_INCOMPLETE;
    payload_length64 = 0;
    for (int i = 0; i < extended_payload_length_size; ++i) {
      payload_length64 <<= 8;
      payload_length64 |= static_cast<unsigned char>(*p++);
    }
  }

  size_t actual_masking_key_length = masked ? kMaskingKeyWidthInBytes : 0;
  static const uint64 max_payload_length = 0x7FFFFFFFFFFFFFFFull;
  static size_t max_length = std::numeric_limits<size_t>::max();
  if (payload_length64 > max_payload_length ||
      payload_length64 + actual_masking_key_length > max_length) {
    // WebSocket frame length too large.
    return WebSocket::FRAME_ERROR;
  }
  size_t payload_length = static_cast<size_t>(payload_length64);

  size_t total_length = actual_masking_key_length + payload_length;
  if (static_cast<size_t>(buffer_end - p) < total_length)
    return WebSocket::FRAME_INCOMPLETE;

  if (masked) {
    output->resize(payload_length);
    const char* masking_key = p;
    char* payload = const_cast<char*>(p + kMaskingKeyWidthInBytes);
    for (size_t i = 0; i < payload_length; ++i)  // Unmask the payload.
      (*output)[i] = payload[i] ^ masking_key[i % kMaskingKeyWidthInBytes];
  } else {
    output->assign(p, p + payload_length);
  }

  size_t pos = p + actual_masking_key_length + payload_length - buffer_begin;
  *bytes_consumed = pos;
  return closed ? WebSocket::FRAME_CLOSE : WebSocket::FRAME_OK;
}

void EncodeFrameHybi17(const std::string& message,
                       int masking_key,
                       bool compressed,
                       std::string* output) {
  std::vector<char> frame;
  OpCode op_code = kOpCodeText;
  size_t data_length = message.length();

  int reserved1 = compressed ? kReserved1Bit : 0;
  frame.push_back(kFinalBit | op_code | reserved1);
  char mask_key_bit = masking_key != 0 ? kMaskBit : 0;
  if (data_length <= kMaxSingleBytePayloadLength)
    frame.push_back(data_length | mask_key_bit);
  else if (data_length <= 0xFFFF) {
    frame.push_back(kTwoBytePayloadLengthField | mask_key_bit);
    frame.push_back((data_length & 0xFF00) >> 8);
    frame.push_back(data_length & 0xFF);
  } else {
    frame.push_back(kEightBytePayloadLengthField | mask_key_bit);
    char extended_payload_length[8];
    size_t remaining = data_length;
    // Fill the length into extended_payload_length in the network byte order.
    for (int i = 0; i < 8; ++i) {
      extended_payload_length[7 - i] = remaining & 0xFF;
      remaining >>= 8;
    }
    frame.insert(frame.end(), extended_payload_length,
                 extended_payload_length + 8);
    DCHECK(!remaining);
  }

  const char* data = const_cast<char*>(message.data());
  if (masking_key != 0) {
    const char* mask_bytes = reinterpret_cast<char*>(&masking_key);
    frame.insert(frame.end(), mask_bytes, mask_bytes + 4);
    for (size_t i = 0; i < data_length; ++i)  // Mask the payload.
      frame.push_back(data[i] ^ mask_bytes[i % kMaskingKeyWidthInBytes]);
  } else {
    frame.insert(frame.end(), data, data + data_length);
  }
  *output = std::string(&frame[0], frame.size());
}

}  // anonymous namespace

// static
WebSocketEncoder* WebSocketEncoder::CreateServer(
    const std::string& request_extensions,
    std::string* response_extensions) {
  bool deflate;
  bool has_client_window_bits;
  int client_window_bits;
  int server_window_bits;
  bool client_no_context_takeover;
  bool server_no_context_takeover;
  ParseExtensions(request_extensions, &deflate, &has_client_window_bits,
                  &client_window_bits, &server_window_bits,
                  &client_no_context_takeover, &server_no_context_takeover);

  if (deflate) {
    *response_extensions = base::StringPrintf(
        "permessage-deflate; server_max_window_bits=%d%s", server_window_bits,
        server_no_context_takeover ? "; server_no_context_takeover" : "");
    if (has_client_window_bits) {
      base::StringAppendF(response_extensions, "; client_max_window_bits=%d",
                          client_window_bits);
    } else {
      DCHECK_EQ(client_window_bits, 15);
    }
    return new WebSocketEncoder(true /* is_server */, server_window_bits,
                                client_window_bits, server_no_context_takeover);
  } else {
    *response_extensions = std::string();
    return new WebSocketEncoder(true /* is_server */);
  }
}

// static
WebSocketEncoder* WebSocketEncoder::CreateClient(
    const std::string& response_extensions) {
  bool deflate;
  bool has_client_window_bits;
  int client_window_bits;
  int server_window_bits;
  bool client_no_context_takeover;
  bool server_no_context_takeover;
  ParseExtensions(response_extensions, &deflate, &has_client_window_bits,
                  &client_window_bits, &server_window_bits,
                  &client_no_context_takeover, &server_no_context_takeover);

  if (deflate) {
    return new WebSocketEncoder(false /* is_server */, client_window_bits,
                                server_window_bits, client_no_context_takeover);
  } else {
    return new WebSocketEncoder(false /* is_server */);
  }
}

// static
void WebSocketEncoder::ParseExtensions(const std::string& extensions,
                                       bool* deflate,
                                       bool* has_client_window_bits,
                                       int* client_window_bits,
                                       int* server_window_bits,
                                       bool* client_no_context_takeover,
                                       bool* server_no_context_takeover) {
  *deflate = false;
  *has_client_window_bits = false;
  *client_window_bits = 15;
  *server_window_bits = 15;
  *client_no_context_takeover = false;
  *server_no_context_takeover = false;

  if (extensions.empty())
    return;

  // TODO(dgozman): split extensions header if another extension is introduced.
  WebSocketExtensionParser parser;
  parser.Parse(extensions);
  if (parser.has_error())
    return;
  if (parser.extension().name() != "permessage-deflate")
    return;

  const std::vector<WebSocketExtension::Parameter>& parameters =
      parser.extension().parameters();
  for (const auto& param : parameters) {
    const std::string& name = param.name();
    if (name == "client_max_window_bits") {
      *has_client_window_bits = true;
      if (param.HasValue()) {
        int bits = 0;
        if (base::StringToInt(param.value(), &bits) && bits >= 8 && bits <= 15)
          *client_window_bits = bits;
      }
    }
    if (name == "server_max_window_bits" && param.HasValue()) {
      int bits = 0;
      if (base::StringToInt(param.value(), &bits) && bits >= 8 && bits <= 15)
        *server_window_bits = bits;
    }
    if (name == "client_no_context_takeover")
      *client_no_context_takeover = true;
    if (name == "server_no_context_takeover")
      *server_no_context_takeover = true;
  }
  *deflate = true;
}

WebSocketEncoder::WebSocketEncoder(bool is_server) : is_server_(is_server) {
}

WebSocketEncoder::WebSocketEncoder(bool is_server,
                                   int deflate_bits,
                                   int inflate_bits,
                                   bool no_context_takeover)
    : is_server_(is_server) {
  deflater_.reset(new WebSocketDeflater(
      no_context_takeover ? WebSocketDeflater::DO_NOT_TAKE_OVER_CONTEXT
                          : WebSocketDeflater::TAKE_OVER_CONTEXT));
  inflater_.reset(
      new WebSocketInflater(kInflaterChunkSize, kInflaterChunkSize));

  if (!deflater_->Initialize(deflate_bits) ||
      !inflater_->Initialize(inflate_bits)) {
    // Disable deflate support.
    deflater_.reset();
    inflater_.reset();
  }
}

WebSocketEncoder::~WebSocketEncoder() {
}

WebSocket::ParseResult WebSocketEncoder::DecodeFrame(
    const base::StringPiece& frame,
    int* bytes_consumed,
    std::string* output) {
  bool compressed;
  WebSocket::ParseResult result =
      DecodeFrameHybi17(frame, is_server_, bytes_consumed, output, &compressed);
  if (result == WebSocket::FRAME_OK && compressed) {
    if (!Inflate(output))
      result = WebSocket::FRAME_ERROR;
  }
  return result;
}

void WebSocketEncoder::EncodeFrame(const std::string& frame,
                                   int masking_key,
                                   std::string* output) {
  std::string compressed;
  if (Deflate(frame, &compressed))
    EncodeFrameHybi17(compressed, masking_key, true, output);
  else
    EncodeFrameHybi17(frame, masking_key, false, output);
}

bool WebSocketEncoder::Inflate(std::string* message) {
  if (!inflater_)
    return false;
  if (!inflater_->AddBytes(message->data(), message->length()))
    return false;
  if (!inflater_->Finish())
    return false;

  std::vector<char> output;
  while (inflater_->CurrentOutputSize() > 0) {
    scoped_refptr<IOBufferWithSize> chunk =
        inflater_->GetOutput(inflater_->CurrentOutputSize());
    if (!chunk.get())
      return false;
    output.insert(output.end(), chunk->data(), chunk->data() + chunk->size());
  }

  *message =
      output.size() ? std::string(&output[0], output.size()) : std::string();
  return true;
}

bool WebSocketEncoder::Deflate(const std::string& message,
                               std::string* output) {
  if (!deflater_)
    return false;
  if (!deflater_->AddBytes(message.data(), message.length())) {
    deflater_->Finish();
    return false;
  }
  if (!deflater_->Finish())
    return false;
  scoped_refptr<IOBufferWithSize> buffer =
      deflater_->GetOutput(deflater_->CurrentOutputSize());
  if (!buffer.get())
    return false;
  *output = std::string(buffer->data(), buffer->size());
  return true;
}

}  // namespace net
