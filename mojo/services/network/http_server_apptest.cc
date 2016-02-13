// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/services/network/net_address_type_converters.h"
#include "mojo/services/network/public/cpp/web_socket_read_queue.h"
#include "mojo/services/network/public/cpp/web_socket_write_queue.h"
#include "mojo/services/network/public/interfaces/http_connection.mojom.h"
#include "mojo/services/network/public/interfaces/http_message.mojom.h"
#include "mojo/services/network/public/interfaces/http_server.mojom.h"
#include "mojo/services/network/public/interfaces/net_address.mojom.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/services/network/public/interfaces/web_socket.mojom.h"
#include "mojo/services/network/public/interfaces/web_socket_factory.mojom.h"
#include "mojo/shell/public/cpp/application_test_base.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/socket/tcp_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

const int kMaxExpectedResponseLength = 2048;

NetAddressPtr GetLocalHostWithAnyPort() {
  NetAddressPtr addr(NetAddress::New());
  addr->family = NetAddressFamily::IPV4;
  addr->ipv4 = NetAddressIPv4::New();
  addr->ipv4->port = 0;
  addr->ipv4->addr.resize(4);
  addr->ipv4->addr[0] = 127;
  addr->ipv4->addr[1] = 0;
  addr->ipv4->addr[2] = 0;
  addr->ipv4->addr[3] = 1;

  return addr;
}

using TestHeaders = std::vector<std::pair<std::string, std::string>>;

struct TestRequest {
  std::string method;
  std::string url;
  TestHeaders headers;
  scoped_ptr<std::string> body;
};

struct TestResponse {
  uint32_t status_code;
  TestHeaders headers;
  scoped_ptr<std::string> body;
};

std::string MakeRequestMessage(const TestRequest& data) {
  std::string message = data.method + " " + data.url + " HTTP/1.1\r\n";
  for (const auto& item : data.headers)
    message += item.first + ": " + item.second + "\r\n";
  message += "\r\n";
  if (data.body)
    message += *data.body;

  return message;
}

HttpResponsePtr MakeResponseStruct(const TestResponse& data) {
  HttpResponsePtr response(HttpResponse::New());
  response->status_code = data.status_code;
  response->headers.resize(data.headers.size());
  size_t index = 0;
  for (const auto& item : data.headers) {
    HttpHeaderPtr header(HttpHeader::New());
    header->name = item.first;
    header->value = item.second;
    response->headers[index++] = std::move(header);
  }

  if (data.body) {
    uint32_t num_bytes = static_cast<uint32_t>(data.body->size());
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = num_bytes;
    DataPipe data_pipe(options);
    response->body = std::move(data_pipe.consumer_handle);
    MojoResult result =
        WriteDataRaw(data_pipe.producer_handle.get(), data.body->data(),
                     &num_bytes, MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    EXPECT_EQ(MOJO_RESULT_OK, result);
  }

  return response;
}

void CheckHeaders(const TestHeaders& expected,
                  const Array<HttpHeaderPtr>& headers) {
  // The server impl fiddles with Content-Length and Content-Type. So we don't
  // do a strict check here.
  std::map<std::string, std::string> header_map;
  for (size_t i = 0; i < headers.size(); ++i) {
    std::string lower_name =
        base::ToLowerASCII(headers[i]->name.To<std::string>());
    header_map[lower_name] = headers[i]->value;
  }

  for (const auto& item : expected) {
    std::string lower_name = base::ToLowerASCII(item.first);
    EXPECT_NE(header_map.end(), header_map.find(lower_name));
    EXPECT_EQ(item.second, header_map[lower_name]);
  }
}

void CheckRequest(const TestRequest& expected, HttpRequestPtr request) {
  EXPECT_EQ(expected.method, request->method);
  EXPECT_EQ(expected.url, request->url);
  CheckHeaders(expected.headers, request->headers);
  if (expected.body) {
    EXPECT_TRUE(request->body.is_valid());
    std::string body;
    common::BlockingCopyToString(std::move(request->body), &body);
    EXPECT_EQ(*expected.body, body);
  } else {
    EXPECT_FALSE(request->body.is_valid());
  }
}

void CheckResponse(const TestResponse& expected, const std::string& response) {
  int header_end =
      net::HttpUtil::LocateEndOfHeaders(response.c_str(), response.size());
  std::string assembled_headers =
      net::HttpUtil::AssembleRawHeaders(response.c_str(), header_end);
  scoped_refptr<net::HttpResponseHeaders> parsed_headers(
      new net::HttpResponseHeaders(assembled_headers));
  EXPECT_EQ(expected.status_code,
            static_cast<uint32_t>(parsed_headers->response_code()));
  for (const auto& item : expected.headers)
    EXPECT_TRUE(parsed_headers->HasHeaderValue(item.first, item.second));

  if (expected.body) {
    EXPECT_NE(-1, header_end);
    std::string body(response, static_cast<size_t>(header_end));
    EXPECT_EQ(*expected.body, body);
  } else {
    EXPECT_EQ(response.size(), static_cast<size_t>(header_end));
  }
}

class TestHttpClient {
 public:
  TestHttpClient() : connect_result_(net::OK) {}

  void Connect(const net::IPEndPoint& address) {
    net::AddressList addresses(address);
    net::NetLog::Source source;
    socket_.reset(new net::TCPClientSocket(addresses, NULL, source));

    base::RunLoop run_loop;
    connect_result_ = socket_->Connect(base::Bind(&TestHttpClient::OnConnect,
                                                  base::Unretained(this),
                                                  run_loop.QuitClosure()));
    if (connect_result_ == net::ERR_IO_PENDING)
      run_loop.Run();

    ASSERT_EQ(net::OK, connect_result_);
  }

  void Send(const std::string& data) {
    write_buffer_ = new net::DrainableIOBuffer(new net::StringIOBuffer(data),
                                               data.length());
    Write();
  }

  // Note: This method determines the end of the response only by Content-Length
  // and connection termination. Besides, it doesn't truncate at the end of the
  // response, so |message| may return more data (e.g., part of the next
  // response).
  void ReadResponse(std::string* message) {
    if (!Read(message, 1))
      return;
    while (!IsCompleteResponse(*message)) {
      std::string chunk;
      if (!Read(&chunk, 1))
        return;
      message->append(chunk);
    }
    return;
  }

 private:
  void OnConnect(const base::Closure& quit_loop, int result) {
    connect_result_ = result;
    quit_loop.Run();
  }

  void Write() {
    int result = socket_->Write(
        write_buffer_.get(), write_buffer_->BytesRemaining(),
        base::Bind(&TestHttpClient::OnWrite, base::Unretained(this)));
    if (result != net::ERR_IO_PENDING)
      OnWrite(result);
  }

  void OnWrite(int result) {
    ASSERT_GT(result, 0);
    write_buffer_->DidConsume(result);
    if (write_buffer_->BytesRemaining())
      Write();
  }

  bool Read(std::string* message, int expected_bytes) {
    int total_bytes_received = 0;
    message->clear();
    while (total_bytes_received < expected_bytes) {
      net::TestCompletionCallback callback;
      ReadInternal(callback.callback());
      int bytes_received = callback.WaitForResult();
      if (bytes_received <= 0)
        return false;

      total_bytes_received += bytes_received;
      message->append(read_buffer_->data(), bytes_received);
    }
    return true;
  }

  void ReadInternal(const net::CompletionCallback& callback) {
    read_buffer_ = new net::IOBufferWithSize(kMaxExpectedResponseLength);
    int result =
        socket_->Read(read_buffer_.get(), kMaxExpectedResponseLength, callback);
    if (result != net::ERR_IO_PENDING)
      callback.Run(result);
  }

  bool IsCompleteResponse(const std::string& response) {
    // Check end of headers first.
    int end_of_headers =
        net::HttpUtil::LocateEndOfHeaders(response.data(), response.size());
    if (end_of_headers < 0)
      return false;

    // Return true if response has data equal to or more than content length.
    int64_t body_size = static_cast<int64_t>(response.size()) - end_of_headers;
    DCHECK_LE(0, body_size);
    scoped_refptr<net::HttpResponseHeaders> headers(
        new net::HttpResponseHeaders(net::HttpUtil::AssembleRawHeaders(
            response.data(), end_of_headers)));
    return body_size >= headers->GetContentLength();
  }

  scoped_refptr<net::IOBufferWithSize> read_buffer_;
  scoped_refptr<net::DrainableIOBuffer> write_buffer_;
  scoped_ptr<net::TCPClientSocket> socket_;
  int connect_result_;

  DISALLOW_COPY_AND_ASSIGN(TestHttpClient);
};

class WebSocketClientImpl : public WebSocketClient {
 public:
  explicit WebSocketClientImpl()
      : binding_(this, &client_ptr_),
        wait_for_message_count_(0),
        run_loop_(nullptr) {}
  ~WebSocketClientImpl() override {}

  // Establishes a connection from the client side.
  void Connect(WebSocketPtr web_socket, const std::string& url) {
    web_socket_ = std::move(web_socket);

    DataPipe data_pipe;
    send_stream_ = std::move(data_pipe.producer_handle);
    write_send_stream_.reset(new WebSocketWriteQueue(send_stream_.get()));

    web_socket_->Connect(url, Array<String>(), "http://example.com",
                         std::move(data_pipe.consumer_handle),
                         std::move(client_ptr_));
  }

  // Establishes a connection from the server side.
  void AcceptConnectRequest(
      const HttpConnectionDelegate::OnReceivedWebSocketRequestCallback&
          callback) {
    InterfaceRequest<WebSocket> web_socket_request = GetProxy(&web_socket_);

    DataPipe data_pipe;
    send_stream_ = std::move(data_pipe.producer_handle);
    write_send_stream_.reset(new WebSocketWriteQueue(send_stream_.get()));

    callback.Run(std::move(web_socket_request),
                 std::move(data_pipe.consumer_handle), std::move(client_ptr_));
  }

  void WaitForConnectCompletion() {
    DCHECK(!run_loop_);

    if (receive_stream_.is_valid())
      return;

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  void Send(const std::string& message) {
    DCHECK(!message.empty());

    uint32_t size = static_cast<uint32_t>(message.size());
    write_send_stream_->Write(
        &message[0], size,
        base::Bind(&WebSocketClientImpl::OnFinishedWritingSendStream,
                   base::Unretained(this), size));
  }

  void WaitForMessage(size_t count) {
    DCHECK(!run_loop_);

    if (received_messages_.size() >= count)
      return;
    wait_for_message_count_ = count;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  std::vector<std::string>& received_messages() { return received_messages_; }

 private:
  // WebSocketClient implementation.
  void DidConnect(const String& selected_subprotocol,
                  const String& extensions,
                  ScopedDataPipeConsumerHandle receive_stream) override {
    receive_stream_ = std::move(receive_stream);
    read_receive_stream_.reset(new WebSocketReadQueue(receive_stream_.get()));

    web_socket_->FlowControl(2048);
    if (run_loop_)
      run_loop_->Quit();
  }

  void DidReceiveData(bool fin,
                      WebSocket::MessageType type,
                      uint32_t num_bytes) override {
    DCHECK(num_bytes > 0);

    read_receive_stream_->Read(
        num_bytes,
        base::Bind(&WebSocketClientImpl::OnFinishedReadingReceiveStream,
                   base::Unretained(this), num_bytes));
  }

  void DidReceiveFlowControl(int64_t quota) override {}

  void DidFail(const String& message) override {}

  void DidClose(bool was_clean, uint16_t code, const String& reason) override {}

  void OnFinishedWritingSendStream(uint32_t num_bytes, const char* buffer) {
    EXPECT_TRUE(buffer);

    web_socket_->Send(true, WebSocket::MessageType::TEXT, num_bytes);
  }

  void OnFinishedReadingReceiveStream(uint32_t num_bytes, const char* data) {
    EXPECT_TRUE(data);

    received_messages_.push_back(std::string(data, num_bytes));
    if (run_loop_ && received_messages_.size() >= wait_for_message_count_) {
      wait_for_message_count_ = 0;
      run_loop_->Quit();
    }
  }

  WebSocketClientPtr client_ptr_;
  Binding<WebSocketClient> binding_;
  WebSocketPtr web_socket_;

  ScopedDataPipeProducerHandle send_stream_;
  scoped_ptr<WebSocketWriteQueue> write_send_stream_;

  ScopedDataPipeConsumerHandle receive_stream_;
  scoped_ptr<WebSocketReadQueue> read_receive_stream_;

  std::vector<std::string> received_messages_;
  size_t wait_for_message_count_;

  // Pointing to a stack-allocated RunLoop instance.
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketClientImpl);
};

class HttpConnectionDelegateImpl : public HttpConnectionDelegate {
 public:
  struct PendingRequest {
    HttpRequestPtr request;
    OnReceivedRequestCallback callback;
  };

  HttpConnectionDelegateImpl(HttpConnectionPtr connection,
                             InterfaceRequest<HttpConnectionDelegate> request)
      : connection_(std::move(connection)),
        binding_(this, std::move(request)),
        wait_for_request_count_(0),
        run_loop_(nullptr) {}
  ~HttpConnectionDelegateImpl() override {}

  // HttpConnectionDelegate implementation:
  void OnReceivedRequest(HttpRequestPtr request,
                         const OnReceivedRequestCallback& callback) override {
    linked_ptr<PendingRequest> pending_request(new PendingRequest);
    pending_request->request = std::move(request);
    pending_request->callback = callback;
    pending_requests_.push_back(pending_request);
    if (run_loop_ && pending_requests_.size() >= wait_for_request_count_) {
      wait_for_request_count_ = 0;
      run_loop_->Quit();
    }
  }

  void OnReceivedWebSocketRequest(
      HttpRequestPtr request,
      const OnReceivedWebSocketRequestCallback& callback) override {
    web_socket_.reset(new WebSocketClientImpl());

    web_socket_->AcceptConnectRequest(callback);

    if (run_loop_)
      run_loop_->Quit();
  }

  void SendResponse(HttpResponsePtr response) {
    ASSERT_FALSE(pending_requests_.empty());
    linked_ptr<PendingRequest> request = pending_requests_[0];
    pending_requests_.erase(pending_requests_.begin());
    request->callback.Run(std::move(response));
  }

  void WaitForRequest(size_t count) {
    DCHECK(!run_loop_);

    if (pending_requests_.size() >= count)
      return;

    wait_for_request_count_ = count;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  void WaitForWebSocketRequest() {
    DCHECK(!run_loop_);

    if (web_socket_)
      return;

    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  std::vector<linked_ptr<PendingRequest>>& pending_requests() {
    return pending_requests_;
  }

  WebSocketClientImpl* web_socket() { return web_socket_.get(); }

 private:
  HttpConnectionPtr connection_;
  Binding<HttpConnectionDelegate> binding_;
  std::vector<linked_ptr<PendingRequest>> pending_requests_;
  size_t wait_for_request_count_;
  scoped_ptr<WebSocketClientImpl> web_socket_;

  // Pointing to a stack-allocated RunLoop instance.
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(HttpConnectionDelegateImpl);
};

class HttpServerDelegateImpl : public HttpServerDelegate {
 public:
  explicit HttpServerDelegateImpl(HttpServerDelegatePtr* delegate_ptr)
      : binding_(this, delegate_ptr),
        wait_for_connection_count_(0),
        run_loop_(nullptr) {}
  ~HttpServerDelegateImpl() override {}

  // HttpServerDelegate implementation.
  void OnConnected(HttpConnectionPtr connection,
                   InterfaceRequest<HttpConnectionDelegate> delegate) override {
    connections_.push_back(make_linked_ptr(new HttpConnectionDelegateImpl(
        std::move(connection), std::move(delegate))));
    if (run_loop_ && connections_.size() >= wait_for_connection_count_) {
      wait_for_connection_count_ = 0;
      run_loop_->Quit();
    }
  }

  void WaitForConnection(size_t count) {
    DCHECK(!run_loop_);

    if (connections_.size() >= count)
      return;

    wait_for_connection_count_ = count;
    base::RunLoop run_loop;
    run_loop_ = &run_loop;
    run_loop.Run();
    run_loop_ = nullptr;
  }

  std::vector<linked_ptr<HttpConnectionDelegateImpl>>& connections() {
    return connections_;
  }

 private:
  Binding<HttpServerDelegate> binding_;
  std::vector<linked_ptr<HttpConnectionDelegateImpl>> connections_;
  size_t wait_for_connection_count_;
  // Pointing to a stack-allocated RunLoop instance.
  base::RunLoop* run_loop_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerDelegateImpl);
};

class HttpServerAppTest : public test::ApplicationTestBase {
 public:
  HttpServerAppTest() : message_loop_(base::MessageLoop::TYPE_IO) {}
  ~HttpServerAppTest() override {}

 protected:
  bool ShouldCreateDefaultRunLoop() override { return false; }

  void SetUp() override {
    ApplicationTestBase::SetUp();

    scoped_ptr<Connection> connection =
        shell()->Connect("mojo:network_service");
    connection->GetInterface(&network_service_);
    connection->GetInterface(&web_socket_factory_);
  }

  void CreateHttpServer(HttpServerDelegatePtr delegate,
                        NetAddressPtr* out_bound_to) {
    network_service_->CreateHttpServer(
        GetLocalHostWithAnyPort(), std::move(delegate),
        [out_bound_to](NetworkErrorPtr result, NetAddressPtr bound_to) {
          ASSERT_EQ(net::OK, result->code);
          EXPECT_NE(0u, bound_to->ipv4->port);
          *out_bound_to = std::move(bound_to);
        });
    network_service_.WaitForIncomingResponse();
  }

  NetworkServicePtr network_service_;
  WebSocketFactoryPtr web_socket_factory_;

 private:
  base::MessageLoop message_loop_;

  DISALLOW_COPY_AND_ASSIGN(HttpServerAppTest);
};

}  // namespace

TEST_F(HttpServerAppTest, BasicHttpRequestResponse) {
  NetAddressPtr bound_to;
  HttpServerDelegatePtr server_delegate_ptr;
  HttpServerDelegateImpl server_delegate_impl(&server_delegate_ptr);
  CreateHttpServer(std::move(server_delegate_ptr), &bound_to);

  TestHttpClient client;
  client.Connect(bound_to.To<net::IPEndPoint>());

  server_delegate_impl.WaitForConnection(1);
  HttpConnectionDelegateImpl& connection =
      *server_delegate_impl.connections()[0];

  TestRequest request_data = {"HEAD", "/test", {{"Hello", "World"}}, nullptr};
  client.Send(MakeRequestMessage(request_data));

  connection.WaitForRequest(1);

  CheckRequest(request_data,
               std::move(connection.pending_requests()[0]->request));

  TestResponse response_data = {200, {{"Content-Length", "4"}}, nullptr};
  connection.SendResponse(MakeResponseStruct(response_data));
  // This causes the underlying TCP connection to be closed. The client can
  // determine the end of the response based on that.
  server_delegate_impl.connections().clear();

  std::string response_message;
  client.ReadResponse(&response_message);

  CheckResponse(response_data, response_message);
}

TEST_F(HttpServerAppTest, HttpRequestResponseWithBody) {
  NetAddressPtr bound_to;
  HttpServerDelegatePtr server_delegate_ptr;
  HttpServerDelegateImpl server_delegate_impl(&server_delegate_ptr);
  CreateHttpServer(std::move(server_delegate_ptr), &bound_to);

  TestHttpClient client;
  client.Connect(bound_to.To<net::IPEndPoint>());

  server_delegate_impl.WaitForConnection(1);
  HttpConnectionDelegateImpl& connection =
      *server_delegate_impl.connections()[0];

  TestRequest request_data = {
      "Post",
      "/test",
      {{"Hello", "World"},
       {"Content-Length", "23"},
       {"Content-Type", "text/plain"}},
      make_scoped_ptr(new std::string("This is a test request!"))};
  client.Send(MakeRequestMessage(request_data));

  connection.WaitForRequest(1);

  CheckRequest(request_data,
               std::move(connection.pending_requests()[0]->request));

  TestResponse response_data = {
      200,
      {{"Content-Length", "26"}},
      make_scoped_ptr(new std::string("This is a test response..."))};
  connection.SendResponse(MakeResponseStruct(response_data));

  std::string response_message;
  client.ReadResponse(&response_message);

  CheckResponse(response_data, response_message);
}

TEST_F(HttpServerAppTest, WebSocket) {
  NetAddressPtr bound_to;
  HttpServerDelegatePtr server_delegate_ptr;
  HttpServerDelegateImpl server_delegate_impl(&server_delegate_ptr);
  CreateHttpServer(std::move(server_delegate_ptr), &bound_to);

  WebSocketPtr web_socket_ptr;
  web_socket_factory_->CreateWebSocket(GetProxy(&web_socket_ptr));
  WebSocketClientImpl socket_0;
  socket_0.Connect(
      std::move(web_socket_ptr),
      base::StringPrintf("ws://127.0.0.1:%d/hello", bound_to->ipv4->port));

  server_delegate_impl.WaitForConnection(1);
  HttpConnectionDelegateImpl& connection =
      *server_delegate_impl.connections()[0];

  connection.WaitForWebSocketRequest();
  WebSocketClientImpl& socket_1 = *connection.web_socket();

  socket_1.WaitForConnectCompletion();
  socket_0.WaitForConnectCompletion();

  socket_0.Send("Hello");
  socket_0.Send("world!");

  socket_1.WaitForMessage(2);
  EXPECT_EQ("Hello", socket_1.received_messages()[0]);
  EXPECT_EQ("world!", socket_1.received_messages()[1]);

  socket_1.Send("How do");
  socket_1.Send("you do?");

  socket_0.WaitForMessage(2);
  EXPECT_EQ("How do", socket_0.received_messages()[0]);
  EXPECT_EQ("you do?", socket_0.received_messages()[1]);
}

}  // namespace mojo
