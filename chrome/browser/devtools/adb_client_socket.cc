// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/adb_client_socket.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "net/base/net_errors.h"

namespace {

const int kBufferSize = 16 * 1024;

std::string EncodeLength(size_t length) {
  static const char kHexChars[] = "0123456789ABCDEF";

  std::string result(4, '\0');
  char b = reinterpret_cast<const char*>(&length)[1];
  result[0] = kHexChars[(b >> 4) & 0xf];
  result[1] = kHexChars[b & 0xf];
  b = reinterpret_cast<const char*>(&length)[0];
  result[2] = kHexChars[(b >> 4) & 0xf];
  result[3] = kHexChars[b & 0xf];
  return result;
}

}  // namespace

// static
void ADBClientSocket::Query(int port,
                            const std::string& query,
                            const Callback& callback) {
  (new ADBClientSocket())->InnerQuery(port, query, callback);
}

ADBClientSocket::ADBClientSocket() : expected_response_length_(-1) {
}

ADBClientSocket::~ADBClientSocket() {
}

void ADBClientSocket::InnerQuery(int port,
                                 const std::string& query,
                                 const Callback& callback) {
  if (query.length() > 0xFFFF) {
    ReportErrorAndDie("Input message is too big");
    return;
  }
  callback_ = callback;

  net::IPAddressNumber ip_number;
  if (!net::ParseIPLiteralToNumber("127.0.0.1", &ip_number)) {
    ReportErrorAndDie("Could not connect to ADB");
    return;
  }

  net::AddressList address_list =
      net::AddressList::CreateFromIPAddress(ip_number, port);
  socket_.reset(new net::TCPClientSocket(address_list, NULL,
                                         net::NetLog::Source()));
  std::string message = EncodeLength(query.length()) + query;
  scoped_refptr<net::StringIOBuffer> request_buffer =
      new net::StringIOBuffer(message);
  int result = socket_->Connect(base::Bind(&ADBClientSocket::OnConnectComplete,
                                           base::Unretained(this),
                                           request_buffer));
  if (result != net::ERR_IO_PENDING)
    ReportErrorAndDie("Could not connect to ADB");
}

void ADBClientSocket::OnConnectComplete(
    scoped_refptr<net::StringIOBuffer> request_buffer,
    int result) {
  if (!CheckNetResultOrDie(result))
    return;
  result = socket_->Write(request_buffer, request_buffer->size(),
      base::Bind(&ADBClientSocket::OnWriteComplete, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING)
    OnWriteComplete(result);
}

void ADBClientSocket::OnWriteComplete(int result) {
  if (!CheckNetResultOrDie(result))
    return;
  scoped_refptr<net::IOBuffer> response_buffer =
      new net::IOBuffer(kBufferSize);
  result = socket_->Read(response_buffer, kBufferSize,
      base::Bind(&ADBClientSocket::OnReadComplete, base::Unretained(this),
                 response_buffer));
  if (result != net::ERR_IO_PENDING)
    OnReadComplete(response_buffer, result);
}

void ADBClientSocket::OnReadComplete(
    scoped_refptr<net::IOBuffer> response_buffer,
    int result) {
  if (!CheckNetResultOrDie(result))
    return;

  response_ += std::string(response_buffer->data(), result);
  if (expected_response_length_ == -1) {
    // Reading header
    if (result < 8) {
      ReportErrorAndDie("Response is too short: " + response_);
      return;
    }

    std::string status = response_.substr(0, 4);
    if (status != "OKAY" && status != "FAIL") {
      ReportInvalidResponseAndDie();
      return;
    }
    std::string payload_length = response_.substr(4, 4);
    if (!base::HexStringToInt(response_.substr(4, 4),
        &expected_response_length_)) {
      ReportInvalidResponseAndDie();
      return;
    }
  }

  if (static_cast<int>(response_.length() - 8) == expected_response_length_) {
    ReportSuccessAndDie();
    return;
  }

  // Read tail
  result = socket_->Read(response_buffer, kBufferSize,
      base::Bind(&ADBClientSocket::OnReadComplete, base::Unretained(this),
                 response_buffer));
  if (result != net::ERR_IO_PENDING)
    OnReadComplete(response_buffer, result);
}

bool ADBClientSocket::CheckNetResultOrDie(int result) {
  if (result >= 0)
    return true;
  ReportErrorAndDie(base::StringPrintf("Internal error %d", result));
  return false;
}

void ADBClientSocket::ReportSuccessAndDie() {
  callback_.Run(std::string(), response_.substr(8));
  Destroy();
}

void ADBClientSocket::ReportInvalidResponseAndDie() {
  callback_.Run("Invalid response: " + response_, std::string());
  Destroy();
}

void ADBClientSocket::ReportErrorAndDie(const std::string& error) {
  callback_.Run(error, std::string());
  Destroy();
}

void ADBClientSocket::Destroy() {
  delete this;
}
