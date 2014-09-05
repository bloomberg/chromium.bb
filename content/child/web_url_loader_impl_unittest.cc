// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_url_loader_impl.h"

#include <string.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"
#include "content/child/resource_dispatcher.h"
#include "content/child/resource_loader_bridge.h"
#include "content/public/child/request_peer.h"
#include "content/public/common/resource_response_info.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURLError.h"
#include "third_party/WebKit/public/platform/WebURLLoaderClient.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "url/gurl.h"

namespace content {
namespace {

const char kTestURL[] = "http://foo";
const char kTestData[] = "blah!";

const char kFtpDirMimeType[] = "text/vnd.chromium.ftp-dir";
// Simple FTP directory listing.  Tests are not concerned with correct parsing,
// but rather correct cleanup when deleted while parsing.  Important details of
// this list are that it contains more than one entry that are not "." or "..".
const char kFtpDirListing[] =
    "drwxr-xr-x    3 ftp      ftp          4096 May 15 18:11 goat\n"
    "drwxr-xr-x    3 ftp      ftp          4096 May 15 18:11 hat";

const char kMultipartResponseMimeType[] = "multipart/x-mixed-replace";
const char kMultipartResponseHeaders[] =
    "HTTP/1.0 200 Peachy\r\n"
    "Content-Type: multipart/x-mixed-replace; boundary=boundary\r\n\r\n";
// Simple multipart response.  Imporant details for the tests are that it
// contains multiple chunks, and that it doesn't end with a boundary, so will
// send data in OnResponseComplete.  Also, it will resolve to kTestData.
const char kMultipartResponse[] =
    "--boundary\n"
    "Content-type: text/html\n\n"
    "bl"
    "--boundary\n"
    "Content-type: text/html\n\n"
    "ah!";

class TestBridge : public ResourceLoaderBridge,
                   public base::SupportsWeakPtr<TestBridge> {
 public:
  TestBridge() : peer_(NULL), canceled_(false) {}
  virtual ~TestBridge() {}

  // ResourceLoaderBridge implementation:
  virtual void SetRequestBody(ResourceRequestBody* request_body) OVERRIDE {}

  virtual bool Start(RequestPeer* peer) OVERRIDE {
    EXPECT_FALSE(peer_);
    peer_ = peer;
    return true;
  }

  virtual void Cancel() OVERRIDE {
    EXPECT_FALSE(canceled_);
    canceled_ = true;
  }

  virtual void SetDefersLoading(bool value) OVERRIDE {}

  virtual void DidChangePriority(net::RequestPriority new_priority,
                                 int intra_priority_value) OVERRIDE {}

  virtual bool AttachThreadedDataReceiver(
      blink::WebThreadedDataReceiver* threaded_data_receiver) OVERRIDE {
    NOTREACHED();
    return false;
  }

  virtual void SyncLoad(SyncLoadResponse* response) OVERRIDE {}

  RequestPeer* peer() { return peer_; }

  bool canceled() { return canceled_; }

 private:
  RequestPeer* peer_;
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(TestBridge);
};

class TestResourceDispatcher : public ResourceDispatcher {
 public:
  TestResourceDispatcher() : ResourceDispatcher(NULL) {}
  virtual ~TestResourceDispatcher() {}

  // ResourceDispatcher implementation:
  virtual ResourceLoaderBridge* CreateBridge(
      const RequestInfo& request_info) OVERRIDE {
    EXPECT_FALSE(bridge_.get());
    TestBridge* bridge = new TestBridge();
    bridge_ = bridge->AsWeakPtr();
    return bridge;
  }

  TestBridge* bridge() { return bridge_.get(); }

 private:
  base::WeakPtr<TestBridge> bridge_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcher);
};

class TestWebURLLoaderClient : public blink::WebURLLoaderClient {
 public:
  TestWebURLLoaderClient(ResourceDispatcher* dispatcher)
      : loader_(new WebURLLoaderImpl(dispatcher)),
        expect_multipart_response_(false),
        delete_on_receive_redirect_(false),
        delete_on_receive_response_(false),
        delete_on_receive_data_(false),
        delete_on_finish_(false),
        delete_on_fail_(false),
        did_receive_redirect_(false),
        did_receive_response_(false),
        did_finish_(false) {
  }

  virtual ~TestWebURLLoaderClient() {}

  // blink::WebURLLoaderClient implementation:
  virtual void willSendRequest(
      blink::WebURLLoader* loader,
      blink::WebURLRequest& newRequest,
      const blink::WebURLResponse& redirectResponse) OVERRIDE {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
    // No test currently simulates mutiple redirects.
    EXPECT_FALSE(did_receive_redirect_);
    did_receive_redirect_ = true;

    if (delete_on_receive_redirect_)
      loader_.reset();
  }

  virtual void didSendData(blink::WebURLLoader* loader,
                           unsigned long long bytesSent,
                           unsigned long long totalBytesToBeSent) OVERRIDE {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
  }

  virtual void didReceiveResponse(
      blink::WebURLLoader* loader,
      const blink::WebURLResponse& response) OVERRIDE {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);

    // Only multipart requests may receive multiple response headers.
    EXPECT_TRUE(expect_multipart_response_ || !did_receive_response_);

    did_receive_response_ = true;
    if (delete_on_receive_response_)
      loader_.reset();
  }

  virtual void didDownloadData(blink::WebURLLoader* loader,
                               int dataLength,
                               int encodedDataLength) OVERRIDE {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
  }

  virtual void didReceiveData(blink::WebURLLoader* loader,
                              const char* data,
                              int dataLength,
                              int encodedDataLength) OVERRIDE {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
    // The response should have started, but must not have finished, or failed.
    EXPECT_TRUE(did_receive_response_);
    EXPECT_FALSE(did_finish_);
    EXPECT_EQ(net::OK, error_.reason);
    EXPECT_EQ("", error_.domain.utf8());

    received_data_.append(data, dataLength);

    if (delete_on_receive_data_)
      loader_.reset();
  }

  virtual void didReceiveCachedMetadata(blink::WebURLLoader* loader,
                                        const char* data,
                                        int dataLength) OVERRIDE {
    EXPECT_EQ(loader_.get(), loader);
  }

  virtual void didFinishLoading(blink::WebURLLoader* loader,
                                double finishTime,
                                int64_t totalEncodedDataLength) OVERRIDE {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
    EXPECT_TRUE(did_receive_response_);
    EXPECT_FALSE(did_finish_);
    did_finish_ = true;

    if (delete_on_finish_)
      loader_.reset();
  }

  virtual void didFail(blink::WebURLLoader* loader,
                       const blink::WebURLError& error) OVERRIDE {
    EXPECT_TRUE(loader_);
    EXPECT_EQ(loader_.get(), loader);
    EXPECT_FALSE(did_finish_);
    error_ = error;

    if (delete_on_fail_)
      loader_.reset();
  }

  WebURLLoaderImpl* loader() { return loader_.get(); }
  void DeleteLoader() {
    loader_.reset();
  }

  void set_expect_multipart_response() { expect_multipart_response_ = true; }

  void set_delete_on_receive_redirect() { delete_on_receive_redirect_ = true; }
  void set_delete_on_receive_response() { delete_on_receive_response_ = true; }
  void set_delete_on_receive_data() { delete_on_receive_data_ = true; }
  void set_delete_on_finish() { delete_on_finish_ = true; }
  void set_delete_on_fail() { delete_on_fail_ = true; }

  bool did_receive_redirect() const { return did_receive_redirect_; }
  bool did_receive_response() const { return did_receive_response_; }
  const std::string& received_data() const { return received_data_; }
  bool did_finish() const { return did_finish_; }
  const blink::WebURLError& error() const { return error_; }

 private:
  scoped_ptr<WebURLLoaderImpl> loader_;

  bool expect_multipart_response_;

  bool delete_on_receive_redirect_;
  bool delete_on_receive_response_;
  bool delete_on_receive_data_;
  bool delete_on_finish_;
  bool delete_on_fail_;

  bool did_receive_redirect_;
  bool did_receive_response_;
  std::string received_data_;
  bool did_finish_;
  blink::WebURLError error_;

  DISALLOW_COPY_AND_ASSIGN(TestWebURLLoaderClient);
};

class WebURLLoaderImplTest : public testing::Test {
 public:
  explicit WebURLLoaderImplTest() : client_(&dispatcher_) {}
  virtual ~WebURLLoaderImplTest() {}

  void DoStartAsyncRequest() {
    blink::WebURLRequest request;
    request.initialize();
    request.setURL(GURL(kTestURL));
    client()->loader()->loadAsynchronously(request, client());
    ASSERT_TRUE(bridge());
    ASSERT_TRUE(peer());
  }

  void DoReceiveRedirect() {
    EXPECT_FALSE(client()->did_receive_redirect());
    net::RedirectInfo redirect_info;
    redirect_info.status_code = 302;
    redirect_info.new_method = "GET";
    redirect_info.new_url = GURL(kTestURL);
    redirect_info.new_first_party_for_cookies = GURL(kTestURL);
    peer()->OnReceivedRedirect(redirect_info,
                               content::ResourceResponseInfo());
    EXPECT_TRUE(client()->did_receive_redirect());
  }

  void DoReceiveResponse() {
    EXPECT_FALSE(client()->did_receive_response());
    peer()->OnReceivedResponse(content::ResourceResponseInfo());
    EXPECT_TRUE(client()->did_receive_response());
  }

  // Assumes it is called only once for a request.
  void DoReceiveData() {
    EXPECT_EQ("", client()->received_data());
    peer()->OnReceivedData(kTestData, strlen(kTestData), strlen(kTestData));
    EXPECT_EQ(kTestData, client()->received_data());
  }

  void DoCompleteRequest() {
    EXPECT_FALSE(client()->did_finish());
    peer()->OnCompletedRequest(net::OK, false, false, "", base::TimeTicks(),
                               strlen(kTestData));
    EXPECT_TRUE(client()->did_finish());
    // There should be no error.
    EXPECT_EQ(net::OK, client()->error().reason);
    EXPECT_EQ("", client()->error().domain.utf8());
  }

  void DoFailRequest() {
    EXPECT_FALSE(client()->did_finish());
    peer()->OnCompletedRequest(net::ERR_FAILED, false, false, "",
                               base::TimeTicks(), strlen(kTestData));
    EXPECT_FALSE(client()->did_finish());
    EXPECT_EQ(net::ERR_FAILED, client()->error().reason);
    EXPECT_EQ(net::kErrorDomain, client()->error().domain.utf8());
  }

  void DoReceiveResponseFtp() {
    EXPECT_FALSE(client()->did_receive_response());
    content::ResourceResponseInfo response_info;
    response_info.mime_type = kFtpDirMimeType;
    peer()->OnReceivedResponse(response_info);
    EXPECT_TRUE(client()->did_receive_response());
  }

  void DoReceiveDataFtp() {
    peer()->OnReceivedData(kFtpDirListing, strlen(kFtpDirListing),
                           strlen(kFtpDirListing));
    // The FTP delegate should modify the data the client sees.
    EXPECT_NE(kFtpDirListing, client()->received_data());
  }

  void DoReceiveResponseMultipart() {
    EXPECT_FALSE(client()->did_receive_response());
    content::ResourceResponseInfo response_info;
    response_info.headers = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(kMultipartResponseHeaders,
                                          strlen(kMultipartResponseHeaders)));
    response_info.mime_type = kMultipartResponseMimeType;
    peer()->OnReceivedResponse(response_info);
    EXPECT_TRUE(client()->did_receive_response());
  }

  void DoReceiveDataMultipart() {
    peer()->OnReceivedData(kMultipartResponse, strlen(kMultipartResponse),
                           strlen(kMultipartResponse));
    // Multipart delegate should modify the data the client sees.
    EXPECT_NE(kMultipartResponse, client()->received_data());
  }

  TestWebURLLoaderClient* client() { return &client_; }
  TestBridge* bridge() { return dispatcher_.bridge(); }
  RequestPeer* peer() { return bridge()->peer(); }
  base::MessageLoop* message_loop() { return &message_loop_; }

 private:
  TestResourceDispatcher dispatcher_;
  TestWebURLLoaderClient client_;

  base::MessageLoop message_loop_;
};

TEST_F(WebURLLoaderImplTest, Success) {
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoCompleteRequest();
  EXPECT_FALSE(bridge()->canceled());
  EXPECT_EQ(kTestData, client()->received_data());
}

TEST_F(WebURLLoaderImplTest, Redirect) {
  DoStartAsyncRequest();
  DoReceiveRedirect();
  DoReceiveResponse();
  DoReceiveData();
  DoCompleteRequest();
  EXPECT_FALSE(bridge()->canceled());
  EXPECT_EQ(kTestData, client()->received_data());
}

TEST_F(WebURLLoaderImplTest, Failure) {
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoFailRequest();
  EXPECT_FALSE(bridge()->canceled());
}

// The client may delete the WebURLLoader during any callback from the loader.
// These tests make sure that doesn't result in a crash.
TEST_F(WebURLLoaderImplTest, DeleteOnReceiveRedirect) {
  client()->set_delete_on_receive_redirect();
  DoStartAsyncRequest();
  DoReceiveRedirect();
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, DeleteOnReceiveResponse) {
  client()->set_delete_on_receive_response();
  DoStartAsyncRequest();
  DoReceiveResponse();
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, DeleteOnReceiveData) {
  client()->set_delete_on_receive_data();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, DeleteOnFinish) {
  client()->set_delete_on_finish();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoCompleteRequest();
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, DeleteOnFail) {
  client()->set_delete_on_fail();
  DoStartAsyncRequest();
  DoReceiveResponse();
  DoReceiveData();
  DoFailRequest();
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, DeleteBeforeResponseDataURL) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->loader()->loadAsynchronously(request, client());
  client()->DeleteLoader();
  message_loop()->RunUntilIdle();
  EXPECT_FALSE(client()->did_receive_response());
  EXPECT_FALSE(bridge());
}

// Data URL tests.

TEST_F(WebURLLoaderImplTest, DataURL) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->loader()->loadAsynchronously(request, client());
  message_loop()->RunUntilIdle();
  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_TRUE(client()->did_finish());
  EXPECT_EQ(net::OK, client()->error().reason);
  EXPECT_EQ("", client()->error().domain.utf8());
}

TEST_F(WebURLLoaderImplTest, DataURLDeleteOnReceiveResponse) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->set_delete_on_receive_response();
  client()->loader()->loadAsynchronously(request, client());
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(client()->did_receive_response());
  EXPECT_EQ("", client()->received_data());
  EXPECT_FALSE(client()->did_finish());
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, DataURLDeleteOnReceiveData) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->set_delete_on_receive_data();
  client()->loader()->loadAsynchronously(request, client());
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(client()->did_receive_response());
  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_FALSE(client()->did_finish());
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, DataURLDeleteOnFinisha) {
  blink::WebURLRequest request;
  request.initialize();
  request.setURL(GURL("data:text/html;charset=utf-8,blah!"));
  client()->set_delete_on_finish();
  client()->loader()->loadAsynchronously(request, client());
  message_loop()->RunUntilIdle();
  EXPECT_TRUE(client()->did_receive_response());
  EXPECT_EQ("blah!", client()->received_data());
  EXPECT_TRUE(client()->did_finish());
  EXPECT_FALSE(bridge());
}

// FTP integration tests.  These are focused more on safe deletion than correct
// parsing of FTP responses.

TEST_F(WebURLLoaderImplTest, Ftp) {
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();
  DoCompleteRequest();
  EXPECT_FALSE(bridge()->canceled());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnReceiveResponse) {
  client()->set_delete_on_receive_response();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();

  // No data should have been received.
  EXPECT_EQ("", client()->received_data());
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnReceiveFirstData) {
  client()->set_delete_on_receive_data();
  DoStartAsyncRequest();
  // Some data is sent in ReceiveResponse for FTP requests, so the bridge should
  // be deleted here.
  DoReceiveResponseFtp();

  EXPECT_NE("", client()->received_data());
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnReceiveMoreData) {
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();

  // Directory listings are only parsed once the request completes, so this will
  // cancel in DoReceiveDataFtp, before the request finishes.
  client()->set_delete_on_receive_data();
  peer()->OnCompletedRequest(net::OK, false, false, "", base::TimeTicks(),
                              strlen(kTestData));
  EXPECT_FALSE(client()->did_finish());

  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnFinish) {
  client()->set_delete_on_finish();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();
  DoCompleteRequest();
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, FtpDeleteOnFail) {
  client()->set_delete_on_fail();
  DoStartAsyncRequest();
  DoReceiveResponseFtp();
  DoReceiveDataFtp();
  DoFailRequest();
  EXPECT_FALSE(bridge());
}

// Multipart integration tests.  These are focused more on safe deletion than
// correct parsing of Multipart responses.

TEST_F(WebURLLoaderImplTest, Multipart) {
  client()->set_expect_multipart_response();
  DoStartAsyncRequest();
  DoReceiveResponseMultipart();
  DoReceiveDataMultipart();
  DoCompleteRequest();
  EXPECT_EQ(kTestData, client()->received_data());
  EXPECT_FALSE(bridge()->canceled());
}

TEST_F(WebURLLoaderImplTest, MultipartDeleteOnReceiveFirstResponse) {
  client()->set_expect_multipart_response();
  client()->set_delete_on_receive_response();
  DoStartAsyncRequest();
  DoReceiveResponseMultipart();
  EXPECT_EQ("", client()->received_data());
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, MultipartDeleteOnReceiveSecondResponse) {
  client()->set_expect_multipart_response();
  DoStartAsyncRequest();
  DoReceiveResponseMultipart();
  client()->set_delete_on_receive_response();
  DoReceiveDataMultipart();
  EXPECT_EQ("", client()->received_data());
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, MultipartDeleteOnReceiveFirstData) {
  client()->set_expect_multipart_response();
  client()->set_delete_on_receive_data();
  DoStartAsyncRequest();
  DoReceiveResponseMultipart();
  DoReceiveDataMultipart();
  EXPECT_EQ("bl", client()->received_data());
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, MultipartDeleteOnReceiveMoreData) {
  client()->set_expect_multipart_response();
  DoStartAsyncRequest();
  DoReceiveResponseMultipart();
  DoReceiveDataMultipart();
  // For multipart responses, the delegate may send some data when notified
  // of a request completing.
  client()->set_delete_on_receive_data();
  peer()->OnCompletedRequest(net::OK, false, false, "", base::TimeTicks(),
                              strlen(kTestData));
  EXPECT_FALSE(client()->did_finish());
  EXPECT_EQ(kTestData, client()->received_data());
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, MultipartDeleteFinish) {
  client()->set_expect_multipart_response();
  client()->set_delete_on_finish();
  DoStartAsyncRequest();
  DoReceiveResponseMultipart();
  DoReceiveDataMultipart();
  DoCompleteRequest();
  EXPECT_EQ(kTestData, client()->received_data());
  EXPECT_FALSE(bridge());
}

TEST_F(WebURLLoaderImplTest, MultipartDeleteFail) {
  client()->set_expect_multipart_response();
  client()->set_delete_on_fail();
  DoStartAsyncRequest();
  DoReceiveResponseMultipart();
  DoReceiveDataMultipart();
  DoFailRequest();
  EXPECT_FALSE(bridge());
}

}  // namespace
}  // namespace content
