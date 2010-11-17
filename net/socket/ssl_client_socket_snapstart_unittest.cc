// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <sslt.h>

#include <vector>
#include <string>

#include "base/eintr_wrapper.h"
#include "base/base64.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/values.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_log.h"
#include "net/base/net_log_unittest.h"
#include "net/base/ssl_config_service.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"
#include "net/socket/ssl_client_socket_nss.h"
#include "net/socket/ssl_host_info.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace net {

// TestSSLHostInfo is an SSLHostInfo which stores a single state in memory and
// pretends that certificate verification always succeeds.
class TestSSLHostInfo : public SSLHostInfo {
 public:
  TestSSLHostInfo()
      : SSLHostInfo("example.com", kDefaultSSLConfig) {
    if (!saved_.empty())
      Parse(saved_);
    cert_verification_complete_ = true;
    cert_verification_error_ = OK;
  }

  virtual void Start() {
  }

  virtual int WaitForDataReady(CompletionCallback*) { return OK; }

  virtual void Persist() {
    saved_ = Serialize();
  }

  static void Reset() {
    saved_.clear();
  }

 private:
  static SSLConfig kDefaultSSLConfig;
  static std::string saved_;
};

std::string TestSSLHostInfo::saved_;

SSLConfig TestSSLHostInfo::kDefaultSSLConfig;

class SSLClientSocketSnapStartTest : public PlatformTest {
 public:
  SSLClientSocketSnapStartTest()
      : child_(base::kNullProcessHandle),
        socket_factory_(ClientSocketFactory::GetDefaultFactory()),
        log_(CapturingNetLog::kUnbounded) {
    TestSSLHostInfo::Reset();
    ssl_config_.snap_start_enabled = true;
  }

  virtual void TearDown() {
    if (child_ != base::kNullProcessHandle) {
      int exit_code;
      EXPECT_TRUE(base::WaitForExitCode(child_, &exit_code));
      EXPECT_EQ(0, exit_code);
    }
  }

 protected:
  void StartSnapStartServer(const char* arg, ...) {
    FilePath dir_exe;
    PathService::Get(base::DIR_EXE, &dir_exe);
    FilePath helper_binary = dir_exe.Append("openssl_helper");

    std::vector<std::string> args;
    args.push_back(helper_binary.value());

    va_list ap;
    va_start(ap, arg);
    while (arg) {
      args.push_back(arg);
      arg = va_arg(ap, const char*);
    }
    va_end(ap);

    const int listener = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(listener, 0);
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = PF_INET;
    ASSERT_EQ(0, bind(listener, (struct sockaddr*) &sin, sizeof(sin)));
    socklen_t socklen = sizeof(remote_);
    ASSERT_EQ(0, getsockname(listener, (struct sockaddr*) &remote_, &socklen));
    ASSERT_EQ(sizeof(remote_), socklen);
    ASSERT_EQ(0, listen(listener, 1));

    base::file_handle_mapping_vector mapping;
    // The listening socket is installed as the child's fd 3.
    mapping.push_back(std::make_pair(listener, 3));
    base::LaunchApp(args, mapping, false /* don't wait */, &child_);
    HANDLE_EINTR(close(listener));
  }

  // LoadDefaultCert returns the DER encoded default certificate.
  std::string LoadDefaultCert() {
    FilePath path;
    PathService::Get(base::DIR_SOURCE_ROOT, &path);
    path = path.Append("net");
    path = path.Append("data");
    path = path.Append("ssl");
    path = path.Append("certificates");
    path = path.Append("ok_cert.pem");

    std::string pem;
    bool r = file_util::ReadFileToString(path, &pem);
    CHECK(r) << "failed to read " << path.value();

    static const char kStartMarker[] = "-----BEGIN CERTIFICATE-----\n";
    static const char kEndMarker[] = "-----END CERTIFICATE-----\n";

    std::string::size_type i = pem.find(kStartMarker);
    std::string::size_type j = pem.find(kEndMarker);
    CHECK(i != std::string::npos);
    CHECK(j != std::string::npos);
    CHECK_GT(j, i);
    i += sizeof(kStartMarker) - 1;

    std::string b64data = pem.substr(i, j - i);
    ReplaceSubstringsAfterOffset(&b64data, 0 /* start offset */, "\n", "");

    std::string der;
    base::Base64Decode(b64data, &der);
    return der;
  }

  void SetupSSLConfig() {
    if (ssl_config_.allowed_bad_certs.size())
      return;
    const std::string der = LoadDefaultCert();
    SSLConfig::CertAndStatus cert_and_status;
    cert_and_status.cert =
        X509Certificate::CreateFromBytes(der.data(), der.size());
    cert_and_status.cert_status = ERR_CERT_AUTHORITY_INVALID;

    ssl_config_.allowed_bad_certs.push_back(cert_and_status);
  }

  // PerformConnection makes an SSL connection to the openssl_helper binary and
  // does a ping-pong test to check the the SSL socket is working correctly.
  void PerformConnection() {
    client_ = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_LE(0, client_);
    ASSERT_EQ(
        0, connect(client_, (struct sockaddr*) &remote_, sizeof(remote_)));
    int on = 1;
    setsockopt(client_, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));

    SetupSSLConfig();
    log_.Clear();

    std::vector<uint8> localhost;
    localhost.push_back(127);
    localhost.push_back(0);
    localhost.push_back(0);
    localhost.push_back(1);
    AddressList addr_list(localhost, 443, false);
    TCPClientSocket* transport = new TCPClientSocket(
        addr_list, &log_, NetLog::Source());

    transport->AdoptSocket(client_);
    scoped_ptr<SSLClientSocket> sock(
        socket_factory_->CreateSSLClientSocket(
            transport, HostPortPair("example.com", 443), ssl_config_,
            new TestSSLHostInfo()));

    TestCompletionCallback callback;
    int rv = sock->Connect(&callback);
    if (rv != OK) {
      ASSERT_EQ(ERR_IO_PENDING, rv);
      EXPECT_FALSE(sock->IsConnected());
      rv = callback.WaitForResult();
      EXPECT_EQ(OK, rv);
    }
    EXPECT_TRUE(sock->IsConnected());

    static const char request_text[] = "hello!";
    scoped_refptr<IOBuffer> request_buffer =
        new IOBuffer(arraysize(request_text) - 1);
    memcpy(request_buffer->data(), request_text, arraysize(request_text) - 1);
    rv = sock->Write(request_buffer, arraysize(request_text), &callback);
    if (rv < 0) {
      ASSERT_EQ(ERR_IO_PENDING, rv);
      rv = callback.WaitForResult();
    }
    EXPECT_EQ(7, rv);

    scoped_refptr<IOBuffer> reply_buffer = new IOBuffer(8);
    rv = sock->Read(reply_buffer, 8, &callback);
    if (rv < 0) {
      ASSERT_EQ(ERR_IO_PENDING, rv);
      rv = callback.WaitForResult();
    }
    EXPECT_EQ(8, rv);
    EXPECT_TRUE(memcmp(reply_buffer->data(), "goodbye!", 8) == 0);

    next_proto_status_ = sock->GetNextProto(&next_proto_);

    sock->Disconnect();
  }

  // SnapStartEventType extracts the type of Snap Start from the NetLog. See
  // the SSL_SNAP_START_* defines in sslt.h
  int SnapStartEventType() {
    const std::vector<CapturingNetLog::Entry>& entries = log_.entries();
    for (std::vector<CapturingNetLog::Entry>::const_iterator
         i = entries.begin(); i != entries.end(); i++) {
      if (i->type == NetLog::TYPE_SSL_SNAP_START) {
        scoped_ptr<Value> value(i->extra_parameters->ToValue());
        CHECK(value->GetType() == Value::TYPE_DICTIONARY);
        DictionaryValue* dict = reinterpret_cast<DictionaryValue*>(value.get());
        int ret;
        CHECK(dict->GetInteger("type", &ret));
        return ret;
      }
    }
    return -1;
  }

  // DidMerge returns true if the NetLog suggests the the SSL connection merged
  // it's certificate validation with the optimistic validation from the
  // SSLHostInfo.
  bool DidMerge() {
    const std::vector<CapturingNetLog::Entry>& entries = log_.entries();
    for (std::vector<CapturingNetLog::Entry>::const_iterator
         i = entries.begin(); i != entries.end(); i++) {
      if (i->type == NetLog::TYPE_SSL_VERIFICATION_MERGED)
        return true;
    }
    return false;
  }

  base::ProcessHandle child_;
  ClientSocketFactory* const socket_factory_;
  struct sockaddr_in remote_;
  int client_;
  SSLConfig ssl_config_;
  CapturingNetLog log_;
  SSLClientSocket::NextProtoStatus next_proto_status_;
  std::string next_proto_;
};

TEST_F(SSLClientSocketSnapStartTest, Basic) {
  // Not a Snap Start connection.
  StartSnapStartServer(NULL);
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_NONE, SnapStartEventType());
  EXPECT_FALSE(DidMerge());
}

TEST_F(SSLClientSocketSnapStartTest, SnapStart) {
  StartSnapStartServer("snap-start", NULL);
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_NONE, SnapStartEventType());
  EXPECT_FALSE(DidMerge());
  SSLClientSocketNSS::ClearSessionCache();
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_FULL, SnapStartEventType());
  EXPECT_TRUE(DidMerge());
}

TEST_F(SSLClientSocketSnapStartTest, SnapStartResume) {
  StartSnapStartServer("snap-start", NULL);
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_NONE, SnapStartEventType());
  EXPECT_FALSE(DidMerge());
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_RESUME, SnapStartEventType());
  EXPECT_TRUE(DidMerge());
}

TEST_F(SSLClientSocketSnapStartTest, SnapStartRecovery) {
  StartSnapStartServer("snap-start-recovery", NULL);
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_NONE, SnapStartEventType());
  EXPECT_FALSE(DidMerge());
  SSLClientSocketNSS::ClearSessionCache();
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_RECOVERY, SnapStartEventType());
  EXPECT_TRUE(DidMerge());
}

TEST_F(SSLClientSocketSnapStartTest, SnapStartResumeRecovery) {
  StartSnapStartServer("snap-start-recovery", NULL);
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_NONE, SnapStartEventType());
  EXPECT_FALSE(DidMerge());
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_RESUME_RECOVERY, SnapStartEventType());
  EXPECT_TRUE(DidMerge());
}

TEST_F(SSLClientSocketSnapStartTest, SnapStartWithNPN) {
  ssl_config_.next_protos.assign("\003foo\003bar");
  StartSnapStartServer("snap-start", "npn", NULL);
  PerformConnection();
  EXPECT_EQ(SSLClientSocket::kNextProtoNegotiated, next_proto_status_);
  EXPECT_EQ("foo", next_proto_);
  EXPECT_EQ(SSL_SNAP_START_NONE, SnapStartEventType());
  EXPECT_FALSE(DidMerge());
  SSLClientSocketNSS::ClearSessionCache();
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_FULL, SnapStartEventType());
  EXPECT_EQ(SSLClientSocket::kNextProtoNegotiated, next_proto_status_);
  EXPECT_EQ("foo", next_proto_);
  EXPECT_TRUE(DidMerge());
}

TEST_F(SSLClientSocketSnapStartTest, SnapStartWithNPNMispredict) {
  // This tests that we recover in the event of a misprediction.
  ssl_config_.next_protos.assign("\003foo\003baz");
  StartSnapStartServer("snap-start", "npn-mispredict", NULL);
  PerformConnection();
  EXPECT_EQ(SSLClientSocket::kNextProtoNegotiated, next_proto_status_);
  EXPECT_EQ("foo", next_proto_);
  EXPECT_EQ(SSL_SNAP_START_NONE, SnapStartEventType());
  EXPECT_FALSE(DidMerge());

  SSLClientSocketNSS::ClearSessionCache();
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_RECOVERY, SnapStartEventType());
  EXPECT_EQ(SSLClientSocket::kNextProtoNegotiated, next_proto_status_);
  EXPECT_EQ("baz", next_proto_);
  EXPECT_TRUE(DidMerge());

  SSLClientSocketNSS::ClearSessionCache();
  PerformConnection();
  EXPECT_EQ(SSL_SNAP_START_FULL, SnapStartEventType());
  EXPECT_EQ(SSLClientSocket::kNextProtoNegotiated, next_proto_status_);
  EXPECT_EQ("baz", next_proto_);
  EXPECT_TRUE(DidMerge());
}

}  // namespace net
