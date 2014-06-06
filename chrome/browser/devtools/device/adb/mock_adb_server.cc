// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"
#include "net/socket/tcp_server_socket.h"

using content::BrowserThread;

namespace {

const char kHostTransportPrefix[] = "host:transport:";
const char kLocalAbstractPrefix[] = "localabstract:";

const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";
const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
const char kDumpsysCommand[] = "shell:dumpsys window policy";
const char kListProcessesCommand[] = "shell:ps";

const char kSerialOnline[] = "01498B321301A00A";
const char kSerialOffline[] = "01498B2B0D01300E";
const char kDeviceModel[] = "Nexus 6";

const char kJsonVersionPath[] = "/json/version";
const char kJsonPath[] = "/json";
const char kJsonListPath[] = "/json/list";

const char kHttpRequestTerminator[] = "\r\n\r\n";

const char kHttpResponse[] =
    "HTTP/1.1 200 OK\r\n"
    "Content-Length:%d\r\n"
    "Content-Type:application/json; charset=UTF-8\r\n\r\n%s";

const char kSampleOpenedUnixSockets[] =
    "Num       RefCount Protocol Flags    Type St Inode Path\n"
    "00000000: 00000004 00000000"
    " 00000000 0002 01  3328 /dev/socket/wpa_wlan0\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01  5394 /dev/socket/vold\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 11810 @webview_devtools_remote_2425\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 20893 @chrome_devtools_remote\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 20894 @chrome_devtools_remote_1002\n"
    "00000000: 00000002 00000000"
    " 00010000 0001 01 20895 @noprocess_devtools_remote\n";

const char kSampleListProcesses[] =
    "USER   PID  PPID VSIZE  RSS    WCHAN    PC         NAME\n"
    "root   1    0    688    508    ffffffff 00000000 S /init\r\n"
    "u0_a75 2425 123  933736 193024 ffffffff 00000000 S com.sample.feed\r\n"
    "nfc    741  123  706448 26316  ffffffff 00000000 S com.android.nfc\r\n"
    "u0_a76 1001 124  111111 222222 ffffffff 00000000 S com.android.chrome\r\n"
    "u0_a77 1002 125  111111 222222 ffffffff 00000000 S com.chrome.beta\r\n"
    "u0_a78 1003 126  111111 222222 ffffffff 00000000 S com.noprocess.app\r\n";

const char kSampleDumpsys[] =
    "WINDOW MANAGER POLICY STATE (dumpsys window policy)\r\n"
    "    mSafeMode=false mSystemReady=true mSystemBooted=true\r\n"
    "    mStable=(0,50)-(720,1184)\r\n" // Only mStable parameter is parsed
    "    mForceStatusBar=false mForceStatusBarFromKeyguard=false\r\n";

char kSampleChromeVersion[] = "{\n"
    "   \"Browser\": \"Chrome/32.0.1679.0\",\n"
    "   \"Protocol-Version\": \"1.0\",\n"
    "   \"User-Agent\": \"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/32.0.1679.0 Safari/537.36\",\n"
    "   \"WebKit-Version\": \"537.36 (@160162)\"\n"
    "}";

char kSampleChromeBetaVersion[] = "{\n"
    "   \"Browser\": \"Chrome/31.0.1599.0\",\n"
    "   \"Protocol-Version\": \"1.0\",\n"
    "   \"User-Agent\": \"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/32.0.1679.0 Safari/537.36\",\n"
    "   \"WebKit-Version\": \"537.36 (@160162)\"\n"
    "}";

char kSampleWebViewVersion[] = "{\n"
    "   \"Browser\": \"Version/4.0\",\n"
    "   \"Protocol-Version\": \"1.0\",\n"
    "   \"User-Agent\": \"Mozilla/5.0 (Linux; Android 4.3; Build/KRS74B) "
    "AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Safari/537.36\",\n"
    "   \"WebKit-Version\": \"537.36 (@157588)\"\n"
    "}";

char kSampleChromePages[] = "[ {\n"
    "   \"description\": \"\",\n"
    "   \"devtoolsFrontendUrl\": \"/devtools/devtools.html?"
    "ws=/devtools/page/755DE5C9-D49F-811D-0693-51B8E15C80D2\",\n"
    "   \"id\": \"755DE5C9-D49F-811D-0693-51B8E15C80D2\",\n"
    "   \"title\": \"The Chromium Projects\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"http://www.chromium.org/\",\n"
    "   \"webSocketDebuggerUrl\": \""
    "ws:///devtools/page/755DE5C9-D49F-811D-0693-51B8E15C80D2\"\n"
    "} ]";

char kSampleChromeBetaPages[] = "[]";

char kSampleWebViewPages[] = "[ {\n"
    "   \"description\": \"{\\\"attached\\\":false,\\\"empty\\\":false,"
    "\\\"height\\\":1173,\\\"screenX\\\":0,\\\"screenY\\\":0,"
    "\\\"visible\\\":true,\\\"width\\\":800}\",\n"
    "   \"devtoolsFrontendUrl\": \"http://chrome-devtools-frontend.appspot.com/"
    "serve_rev/@157588/devtools.html?ws="
    "/devtools/page/3E962D4D-B676-182D-3BE8-FAE7CE224DE7\",\n"
    "   \"faviconUrl\": \"http://chromium.org/favicon.ico\",\n"
    "   \"id\": \"3E962D4D-B676-182D-3BE8-FAE7CE224DE7\",\n"
    "   \"thumbnailUrl\": \"/thumb/3E962D4D-B676-182D-3BE8-FAE7CE224DE7\",\n"
    "   \"title\": \"Blink - The Chromium Projects\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"http://www.chromium.org/blink\",\n"
    "   \"webSocketDebuggerUrl\": \"ws:///devtools/"
    "page/3E962D4D-B676-182D-3BE8-FAE7CE224DE7\"\n"
    "}, {\n"
    "   \"description\": \"{\\\"attached\\\":true,\\\"empty\\\":true,"
    "\\\"screenX\\\":0,\\\"screenY\\\":33,\\\"visible\\\":false}\",\n"
    "   \"devtoolsFrontendUrl\": \"http://chrome-devtools-frontend.appspot.com/"
    "serve_rev/@157588/devtools.html?ws="
    "/devtools/page/44681551-ADFD-2411-076B-3AB14C1C60E2\",\n"
    "   \"faviconUrl\": \"\",\n"
    "   \"id\": \"44681551-ADFD-2411-076B-3AB14C1C60E2\",\n"
    "   \"thumbnailUrl\": \"/thumb/44681551-ADFD-2411-076B-3AB14C1C60E2\",\n"
    "   \"title\": \"More Activity\",\n"
    "   \"type\": \"page\",\n"
    "   \"url\": \"about:blank\",\n"
    "   \"webSocketDebuggerUrl\": \"ws:///devtools/page/"
    "44681551-ADFD-2411-076B-3AB14C1C60E2\"\n"
    "}]";

static const int kBufferSize = 16*1024;
static const int kAdbPort = 5037;

static const int kAdbMessageHeaderSize = 4;

class SimpleHttpServer : base::NonThreadSafe {
 public:
  class Parser {
   public:
    virtual int Consume(const char* data, int size) = 0;
    virtual ~Parser() {}
  };

  typedef base::Callback<void(const std::string&)> SendCallback;
  typedef base::Callback<Parser*(const SendCallback&)> ParserFactory;

  SimpleHttpServer(const ParserFactory& factory, net::IPEndPoint endpoint);
  virtual ~SimpleHttpServer();

 private:
  class Connection : base::NonThreadSafe {
   public:
    Connection(net::StreamSocket* socket, const ParserFactory& factory);
    virtual ~Connection();

   private:
    void Send(const std::string& message);
    void ReadData();
    void OnDataRead(int count);
    void WriteData();
    void OnDataWritten(int count);

    scoped_ptr<net::StreamSocket> socket_;
    scoped_ptr<Parser> parser_;
    scoped_refptr<net::GrowableIOBuffer> input_buffer_;
    scoped_refptr<net::GrowableIOBuffer> output_buffer_;
    int bytes_to_write_;
    bool read_closed_;

    DISALLOW_COPY_AND_ASSIGN(Connection);
  };

  void AcceptConnection();
  void OnAccepted(int result);

  ParserFactory factory_;
  scoped_ptr<net::TCPServerSocket> socket_;
  scoped_ptr<net::StreamSocket> client_socket_;

  DISALLOW_COPY_AND_ASSIGN(SimpleHttpServer);
};

SimpleHttpServer::SimpleHttpServer(const ParserFactory& factory,
                                   net::IPEndPoint endpoint)
    : factory_(factory),
      socket_(new net::TCPServerSocket(NULL, net::NetLog::Source())) {
  socket_->Listen(endpoint, 5);
  AcceptConnection();
}

SimpleHttpServer::~SimpleHttpServer() {
}

SimpleHttpServer::Connection::Connection(net::StreamSocket* socket,
                                         const ParserFactory& factory)
    : socket_(socket),
      parser_(factory.Run(base::Bind(&Connection::Send,
                                     base::Unretained(this)))),
      input_buffer_(new net::GrowableIOBuffer()),
      output_buffer_(new net::GrowableIOBuffer()),
      bytes_to_write_(0),
      read_closed_(false) {
  input_buffer_->SetCapacity(kBufferSize);
  ReadData();
}

SimpleHttpServer::Connection::~Connection() {
}

void SimpleHttpServer::Connection::Send(const std::string& message) {
  CHECK(CalledOnValidThread());
  const char* data = message.c_str();
  int size = message.size();

  if ((output_buffer_->offset() + bytes_to_write_ + size) >
      output_buffer_->capacity()) {
    // If not enough space without relocation
    if (output_buffer_->capacity() < (bytes_to_write_ + size)) {
      // If even buffer is not enough
      int new_size = std::max(output_buffer_->capacity() * 2, size * 2);
      output_buffer_->SetCapacity(new_size);
    }
    memmove(output_buffer_->StartOfBuffer(),
            output_buffer_->data(),
            bytes_to_write_);
    output_buffer_->set_offset(0);
  }

  memcpy(output_buffer_->data() + bytes_to_write_, data, size);
  bytes_to_write_ += size;

  if (bytes_to_write_ == size)
    // If write loop wasn't yet started, then start it
    WriteData();
}

void SimpleHttpServer::Connection::ReadData() {
  CHECK(CalledOnValidThread());

  if (input_buffer_->RemainingCapacity() == 0)
    input_buffer_->SetCapacity(input_buffer_->capacity() * 2);

  int read_result = socket_->Read(
      input_buffer_.get(),
      input_buffer_->RemainingCapacity(),
      base::Bind(&Connection::OnDataRead, base::Unretained(this)));

  if (read_result != net::ERR_IO_PENDING)
    OnDataRead(read_result);
}

void SimpleHttpServer::Connection::OnDataRead(int count) {
  CHECK(CalledOnValidThread());
  if (count <= 0) {
    if (bytes_to_write_ == 0)
      delete this;
    else
      read_closed_ = true;
    return;
  }
  input_buffer_->set_offset(input_buffer_->offset() + count);
  int bytes_processed;

  do {
    char* data = input_buffer_->StartOfBuffer();
    int data_size = input_buffer_->offset();
    bytes_processed = parser_->Consume(data, data_size);

    if (bytes_processed) {
      memmove(data, data + bytes_processed, data_size - bytes_processed);
      input_buffer_->set_offset(data_size - bytes_processed);
    }
  } while (bytes_processed);
  // Posting to avoid deep recursion in case of synchronous IO
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&Connection::ReadData, base::Unretained(this)));
}

void SimpleHttpServer::Connection::WriteData() {
  CHECK(CalledOnValidThread());
  CHECK_GE(output_buffer_->capacity(),
           output_buffer_->offset() + bytes_to_write_) << "Overflow";

  int write_result = socket_->Write(
      output_buffer_,
      bytes_to_write_,
      base::Bind(&Connection::OnDataWritten, base::Unretained(this)));

  if (write_result != net::ERR_IO_PENDING)
    OnDataWritten(write_result);
}

void SimpleHttpServer::Connection::OnDataWritten(int count) {
  CHECK(CalledOnValidThread());
  if (count < 0) {
    delete this;
    return;
  }
  CHECK_GT(count, 0);
  CHECK_GE(output_buffer_->capacity(),
           output_buffer_->offset() + bytes_to_write_) << "Overflow";

  bytes_to_write_ -= count;
  output_buffer_->set_offset(output_buffer_->offset() + count);

  if (bytes_to_write_ != 0)
    // Posting to avoid deep recursion in case of synchronous IO
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&Connection::WriteData, base::Unretained(this)));
  else if (read_closed_)
    delete this;
}

void SimpleHttpServer::AcceptConnection() {
  CHECK(CalledOnValidThread());

  int accept_result = socket_->Accept(&client_socket_,
      base::Bind(&SimpleHttpServer::OnAccepted, base::Unretained(this)));

  if (accept_result != net::ERR_IO_PENDING)
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&SimpleHttpServer::OnAccepted,
                   base::Unretained(this),
                   accept_result));
}

void SimpleHttpServer::OnAccepted(int result) {
  CHECK(CalledOnValidThread());
  ASSERT_EQ(result, 0);  // Fails if the socket is already in use.
  new Connection(client_socket_.release(), factory_);
  AcceptConnection();
}

class AdbParser : SimpleHttpServer::Parser, base::NonThreadSafe {
 public:
  static Parser* Create(const SimpleHttpServer::SendCallback& callback) {
    return new AdbParser(callback);
  }

  explicit AdbParser(const SimpleHttpServer::SendCallback& callback)
      : callback_(callback) {
  }

  virtual ~AdbParser() {
  }

 private:
  virtual int Consume(const char* data, int size) OVERRIDE {
    CHECK(CalledOnValidThread());
    if (!selected_socket_.empty()) {
      std::string message(data, size);
      size_t request_end_pos = message.find(kHttpRequestTerminator);
      if (request_end_pos != std::string::npos) {
        ProcessHTTPRequest(message.substr(0, request_end_pos));
        return request_end_pos + strlen(kHttpRequestTerminator);
      }
      return 0;
    }
    if (size >= kAdbMessageHeaderSize) {
      std::string message_header(data, kAdbMessageHeaderSize);
      int message_size;

      EXPECT_TRUE(base::HexStringToInt(message_header, &message_size));

      if (size >= message_size + kAdbMessageHeaderSize) {
        std::string message_body(data + kAdbMessageHeaderSize, message_size);
        ProcessCommand(message_body);
        return kAdbMessageHeaderSize + message_size;
      }
    }
    return 0;
  }

  void ProcessHTTPRequest(const std::string& request) {
    CHECK(CalledOnValidThread());
    std::vector<std::string> tokens;
    Tokenize(request, " ", &tokens);
    CHECK_EQ(3U, tokens.size());
    CHECK_EQ("GET", tokens[0]);
    CHECK_EQ("HTTP/1.1", tokens[2]);

    std::string path(tokens[1]);
    if (path == kJsonPath)
      path = kJsonListPath;

    if (selected_socket_ == "chrome_devtools_remote") {
      if (path == kJsonVersionPath)
        SendHTTPResponse(kSampleChromeVersion);
      else if (path == kJsonListPath)
        SendHTTPResponse(kSampleChromePages);
      else
        NOTREACHED() << "Unknown command " << request;
    } else if (selected_socket_ == "chrome_devtools_remote_1002") {
      if (path == kJsonVersionPath)
        SendHTTPResponse(kSampleChromeBetaVersion);
      else if (path == kJsonListPath)
        SendHTTPResponse(kSampleChromeBetaPages);
      else
        NOTREACHED() << "Unknown command " << request;
    } else if (selected_socket_.find("noprocess_devtools_remote") == 0) {
      if (path == kJsonVersionPath)
        SendHTTPResponse("{}");
      else if (path == kJsonListPath)
        SendHTTPResponse("[]");
      else
        NOTREACHED() << "Unknown command " << request;
    } else if (selected_socket_ == "webview_devtools_remote_2425") {
      if (path == kJsonVersionPath)
        SendHTTPResponse(kSampleWebViewVersion);
      else if (path == kJsonListPath)
        SendHTTPResponse(kSampleWebViewPages);
      else
        NOTREACHED() << "Unknown command " << request;
    } else {
      NOTREACHED() << "Unknown socket " << selected_socket_;
    }
  }

  void ProcessCommand(const std::string& command) {
    CHECK(CalledOnValidThread());
    if (command == "host:devices") {
      SendResponse(base::StringPrintf("%s\tdevice\n%s\toffline",
                                      kSerialOnline,
                                      kSerialOffline));
    } else if (command.find(kHostTransportPrefix) == 0) {
      selected_device_ = command.substr(strlen(kHostTransportPrefix));
      SendResponse("");
    } else if (selected_device_ != kSerialOnline) {
      Send("FAIL", "device offline (x)");
    } else if (command == kDeviceModelCommand) {
      SendResponse(kDeviceModel);
    } else if (command == kOpenedUnixSocketsCommand) {
      SendResponse(kSampleOpenedUnixSockets);
    } else if (command == kDumpsysCommand) {
      SendResponse(kSampleDumpsys);
    } else if (command == kListProcessesCommand) {
      SendResponse(kSampleListProcesses);
    } else if (command.find(kLocalAbstractPrefix) == 0) {
      selected_socket_ = command.substr(strlen(kLocalAbstractPrefix));
      SendResponse("");
    } else {
      NOTREACHED() << "Unknown command - " << command;
    }
  }

  void SendResponse(const std::string& response) {
    Send("OKAY", response);
  }

  void Send(const std::string& status, const std::string& response) {
    CHECK(CalledOnValidThread());
    CHECK_EQ(4U, status.size());

    std::stringstream response_stream;
    response_stream << status;

    int size = response.size();
    if (size > 0) {
      static const char kHexChars[] = "0123456789ABCDEF";
      for (int i = 3; i >= 0; i--)
        response_stream << kHexChars[ (size >> 4*i) & 0x0f ];
      response_stream << response;
    }
    callback_.Run(response_stream.str());
  }

  void SendHTTPResponse(const std::string& body) {
    CHECK(CalledOnValidThread());
    std::string response_data(base::StringPrintf(kHttpResponse,
                                                 static_cast<int>(body.size()),
                                                 body.c_str()));
    callback_.Run(response_data);
  }

  std::string selected_device_;
  std::string selected_socket_;
  SimpleHttpServer::SendCallback callback_;
};

static SimpleHttpServer* mock_adb_server_ = NULL;

void StartMockAdbServerOnIOThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  CHECK(mock_adb_server_ == NULL);
  net::IPAddressNumber address;
  net::ParseIPLiteralToNumber("127.0.0.1", &address);
  net::IPEndPoint endpoint(address, kAdbPort);
  mock_adb_server_ =
      new SimpleHttpServer(base::Bind(&AdbParser::Create), endpoint);
}

void StopMockAdbServerOnIOThread() {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  CHECK(mock_adb_server_ != NULL);
  delete mock_adb_server_;
  mock_adb_server_ = NULL;
}

} // namespace

void StartMockAdbServer() {
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&StartMockAdbServerOnIOThread),
      base::MessageLoop::QuitClosure());
  content::RunMessageLoop();
}

void StopMockAdbServer() {
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&StopMockAdbServerOnIOThread),
      base::MessageLoop::QuitClosure());
  content::RunMessageLoop();
}

