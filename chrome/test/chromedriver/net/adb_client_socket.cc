// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/net/adb_client_socket.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "net/base/address_list.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "third_party/blink/public/public_buildflags.h"

namespace {

const int kBufferSize = 16 * 1024;
const size_t kAdbDataChunkSize = 32 * 1024;
const char kOkayResponse[] = "OKAY";
const char kHostTransportCommand[] = "host:transport:%s";
const char kLocalAbstractCommand[] = "localabstract:%s";
const char kSyncCommand[] = "sync:";
const char kSendCommand[] = "SEND";
const char kDataCommand[] = "DATA";
const char kDoneCommand[] = "DONE";

typedef base::Callback<void(int, const std::string&)> CommandCallback;
typedef base::Callback<void(int, net::StreamSocket*)> SocketCallback;

std::string EncodeMessage(const std::string& message) {
  static const char kHexChars[] = "0123456789ABCDEF";

  size_t length = message.length();
  std::string result(4, '\0');
  char b = reinterpret_cast<const char*>(&length)[1];
  result[0] = kHexChars[(b >> 4) & 0xf];
  result[1] = kHexChars[b & 0xf];
  b = reinterpret_cast<const char*>(&length)[0];
  result[2] = kHexChars[(b >> 4) & 0xf];
  result[3] = kHexChars[b & 0xf];
  return result + message;
}

class AdbTransportSocket : public AdbClientSocket {
 public:
  AdbTransportSocket(int port,
                     const std::string& serial,
                     const std::string& socket_name,
                     const SocketCallback& callback)
    : AdbClientSocket(port),
      serial_(serial),
      socket_name_(socket_name),
      callback_(callback) {
    Connect(base::Bind(&AdbTransportSocket::OnConnected,
                       base::Unretained(this)));
  }

 private:
  ~AdbTransportSocket() {}

  void OnConnected(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    SendCommand(base::StringPrintf(kHostTransportCommand, serial_.c_str()),
        true, true, base::Bind(&AdbTransportSocket::SendLocalAbstract,
                         base::Unretained(this)));
  }

  void SendLocalAbstract(int result, const std::string& response) {
    if (!CheckNetResultOrDie(result))
      return;
    SendCommand(base::StringPrintf(kLocalAbstractCommand, socket_name_.c_str()),
        true, true, base::Bind(&AdbTransportSocket::OnSocketAvailable,
                         base::Unretained(this)));
  }

  void OnSocketAvailable(int result, const std::string& response) {
    if (!CheckNetResultOrDie(result))
      return;
    callback_.Run(net::OK, socket_.release());
    delete this;
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    callback_.Run(result, NULL);
    delete this;
    return false;
  }

  std::string serial_;
  std::string socket_name_;
  SocketCallback callback_;
};

class HttpOverAdbSocket {
 public:
  HttpOverAdbSocket(int port,
                    const std::string& serial,
                    const std::string& socket_name,
                    const std::string& request,
                    const CommandCallback& callback)
    : request_(request),
      command_callback_(callback),
      body_pos_(0) {
    Connect(port, serial, socket_name);
  }

  HttpOverAdbSocket(int port,
                    const std::string& serial,
                    const std::string& socket_name,
                    const std::string& request,
                    const SocketCallback& callback)
    : request_(request),
      socket_callback_(callback),
      body_pos_(0) {
    Connect(port, serial, socket_name);
  }

 private:
  ~HttpOverAdbSocket() {
  }

  void Connect(int port,
               const std::string& serial,
               const std::string& socket_name) {
    AdbClientSocket::TransportQuery(
        port, serial, socket_name,
        base::Bind(&HttpOverAdbSocket::OnSocketAvailable,
                   base::Unretained(this)));
  }

  void OnSocketAvailable(int result,
                         net::StreamSocket* socket) {
    if (!CheckNetResultOrDie(result))
      return;

    socket_.reset(socket);

    scoped_refptr<net::StringIOBuffer> request_buffer =
        new net::StringIOBuffer(request_);

    result = socket_->Write(
        request_buffer.get(), request_buffer->size(),
        base::Bind(&HttpOverAdbSocket::ReadResponse, base::Unretained(this)),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    if (result != net::ERR_IO_PENDING)
      ReadResponse(result);
  }

  void ReadResponse(int result) {
    if (!CheckNetResultOrDie(result))
      return;

    scoped_refptr<net::IOBuffer> response_buffer =
        new net::IOBuffer(kBufferSize);

    result = socket_->Read(response_buffer.get(),
                           kBufferSize,
                           base::Bind(&HttpOverAdbSocket::OnResponseData,
                                      base::Unretained(this),
                                      response_buffer,
                                      -1));
    if (result != net::ERR_IO_PENDING)
      OnResponseData(response_buffer, -1, result);
  }

  void OnResponseData(scoped_refptr<net::IOBuffer> response_buffer,
                      int bytes_total,
                      int result) {
    if (!CheckNetResultOrDie(result))
      return;
    if (result == 0) {
      CheckNetResultOrDie(net::ERR_CONNECTION_CLOSED);
      return;
    }

    response_ += std::string(response_buffer->data(), result);
    int expected_length = 0;
    if (bytes_total < 0) {
      size_t content_pos = response_.find("Content-Length:");
      if (content_pos != std::string::npos) {
        size_t endline_pos = response_.find("\n", content_pos);
        if (endline_pos != std::string::npos) {
          std::string len = response_.substr(content_pos + 15,
                                             endline_pos - content_pos - 15);
          base::TrimWhitespaceASCII(len, base::TRIM_ALL, &len);
          if (!base::StringToInt(len, &expected_length)) {
            CheckNetResultOrDie(net::ERR_FAILED);
            return;
          }
        }
      }

      body_pos_ = response_.find("\r\n\r\n");
      if (body_pos_ != std::string::npos) {
        body_pos_ += 4;
        bytes_total = body_pos_ + expected_length;
      }
    }

    if (bytes_total == static_cast<int>(response_.length())) {
      if (!command_callback_.is_null())
        command_callback_.Run(body_pos_, response_);
      else
        socket_callback_.Run(net::OK, socket_.release());
      delete this;
      return;
    }

    result = socket_->Read(response_buffer.get(),
                           kBufferSize,
                           base::Bind(&HttpOverAdbSocket::OnResponseData,
                                      base::Unretained(this),
                                      response_buffer,
                                      bytes_total));
    if (result != net::ERR_IO_PENDING)
      OnResponseData(response_buffer, bytes_total, result);
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    if (!command_callback_.is_null())
      command_callback_.Run(result, std::string());
    else
      socket_callback_.Run(result, NULL);
    delete this;
    return false;
  }

  std::unique_ptr<net::StreamSocket> socket_;
  std::string request_;
  std::string response_;
  CommandCallback command_callback_;
  SocketCallback socket_callback_;
  size_t body_pos_;
};

class AdbQuerySocket : AdbClientSocket {
 public:
  AdbQuerySocket(int port,
                 const std::string& query,
                 const CommandCallback& callback)
      : AdbClientSocket(port),
        current_query_(0),
        callback_(callback) {
    queries_ = base::SplitString(
        query, "|", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (queries_.empty()) {
      CheckNetResultOrDie(net::ERR_INVALID_ARGUMENT);
      return;
    }
    Connect(base::Bind(&AdbQuerySocket::SendNextQuery, base::Unretained(this)));
  }

 private:
  ~AdbQuerySocket() {
  }

  void SendNextQuery(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    std::string query = queries_[current_query_];
    if (query.length() > 0xFFFF) {
      CheckNetResultOrDie(net::ERR_MSG_TOO_BIG);
      return;
    }
    bool is_void = current_query_ < queries_.size() - 1;
    // The |shell| command is a special case because it is the only command that
    // doesn't include a length at the beginning of the data stream.
    bool has_length =
        !base::StartsWith(query, "shell:", base::CompareCase::SENSITIVE);
    SendCommand(query, is_void, has_length,
        base::Bind(&AdbQuerySocket::OnResponse, base::Unretained(this)));
  }

  void OnResponse(int result, const std::string& response) {
    if (++current_query_ < queries_.size()) {
      SendNextQuery(net::OK);
    } else {
      callback_.Run(result, response);
      delete this;
    }
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    callback_.Run(result, std::string());
    delete this;
    return false;
  }

  std::vector<std::string> queries_;
  size_t current_query_;
  CommandCallback callback_;
};

// Implement the ADB protocol to send a file to the device.
// The protocol consists of the following steps:
// * Send "host:transport" command with device serial
// * Send "sync:" command to initialize file transfer
// * Send "SEND" command with name and mode of the file
// * Send "DATA" command one or more times for the file content
// * Send "DONE" command to indicate end of file transfer
// The first two commands use normal ADB command format implemented by
// AdbClientSocket::SendCommand. The remaining commands use a special
// format implemented by AdbSendFileSocket::SendPayload.
class AdbSendFileSocket : AdbClientSocket {
 public:
  AdbSendFileSocket(int port,
                    const std::string& serial,
                    const std::string& filename,
                    const std::string& content,
                    const CommandCallback& callback)
      : AdbClientSocket(port),
        serial_(serial),
        filename_(filename),
        content_(content),
        current_offset_(0),
        callback_(callback) {
    Connect(
        base::Bind(&AdbSendFileSocket::SendTransport, base::Unretained(this)));
  }

 private:
  ~AdbSendFileSocket() {}

  void SendTransport(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    SendCommand(
        base::StringPrintf(kHostTransportCommand, serial_.c_str()), true, true,
        base::Bind(&AdbSendFileSocket::SendSync, base::Unretained(this)));
  }

  void SendSync(int result, const std::string& response) {
    if (!CheckNetResultOrDie(result))
      return;
    SendCommand(
        kSyncCommand, true, true,
        base::Bind(&AdbSendFileSocket::SendSend, base::Unretained(this)));
  }

  void SendSend(int result, const std::string& response) {
    if (!CheckNetResultOrDie(result))
      return;
    // File mode. The following value is equivalent to S_IRUSR | S_IWUSR.
    // Can't use the symbolic names since they are not available on Windows.
    int mode = 0600;
    std::string payload = base::StringPrintf("%s,%d", filename_.c_str(), mode);
    SendPayload(
        kSendCommand, payload.length(), payload.c_str(), payload.length(),
        base::Bind(&AdbSendFileSocket::SendContent, base::Unretained(this)));
  }

  void SendContent(int result) {
    if (!CheckNetResultOrDie(result))
      return;
    if (current_offset_ >= content_.length()) {
      SendDone();
      return;
    }
    size_t offset = current_offset_;
    size_t length = std::min(content_.length() - offset, kAdbDataChunkSize);
    current_offset_ += length;
    SendPayload(
        kDataCommand, length, content_.c_str() + offset, length,
        base::Bind(&AdbSendFileSocket::SendContent, base::Unretained(this)));
  }

  void SendDone() {
    int data = time(NULL);
    SendPayload(kDoneCommand, data, nullptr, 0,
                base::Bind(&AdbSendFileSocket::ReadFinalResponse,
                           base::Unretained(this)));
  }

  void ReadFinalResponse(int result) {
    ReadResponse(callback_, true, false, result);
  }

  // Send a special payload command ("SEND", "DATA", or "DONE").
  // Each command consists of a command line, followed by a 4-byte integer
  // sent in raw little-endian format, followed by an optional payload.
  void SendPayload(const char* command,
                   int data,
                   const char* payload,
                   size_t payloadLength,
                   const net::CompletionCallback& callback) {
    std::string buffer(command);
    for (int i = 0; i < 4; i++) {
      buffer.append(1, static_cast<char>(data & 0xff));
      data >>= 8;
    }
    if (payloadLength > 0)
      buffer.append(payload, payloadLength);

    scoped_refptr<net::StringIOBuffer> request_buffer =
        new net::StringIOBuffer(buffer);
    int result = socket_->Write(request_buffer.get(), request_buffer->size(),
                                callback, TRAFFIC_ANNOTATION_FOR_TESTS);
    if (result != net::ERR_IO_PENDING)
      callback.Run(result);
  }

  bool CheckNetResultOrDie(int result) {
    if (result >= 0)
      return true;
    callback_.Run(result, NULL);
    delete this;
    return false;
  }

  std::string serial_;
  std::string filename_;
  std::string content_;
  size_t current_offset_;
  CommandCallback callback_;
};

}  // namespace

// static
void AdbClientSocket::AdbQuery(int port,
                               const std::string& query,
                               const CommandCallback& callback) {
  new AdbQuerySocket(port, query, callback);
}

// static
void AdbClientSocket::SendFile(int port,
                               const std::string& serial,
                               const std::string& filename,
                               const std::string& content,
                               const CommandCallback& callback) {
  new AdbSendFileSocket(port, serial, filename, content, callback);
}

#if BUILDFLAG(DEBUG_DEVTOOLS)
static void UseTransportQueryForDesktop(const SocketCallback& callback,
                                        net::StreamSocket* socket,
                                        int result) {
  callback.Run(result, socket);
}
#endif  // BUILDFLAG(DEBUG_DEVTOOLS)

// static
void AdbClientSocket::TransportQuery(int port,
                                     const std::string& serial,
                                     const std::string& socket_name,
                                     const SocketCallback& callback) {
#if BUILDFLAG(DEBUG_DEVTOOLS)
  if (serial.empty()) {
    // Use plain socket for remote debugging on Desktop (debugging purposes).
    int tcp_port = 0;
    if (!base::StringToInt(socket_name, &tcp_port))
      tcp_port = 9222;

    net::AddressList address_list = net::AddressList::CreateFromIPAddress(
        net::IPAddress::IPv4Localhost(), tcp_port);
    net::TCPClientSocket* socket = new net::TCPClientSocket(
        address_list, nullptr, nullptr, net::NetLogSource());
    socket->Connect(base::Bind(&UseTransportQueryForDesktop, callback, socket));
    return;
  }
#endif  // BUILDFLAG(DEBUG_DEVTOOLS)
  new AdbTransportSocket(port, serial, socket_name, callback);
}

// static
void AdbClientSocket::HttpQuery(int port,
                                const std::string& serial,
                                const std::string& socket_name,
                                const std::string& request_path,
                                const CommandCallback& callback) {
  new HttpOverAdbSocket(port, serial, socket_name, request_path,
      callback);
}

// static
void AdbClientSocket::HttpQuery(int port,
                                const std::string& serial,
                                const std::string& socket_name,
                                const std::string& request_path,
                                const SocketCallback& callback) {
  new HttpOverAdbSocket(port, serial, socket_name, request_path,
      callback);
}

AdbClientSocket::AdbClientSocket(int port) : port_(port) {}

AdbClientSocket::~AdbClientSocket() {
}

void AdbClientSocket::Connect(const net::CompletionCallback& callback) {
  // In a IPv4/IPv6 dual stack environment, getaddrinfo for localhost could
  // only return IPv6 address while current adb (1.0.36) will always listen
  // on IPv4. So just try IPv4 first, then fall back to IPv6.
  net::IPAddressList list = {net::IPAddress::IPv4Localhost(),
                             net::IPAddress::IPv6Localhost()};
  net::AddressList ip_list = net::AddressList::CreateFromIPAddressList(
      list, "localhost");
  net::AddressList address_list = net::AddressList::CopyWithPort(
      ip_list, port_);

  socket_.reset(new net::TCPClientSocket(address_list, NULL, NULL,
                                         net::NetLogSource()));
  int result = socket_->Connect(callback);
  if (result != net::ERR_IO_PENDING)
    callback.Run(result);
}

void AdbClientSocket::SendCommand(const std::string& command,
                                  bool is_void,
                                  bool has_length,
                                  const CommandCallback& callback) {
  scoped_refptr<net::StringIOBuffer> request_buffer =
      new net::StringIOBuffer(EncodeMessage(command));
  int result = socket_->Write(
      request_buffer.get(), request_buffer->size(),
      base::Bind(&AdbClientSocket::ReadResponse, base::Unretained(this),
                 callback, is_void, has_length),
      TRAFFIC_ANNOTATION_FOR_TESTS);
  if (result != net::ERR_IO_PENDING)
    ReadResponse(callback, is_void, has_length, result);
}

void AdbClientSocket::ReadResponse(const CommandCallback& callback,
                                   bool is_void,
                                   bool has_length,
                                   int result) {
  if (result < 0) {
    callback.Run(result, "IO error");
    return;
  }
  scoped_refptr<net::IOBuffer> response_buffer =
      new net::IOBuffer(kBufferSize);
  result = socket_->Read(response_buffer.get(),
                         kBufferSize,
                         base::Bind(&AdbClientSocket::OnResponseStatus,
                                    base::Unretained(this),
                                    callback,
                                    is_void,
                                    has_length,
                                    response_buffer));
  if (result != net::ERR_IO_PENDING)
    OnResponseStatus(callback, is_void, has_length, response_buffer, result);
}

void AdbClientSocket::OnResponseStatus(
    const CommandCallback& callback,
    bool is_void,
    bool has_length,
    scoped_refptr<net::IOBuffer> response_buffer,
    int result) {
  if (result <= 0) {
    callback.Run(result == 0 ? net::ERR_CONNECTION_CLOSED : result,
                 "IO error");
    return;
  }

  std::string data = std::string(response_buffer->data(), result);
  if (result < 4) {
    callback.Run(net::ERR_FAILED, "Response is too short: " + data);
    return;
  }

  std::string status = data.substr(0, 4);
  if (status != kOkayResponse) {
    callback.Run(net::ERR_FAILED, data);
    return;
  }

  data = data.substr(4);

  if (!is_void) {
    if (!has_length) {
      // Payload doesn't include length, so skip straight to reading in data.
      OnResponseData(callback, data, response_buffer, -1, 0);
    } else if (data.length() >= 4) {
      // We've already read the length out of the socket, so we don't need to
      // read more yet.
      OnResponseLength(callback, data, response_buffer, 0);
    } else {
      // Part or all of the length is still in the socket, so we need to read it
      // out of the socket before parsing the length.
      result = socket_->Read(response_buffer.get(),
                             kBufferSize,
                             base::Bind(&AdbClientSocket::OnResponseLength,
                                        base::Unretained(this),
                                        callback,
                                        data,
                                        response_buffer));
      if (result != net::ERR_IO_PENDING)
        OnResponseLength(callback, data, response_buffer, result);
    }
  } else {
    callback.Run(net::OK, data);
  }
}

void AdbClientSocket::OnResponseLength(
    const CommandCallback& callback,
    const std::string& response,
    scoped_refptr<net::IOBuffer> response_buffer,
    int result) {
  if (result < 0) {
    callback.Run(result, "IO error");
    return;
  }

  std::string new_response =
      response + std::string(response_buffer->data(), result);
  // Sometimes ADB server will respond like "OKAYOKAY<payload_length><payload>"
  // instead of the expected "OKAY<payload_length><payload>".
  // For the former case, the first OKAY is cropped off in the OnResponseStatus
  // but we still need to crop the second OKAY.
  if (new_response.substr(0, 4) == "OKAY") {
    VLOG(3) << "cutting new_response down to size";
    new_response = new_response.substr(4);
  }
  if (new_response.length() < 4) {
    if (new_response.length() == 0 && result == net::OK) {
      // The socket is shut down and all data has been read.
      // Note that this is a hack because is_void is false,
      // but we are not returning any data. The upstream logic
      // to determine is_void is not in a good state.
      // However, this is a better solution than trusting is_void anyway
      // because otherwise this can become an infinite loop.
      callback.Run(net::OK, new_response);
      return;
    }
    result = socket_->Read(response_buffer.get(),
                           kBufferSize,
                           base::Bind(&AdbClientSocket::OnResponseLength,
                                      base::Unretained(this),
                                      callback,
                                      new_response,
                                      response_buffer));
    if (result != net::ERR_IO_PENDING)
      OnResponseLength(callback, new_response, response_buffer, result);
  } else {
    int payload_length = 0;
    if (!base::HexStringToInt(new_response.substr(0, 4), &payload_length)) {
      VLOG(1) << "net error since payload length wasn't readable.";
      callback.Run(net::ERR_FAILED, "response <" + new_response +
                                        "> from adb server was unexpected");
      return;
    }

    new_response = new_response.substr(4);
    int bytes_left = payload_length - new_response.length();
    OnResponseData(callback, new_response, response_buffer, bytes_left, 0);
  }
}

void AdbClientSocket::OnResponseData(
    const CommandCallback& callback,
    const std::string& response,
    scoped_refptr<net::IOBuffer> response_buffer,
    int bytes_left,
    int result) {
  if (result < 0) {
    callback.Run(result, "IO error");
    return;
  }

  bytes_left -= result;
  std::string new_response =
      response + std::string(response_buffer->data(), result);
  if (bytes_left == 0) {
    callback.Run(net::OK, new_response);
    return;
  }

  // Read tail
  result = socket_->Read(response_buffer.get(),
                         kBufferSize,
                         base::Bind(&AdbClientSocket::OnResponseData,
                                    base::Unretained(this),
                                    callback,
                                    new_response,
                                    response_buffer,
                                    bytes_left));
  if (result > 0)
    OnResponseData(callback, new_response, response_buffer, bytes_left, result);
  else if (result != net::ERR_IO_PENDING)
    callback.Run(net::OK, new_response);
}
