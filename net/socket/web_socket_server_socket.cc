// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/web_socket_server_socket.h"

#include <algorithm>
#include <deque>
#include <limits>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "base/sys_byteorder.h"
#include "googleurl/src/gurl.h"
#include "net/base/completion_callback.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"

namespace {

const size_t kHandshakeLimitBytes = 1 << 14;

const char kCrOctet = '\r';
COMPILE_ASSERT(kCrOctet == '\x0d', ASCII);
const char kLfOctet = '\n';
COMPILE_ASSERT(kLfOctet == '\x0a', ASCII);
const char kSpaceOctet = ' ';
COMPILE_ASSERT(kSpaceOctet == '\x20', ASCII);
const char kCommaOctet = ',';
COMPILE_ASSERT(kCommaOctet == '\x2c', ASCII);

const char kCRLF[] = { kCrOctet, kLfOctet, 0 };
const char kCRLFCRLF[] = { kCrOctet, kLfOctet, kCrOctet, kLfOctet, 0 };

const char kPlainHostFieldName[] = "Host";
const char kPlainOriginFieldName[] = "Origin";
const char kOriginFieldName[] = "Sec-WebSocket-Origin";
const char kProtocolFieldName[] = "Sec-WebSocket-Protocol";
const char kVersionFieldName[] = "Sec-WebSocket-Version";
const char kLocationFieldName[] = "Sec-WebSocket-Location";
const char kKey1FieldName[] = "Sec-WebSocket-Key1";
const char kKey2FieldName[] = "Sec-WebSocket-Key2";

int CountSpaces(const std::string& s) {
  return std::count(s.begin(), s.end(), kSpaceOctet);
}

// Returns true on success.
bool FetchDecimalDigits(const std::string& s, uint32* result) {
  *result = 0;
  bool got_something = false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (IsAsciiDigit(s[i])) {
      got_something = true;
      if (*result > std::numeric_limits<uint32>::max() / 10)
        return false;
      *result *= 10;
      int digit = s[i] - '0';
      if (*result > std::numeric_limits<uint32>::max() - digit)
        return false;
      *result += digit;
    }
  }
  return got_something;
}

// Returns number of fetched subprotocols or negative error code.
int FetchSubprotocolList(
    const std::string& s, std::vector<std::string>* subprotocol_list) {
  subprotocol_list->clear();
  subprotocol_list->push_back(std::string());
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] > '\x20' && s[i] < '\x7f' && s[i] != kCommaOctet)
      subprotocol_list->back() += s[i];
    else if (!subprotocol_list->back().empty()) {
      if (subprotocol_list->size() < 16)
        subprotocol_list->push_back(std::string());
      else
        return net::ERR_LIMIT_VIOLATION;
    }
  }
  if (subprotocol_list->back().empty())
    subprotocol_list->pop_back();
  if (subprotocol_list->empty())
    return net::ERR_WS_PROTOCOL_ERROR;

  {
    std::vector<std::string> tmp(*subprotocol_list);
    std::sort(tmp.begin(), tmp.end());
    if (tmp.end() != std::unique(tmp.begin(), tmp.end()))
      return net::ERR_WS_PROTOCOL_ERROR;
  }
  return subprotocol_list->size();
}

class WebSocketServerSocketImpl : public net::WebSocketServerSocket {
 public:
  WebSocketServerSocketImpl(net::Socket* transport_socket, Delegate* delegate)
      : phase_(PHASE_NYMPH),
        frame_bytes_remaining_(0),
        transport_socket_(transport_socket),
        delegate_(delegate),
        handshake_buf_(new net::IOBuffer(kHandshakeLimitBytes)),
        fill_handshake_buf_(new net::DrainableIOBuffer(
            handshake_buf_, kHandshakeLimitBytes)),
        process_handshake_buf_(new net::DrainableIOBuffer(
            handshake_buf_, kHandshakeLimitBytes)),
        is_transport_read_pending_(false),
        is_transport_write_pending_(false),
        weak_factory_(this) {
    DCHECK(transport_socket);
    DCHECK(delegate);
  }

  virtual ~WebSocketServerSocketImpl() {
    std::deque<PendingReq>::iterator it = GetPendingReq(PendingReq::TYPE_READ);
    if (it != pending_reqs_.end() &&
        it->type == PendingReq::TYPE_READ &&
        it->io_buf != NULL &&
        it->io_buf->data() != NULL &&
        !it->callback.is_null()) {
      it->callback.Run(0);  // Report EOF.
    }
  }

 private:
  enum Phase {
    // Before Accept() is called.
    PHASE_NYMPH,

    // After Accept() is called and until handshake success/fail.
    PHASE_HANDSHAKE,

    // Processing data stream.
    PHASE_FRAME_OUTSIDE,  // Outside data frame.
    PHASE_FRAME_INSIDE,  // Inside text frame.
    PHASE_FRAME_LENGTH,  // Reading length of binary frame.
    PHASE_FRAME_SKIP,  // Skipping binary frame.

    // After termination.
    PHASE_SHUT
  };

  struct PendingReq {
    enum Type {
      // Frame delimiters or handshake (as opposed to user data).
      TYPE_METADATA       = 1 << 0,
      // Read request.
      TYPE_READ           = 1 << 1,
      // Write request.
      TYPE_WRITE          = 1 << 2,

      TYPE_READ_METADATA  = TYPE_READ | TYPE_METADATA,
      TYPE_WRITE_METADATA = TYPE_WRITE | TYPE_METADATA
    };

    PendingReq(Type type, net::DrainableIOBuffer* io_buf,
               const net::CompletionCallback& callback)
        : type(type),
          io_buf(io_buf),
          callback(callback) {
      switch (type) {
        case PendingReq::TYPE_READ:
        case PendingReq::TYPE_WRITE:
        case PendingReq::TYPE_READ_METADATA:
        case PendingReq::TYPE_WRITE_METADATA: {
          DCHECK(io_buf);
          break;
        }
        default: {
          NOTREACHED();
          break;
        }
      }
    }

    Type type;
    scoped_refptr<net::DrainableIOBuffer> io_buf;
    net::CompletionCallback callback;
  };

  // Socket implementation.
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   const net::CompletionCallback& callback) OVERRIDE {
    if (buf_len == 0)
      return 0;
    if (buf == NULL || buf_len < 0) {
      NOTREACHED();
      return net::ERR_INVALID_ARGUMENT;
    }
    while (int bytes_remaining = fill_handshake_buf_->BytesConsumed() -
        process_handshake_buf_->BytesConsumed()) {
      DCHECK(!is_transport_read_pending_);
      DCHECK(GetPendingReq(PendingReq::TYPE_READ) == pending_reqs_.end());
      switch (phase_) {
        case PHASE_FRAME_OUTSIDE:
        case PHASE_FRAME_INSIDE:
        case PHASE_FRAME_LENGTH:
        case PHASE_FRAME_SKIP: {
          int n = std::min(bytes_remaining, buf_len);
          int rv = ProcessDataFrames(
              process_handshake_buf_->data(), n, buf->data(), buf_len);
          process_handshake_buf_->DidConsume(n);
          if (rv == 0) {
            // ProcessDataFrames may return zero for non-empty buffer if it
            // contains only frame delimiters without real data.  In this case:
            // try again and do not just return zero (zero stands for EOF).
            continue;
          }
          return rv;
        }
        case PHASE_SHUT: {
          return 0;
        }
        case PHASE_NYMPH:
        case PHASE_HANDSHAKE:
        default: {
          NOTREACHED();
          return net::ERR_UNEXPECTED;
        }
      }
    }
    switch (phase_) {
      case PHASE_FRAME_OUTSIDE:
      case PHASE_FRAME_INSIDE:
      case PHASE_FRAME_LENGTH:
      case PHASE_FRAME_SKIP: {
        pending_reqs_.push_back(PendingReq(
            PendingReq::TYPE_READ,
            new net::DrainableIOBuffer(buf, buf_len),
            callback));
        ConsiderTransportRead();
        break;
      }
      case PHASE_SHUT: {
        return 0;
      }
      case PHASE_NYMPH:
      case PHASE_HANDSHAKE:
      default: {
        NOTREACHED();
        return net::ERR_UNEXPECTED;
      }
    }
    return net::ERR_IO_PENDING;
  }

  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE {
    if (buf_len == 0)
      return 0;
    if (buf == NULL || buf_len < 0) {
      NOTREACHED();
      return net::ERR_INVALID_ARGUMENT;
    }
    DCHECK_EQ(std::find(buf->data(), buf->data() + buf_len, '\xff'),
              buf->data() + buf_len);
    switch (phase_) {
      case PHASE_FRAME_OUTSIDE:
      case PHASE_FRAME_INSIDE:
      case PHASE_FRAME_LENGTH:
      case PHASE_FRAME_SKIP: {
        break;
      }
      case PHASE_SHUT: {
        return net::ERR_SOCKET_NOT_CONNECTED;
      }
      case PHASE_NYMPH:
      case PHASE_HANDSHAKE:
      default: {
        NOTREACHED();
        return net::ERR_UNEXPECTED;
      }
    }

    net::IOBuffer* frame_start = new net::IOBuffer(1);
    frame_start->data()[0] = '\x00';
    pending_reqs_.push_back(PendingReq(PendingReq::TYPE_WRITE_METADATA,
                            new net::DrainableIOBuffer(frame_start, 1),
                            net::CompletionCallback()));

    pending_reqs_.push_back(PendingReq(PendingReq::TYPE_WRITE,
                            new net::DrainableIOBuffer(buf, buf_len),
                            callback));

    net::IOBuffer* frame_end = new net::IOBuffer(1);
    frame_end->data()[0] = '\xff';
    pending_reqs_.push_back(PendingReq(PendingReq::TYPE_WRITE_METADATA,
                            new net::DrainableIOBuffer(frame_end, 1),
                            net::CompletionCallback()));

    ConsiderTransportWrite();
    return net::ERR_IO_PENDING;
  }

  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE {
    return transport_socket_->SetReceiveBufferSize(size);
  }

  virtual bool SetSendBufferSize(int32 size) OVERRIDE {
    return transport_socket_->SetSendBufferSize(size);
  }

  // WebSocketServerSocket implementation.
  virtual int Accept(const net::CompletionCallback& callback) OVERRIDE {
    if (phase_ != PHASE_NYMPH)
      return net::ERR_UNEXPECTED;
    phase_ = PHASE_HANDSHAKE;
    pending_reqs_.push_front(PendingReq(
        PendingReq::TYPE_READ_METADATA, fill_handshake_buf_.get(), callback));
    ConsiderTransportRead();
    return net::ERR_IO_PENDING;
  }

  std::deque<PendingReq>::iterator GetPendingReq(PendingReq::Type type) {
    for (std::deque<PendingReq>::iterator it = pending_reqs_.begin();
         it != pending_reqs_.end(); ++it) {
      if (it->type & type)
        return it;
    }
    return pending_reqs_.end();
  }

  void ConsiderTransportRead() {
    if (pending_reqs_.empty())
      return;
    if (is_transport_read_pending_)
      return;
    std::deque<PendingReq>::iterator it = GetPendingReq(PendingReq::TYPE_READ);
    if (it == pending_reqs_.end())
      return;
    if (it->io_buf == NULL || it->io_buf->BytesRemaining() == 0) {
      NOTREACHED();
      return;
    }
    is_transport_read_pending_ = true;
    int rv = transport_socket_->Read(
        it->io_buf.get(), it->io_buf->BytesRemaining(),
        base::Bind(&WebSocketServerSocketImpl::OnRead,
                   base::Unretained(this)));
    if (rv != net::ERR_IO_PENDING) {
      // PostTask rather than direct call in order to:
      // (1) guarantee calling callback after returning from Read();
      // (2) avoid potential stack overflow;
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&WebSocketServerSocketImpl::OnRead,
                                weak_factory_.GetWeakPtr(), rv));
    }
  }

  void ConsiderTransportWrite() {
    if (is_transport_write_pending_)
      return;
    if (pending_reqs_.empty())
      return;
    std::deque<PendingReq>::iterator it = GetPendingReq(PendingReq::TYPE_WRITE);
    if (it == pending_reqs_.end())
      return;
    if (it->io_buf == NULL || it->io_buf->BytesRemaining() == 0) {
      NOTREACHED();
      Shut(net::ERR_UNEXPECTED);
      return;
    }
    is_transport_write_pending_ = true;
    int rv = transport_socket_->Write(
        it->io_buf.get(), it->io_buf->BytesRemaining(),
        base::Bind(&WebSocketServerSocketImpl::OnWrite,
                   base::Unretained(this)));
    if (rv != net::ERR_IO_PENDING) {
      // PostTask rather than direct call in order to:
      // (1) guarantee calling callback after returning from Read();
      // (2) avoid potential stack overflow;
      MessageLoop::current()->PostTask(
          FROM_HERE, base::Bind(&WebSocketServerSocketImpl::OnWrite,
                                weak_factory_.GetWeakPtr(), rv));
    }
  }

  void Shut(int result) {
    if (result > 0 || result == net::ERR_IO_PENDING)
      result = net::ERR_UNEXPECTED;
    if (result != 0) {
      while (!pending_reqs_.empty()) {
        PendingReq& req = pending_reqs_.front();
        if (!req.callback.is_null())
          req.callback.Run(result);
        pending_reqs_.pop_front();
      }
      transport_socket_.reset();  // terminate underlying connection.
    }
    phase_ = PHASE_SHUT;
  }

  // Callbacks for transport socket.
  void OnRead(int result) {
    if (!is_transport_read_pending_) {
      NOTREACHED();
      Shut(net::ERR_UNEXPECTED);
      return;
    }
    is_transport_read_pending_ = false;

    if (result <= 0) {
      Shut(result);
      return;
    }

    std::deque<PendingReq>::iterator it = GetPendingReq(PendingReq::TYPE_READ);
    if (it == pending_reqs_.end() ||
        it->io_buf == NULL ||
        it->io_buf->data() == NULL) {
      NOTREACHED();
      Shut(net::ERR_UNEXPECTED);
      return;
    }
    if ((phase_ == PHASE_HANDSHAKE) == (it->type == PendingReq::TYPE_READ)) {
      NOTREACHED();
      Shut(net::ERR_UNEXPECTED);
      return;
    }

    switch (phase_) {
      case PHASE_HANDSHAKE: {
        if (it != pending_reqs_.begin() || it->io_buf != fill_handshake_buf_) {
          NOTREACHED();
          Shut(net::ERR_UNEXPECTED);
          return;
        }
        fill_handshake_buf_->DidConsume(result);
        // ProcessHandshake invalidates iterators for |pending_reqs_|
        int rv = ProcessHandshake();
        if (rv > 0) {
          process_handshake_buf_->DidConsume(rv);
          phase_ = PHASE_FRAME_OUTSIDE;
          net::CompletionCallback cb = pending_reqs_.front().callback;
          pending_reqs_.pop_front();
          ConsiderTransportWrite();  // Schedule answer handshake.
          if (!cb.is_null())
            cb.Run(0);
        } else if (rv == net::ERR_IO_PENDING) {
          if (fill_handshake_buf_->BytesRemaining() < 1)
            Shut(net::ERR_LIMIT_VIOLATION);
        } else if (rv < 0) {
          Shut(rv);
        } else {
          Shut(net::ERR_UNEXPECTED);
        }
        break;
      }
      case PHASE_FRAME_OUTSIDE:
      case PHASE_FRAME_INSIDE:
      case PHASE_FRAME_LENGTH:
      case PHASE_FRAME_SKIP: {
        int rv = ProcessDataFrames(
            it->io_buf->data(), result,
            it->io_buf->data(), it->io_buf->BytesRemaining());
        if (rv < 0) {
          Shut(rv);
          return;
        }
        if (rv > 0 || phase_ == PHASE_SHUT) {
          net::CompletionCallback cb = it->callback;
          pending_reqs_.erase(it);
          if (!cb.is_null())
            cb.Run(rv);
        }
        break;
      }
      case PHASE_NYMPH:
      default: {
        NOTREACHED();
        Shut(net::ERR_UNEXPECTED);
        break;
      }
    }
    ConsiderTransportRead();
  }

  void OnWrite(int result) {
    if (!is_transport_write_pending_) {
      NOTREACHED();
      Shut(net::ERR_UNEXPECTED);
      return;
    }
    is_transport_write_pending_ = false;

    if (result < 0) {
      Shut(result);
      return;
    }

    std::deque<PendingReq>::iterator it = GetPendingReq(PendingReq::TYPE_WRITE);
    if (it == pending_reqs_.end() ||
        it->io_buf == NULL ||
        it->io_buf->data() == NULL) {
      NOTREACHED();
      Shut(net::ERR_UNEXPECTED);
      return;
    }
    DCHECK_LE(result, it->io_buf->BytesRemaining());
    it->io_buf->DidConsume(result);
    if (it->io_buf->BytesRemaining() == 0) {
      net::CompletionCallback cb = it->callback;
      int bytes_written = it->io_buf->BytesConsumed();
      DCHECK_GT(bytes_written, 0);
      pending_reqs_.erase(it);
      if (!cb.is_null())
        cb.Run(bytes_written);
    }
    ConsiderTransportWrite();
  }

  // Returns (positive) number of consumed bytes on success.
  // Returns ERR_IO_PENDING in case of incomplete input.
  // Returns ERR_WS_PROTOCOL_ERROR or ERR_LIMIT_VIOLATION in case of failure to
  // reasonably parse input.
  int ProcessHandshake() {
    static const char kGetPrefix[] = "GET ";
    static const char kKeyValueDelimiter[] = ": ";

    class Fields {
     public:
      bool Has(const std::string& name) {
        return map_.find(StringToLowerASCII(name)) != map_.end();
      }

      std::string Get(const std::string& name) {
        return Has(name) ? map_[StringToLowerASCII(name)] : std::string();
      }

      void Set(const std::string& name, const std::string& value) {
        map_[StringToLowerASCII(name)] = StringToLowerASCII(value);
      }

     private:
      std::map<std::string, std::string> map_;
    } fields;

    char* buf = process_handshake_buf_->data();
    size_t buf_size = fill_handshake_buf_->BytesConsumed();

    if (buf_size < 1)
      return net::ERR_IO_PENDING;
    if (!std::equal(buf, buf + std::min(buf_size, strlen(kGetPrefix)),
                    kGetPrefix)) {
      // Data head does not match what is expected.
      return net::ERR_WS_PROTOCOL_ERROR;
    }
    if (buf_size >= kHandshakeLimitBytes)
      return net::ERR_LIMIT_VIOLATION;
    char* buf_end = buf + buf_size;

    if (buf_size < strlen(kGetPrefix))
      return net::ERR_IO_PENDING;
    char* resource_begin = buf + strlen(kGetPrefix);
    char* resource_end = std::find(resource_begin, buf_end, kSpaceOctet);
    if (resource_end == buf_end)
      return net::ERR_IO_PENDING;
    std::string resource(resource_begin, resource_end);
    if (!IsStringUTF8(resource) ||
        resource.find_first_of(kCRLF) != std::string::npos) {
      return net::ERR_WS_PROTOCOL_ERROR;
    }
    char* term_pos = std::search(
        buf, buf_end, kCRLFCRLF, kCRLFCRLF + strlen(kCRLFCRLF));
    char key3[8];  // Notation (key3) matches websocket RFC.
    size_t message_len = buf_end - term_pos;
    if (message_len < sizeof(key3) + strlen(kCRLFCRLF))
      return net::ERR_IO_PENDING;
    term_pos += strlen(kCRLFCRLF);
    memcpy(key3, term_pos, sizeof(key3));
    term_pos += sizeof(key3);
    // First line is "GET resource" line, so skip it.
    char* pos = std::search(buf, term_pos, kCRLF, kCRLF + strlen(kCRLF));
    if (pos == term_pos)
      return net::ERR_WS_PROTOCOL_ERROR;
    for (;;) {
      pos += strlen(kCRLF);
      if (term_pos - pos <
          static_cast<ptrdiff_t>(sizeof(key3) + strlen(kCRLF))) {
        return net::ERR_WS_PROTOCOL_ERROR;
      }
      if (term_pos - pos ==
          static_cast<ptrdiff_t>(sizeof(key3) + strlen(kCRLF))) {
        break;
      }
      char* next_pos = std::search(
          pos, term_pos, kKeyValueDelimiter,
          kKeyValueDelimiter + strlen(kKeyValueDelimiter));
      if (next_pos == term_pos)
        return net::ERR_WS_PROTOCOL_ERROR;
      std::string key(pos, next_pos);
      if (!IsStringASCII(key) ||
          key.find_first_of(kCRLF) != std::string::npos) {
        return net::ERR_WS_PROTOCOL_ERROR;
      }
      pos = std::search(next_pos += strlen(kKeyValueDelimiter), term_pos,
                        kCRLF, kCRLF + strlen(kCRLF));
      if (pos == term_pos)
        return net::ERR_WS_PROTOCOL_ERROR;
      if (!key.empty()) {
        std::string value(next_pos, pos);
        if (!IsStringASCII(value) ||
            value.find_first_of(kCRLF) != std::string::npos) {
          return net::ERR_WS_PROTOCOL_ERROR;
        }
        fields.Set(key, value);
      }
    }

    // Values of Upgrade and Connection fields are hardcoded in the protocol.
    if (fields.Get("Upgrade") != "websocket" ||
        fields.Get("Connection") != "upgrade") {
      return net::ERR_WS_PROTOCOL_ERROR;
    }
    if (fields.Has(kVersionFieldName)) {
      NOTIMPLEMENTED();  // new protocol.
      return net::ERR_NOT_IMPLEMENTED;
    }

    if (!fields.Has(kPlainOriginFieldName))
      return net::ERR_CONNECTION_REFUSED;
    // Normalize (e.g. w.r.t. leading slashes) origin.
    GURL origin = GURL(fields.Get(kPlainOriginFieldName)).GetOrigin();
    if (!origin.is_valid())
      return net::ERR_WS_PROTOCOL_ERROR;
    std::string normalized_origin = origin.spec();

    if (!fields.Has(kPlainHostFieldName))
      return net::ERR_CONNECTION_REFUSED;

    std::vector<std::string> subprotocol_list;
    if (fields.Has(kProtocolFieldName)) {
      int rv = FetchSubprotocolList(
          fields.Get(kProtocolFieldName), &subprotocol_list);
      if (rv < 0)
        return rv;
      DCHECK(subprotocol_list.end() == std::find(
          subprotocol_list.begin(), subprotocol_list.end(), ""));
    }

    std::string location;
    std::string subprotocol;
    if (!delegate_->ValidateWebSocket(resource,
                                      normalized_origin,
                                      fields.Get(kPlainHostFieldName),
                                      subprotocol_list,
                                      &location,
                                      &subprotocol)) {
      return net::ERR_CONNECTION_REFUSED;
    }
    if (subprotocol_list.empty()) {
      DCHECK(subprotocol.empty());
    } else {
      if (!subprotocol.empty()) {
        if (subprotocol_list.end() == std::find(
            subprotocol_list.begin(), subprotocol_list.end(), subprotocol)) {
          NOTREACHED() << "delegate must pick subprotocol from given list";
          return net::ERR_UNEXPECTED;
        }
      }
    }

    uint32 key_number1 = 0;
    uint32 key_number2 = 0;
    if (!FetchDecimalDigits(fields.Get(kKey1FieldName), &key_number1) ||
        !FetchDecimalDigits(fields.Get(kKey2FieldName), &key_number2)) {
      return net::ERR_WS_PROTOCOL_ERROR;
    }

    // We limit incoming header size so following numbers shall not be too high.
    int spaces1 = CountSpaces(fields.Get(kKey1FieldName));
    int spaces2 = CountSpaces(fields.Get(kKey2FieldName));
    if (spaces1 == 0 ||
        spaces2 == 0 ||
        key_number1 % spaces1 != 0 ||
        key_number2 % spaces2 != 0) {
      return net::ERR_WS_PROTOCOL_ERROR;
    }

    char challenge[4 + 4 + sizeof(key3)];
    int32 part1 = base::HostToNet32(key_number1 / spaces1);
    int32 part2 = base::HostToNet32(key_number2 / spaces2);
    memcpy(challenge, &part1, 4);
    memcpy(challenge + 4, &part2, 4);
    memcpy(challenge + 4 + 4, key3, sizeof(key3));
    base::MD5Digest challenge_response;
    base::MD5Sum(challenge, sizeof(challenge), &challenge_response);

    // Concocting response handshake.
    class Buffer {
     public:
      Buffer()
          : io_buf_(new net::IOBuffer(kHandshakeLimitBytes)),
            bytes_written_(0),
            is_ok_(true) {
      }

      bool Write(const void* p, int len) {
        DCHECK(p);
        DCHECK_GE(len, 0);
        if (!is_ok_)
          return false;
        if (bytes_written_ + len > kHandshakeLimitBytes) {
          NOTREACHED();
          is_ok_ = false;
          return false;
        }
        memcpy(io_buf_->data() + bytes_written_, p, len);
        bytes_written_ += len;
        return true;
      }

      bool WriteLine(const char* p) {
        return Write(p, strlen(p)) && Write(kCRLF, strlen(kCRLF));
      }

      operator net::DrainableIOBuffer*() {
        return new net::DrainableIOBuffer(io_buf_.get(), bytes_written_);
      }

      bool is_ok() { return is_ok_; }

     private:
      scoped_refptr<net::IOBuffer> io_buf_;
      size_t bytes_written_;
      bool is_ok_;
    } buffer;

    buffer.WriteLine("HTTP/1.1 101 WebSocket Protocol Handshake");
    buffer.WriteLine("Upgrade: WebSocket");
    buffer.WriteLine("Connection: Upgrade");

    {
      // Take care of Location field.
      char tmp[2048];
      int rv = base::snprintf(tmp, sizeof(tmp),
                              "%s: %s",
                              kLocationFieldName,
                              location.c_str());
      if (rv <= 0 || rv + 0u >= sizeof(tmp))
        return net::ERR_LIMIT_VIOLATION;
      buffer.WriteLine(tmp);
    }
    {
      // Take care of Origin field.
      char tmp[2048];
      int rv = base::snprintf(tmp, sizeof(tmp),
                              "%s: %s",
                              kOriginFieldName,
                              fields.Get(kPlainOriginFieldName).c_str());
      if (rv <= 0 || rv + 0u >= sizeof(tmp))
        return net::ERR_LIMIT_VIOLATION;
      buffer.WriteLine(tmp);
    }
    if (!subprotocol.empty()) {
      char tmp[2048];
      int rv = base::snprintf(tmp, sizeof(tmp),
                              "%s: %s",
                              kProtocolFieldName,
                              subprotocol.c_str());
      if (rv <= 0 || rv + 0u >= sizeof(tmp))
        return net::ERR_LIMIT_VIOLATION;
      buffer.WriteLine(tmp);
    }
    buffer.WriteLine("");
    buffer.Write(&challenge_response, sizeof(challenge_response));

    if (!buffer.is_ok())
      return net::ERR_LIMIT_VIOLATION;

    pending_reqs_.push_back(PendingReq(
        PendingReq::TYPE_WRITE_METADATA, buffer, net::CompletionCallback()));
    DCHECK_GT(term_pos - buf, 0);
    return term_pos - buf;
  }

  // Removes frame delimiters and returns net number of data bytes (or error).
  // |out| may be equal to |buf|, in that case it is in-place operation.
  int ProcessDataFrames(char* buf, int buf_len, char* out, int out_len) {
    if (out_len < buf_len) {
      NOTREACHED();
      return net::ERR_UNEXPECTED;
    }
    int out_pos = 0;
    for (char* p = buf; p < buf + buf_len; ++p) {
      switch (phase_) {
        case PHASE_FRAME_INSIDE: {
          if (*p == '\x00')
            return net::ERR_WS_PROTOCOL_ERROR;
          if (*p == '\xff')
            phase_ = PHASE_FRAME_OUTSIDE;
          else
            out[out_pos++] = *p;
          break;
        }
        case PHASE_FRAME_OUTSIDE: {
          if (*p == '\x00') {
            phase_ = PHASE_FRAME_INSIDE;
          } else if (*p == '\xff') {
            phase_ = PHASE_FRAME_LENGTH;
            frame_bytes_remaining_ = 0;
          }
          else {
            return net::ERR_WS_PROTOCOL_ERROR;
          }
          break;
        }
        case PHASE_FRAME_LENGTH: {
          static const int kValueBits = 7;
          static const char kValueMask = (1 << kValueBits) - 1;
          frame_bytes_remaining_ <<= kValueBits;
          frame_bytes_remaining_ += (*p & kValueMask);
          if (*p & ~kValueMask) {
            // Check that next byte would not overflow.
            if (frame_bytes_remaining_ >
                (std::numeric_limits<int>::max() - ((1 << 7) - 1)) >> 7) {
              return net::ERR_LIMIT_VIOLATION;
            }
          } else {
            if (frame_bytes_remaining_ == 0) {
              phase_ = PHASE_SHUT;
              return out_pos;
            } else {
              phase_ = PHASE_FRAME_SKIP;
            }
          }
          break;
        }
        case PHASE_FRAME_SKIP: {
          DCHECK_GE(frame_bytes_remaining_, 1);
          frame_bytes_remaining_ -= 1;
          if (frame_bytes_remaining_ < 1)
            phase_ = PHASE_FRAME_OUTSIDE;
          break;
        }
        default: {
          NOTREACHED();
        }
      }
    }
    return out_pos;
  }

  // State machinery.
  Phase phase_;

  // Counts frame length for PHASE_FRAME_LENGTH and PHASE_FRAME_SKIP.
  int frame_bytes_remaining_;

  // Underlying socket.
  scoped_ptr<net::Socket> transport_socket_;

  // Validation is performed via delegate.
  Delegate* delegate_;

  // IOBuffer used to communicate with transport at initial stage.
  scoped_refptr<net::IOBuffer> handshake_buf_;
  scoped_refptr<net::DrainableIOBuffer> fill_handshake_buf_;
  scoped_refptr<net::DrainableIOBuffer> process_handshake_buf_;

  // Pending IO requests we need to complete.
  std::deque<PendingReq> pending_reqs_;

  // Whether transport requests are pending.
  bool is_transport_read_pending_;
  bool is_transport_write_pending_;

  base::WeakPtrFactory<WebSocketServerSocketImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketServerSocketImpl);
};

}  // namespace

namespace net {

WebSocketServerSocket* CreateWebSocketServerSocket(
    Socket* transport_socket, WebSocketServerSocket::Delegate* delegate) {
  return new WebSocketServerSocketImpl(transport_socket, delegate);
}

WebSocketServerSocket::~WebSocketServerSocket() {
}

}  // namespace net;
