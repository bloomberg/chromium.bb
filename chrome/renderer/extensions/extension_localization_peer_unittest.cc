// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "chrome/renderer/extensions/extension_localization_peer.h"
#include "extensions/common/message_bundle.h"
#include "ipc/ipc_sender.h"
#include "ipc/ipc_sync_message.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::StrEq;
using testing::Return;

static const char* const kExtensionUrl_1 =
    "chrome-extension://some_id/popup.css";

static const char* const kExtensionUrl_2 =
    "chrome-extension://some_id2/popup.css";

static const char* const kExtensionUrl_3 =
    "chrome-extension://some_id3/popup.css";

void MessageDeleter(IPC::Message* message) {
  delete message;
}

class MockIpcMessageSender : public IPC::Sender {
 public:
  MockIpcMessageSender() {
    ON_CALL(*this, Send(_))
        .WillByDefault(DoAll(Invoke(MessageDeleter), Return(true)));
  }

  virtual ~MockIpcMessageSender() {}

  MOCK_METHOD1(Send, bool(IPC::Message* message));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockIpcMessageSender);
};

class MockResourceLoaderBridgePeer : public content::RequestPeer {
 public:
  MockResourceLoaderBridgePeer() {}
  virtual ~MockResourceLoaderBridgePeer() {}

  MOCK_METHOD2(OnUploadProgress, void(uint64 position, uint64 size));
  MOCK_METHOD3(OnReceivedRedirect,
               bool(const GURL& new_url,
                    const GURL& new_first_party_for_cookies,
                    const content::ResourceResponseInfo& info));
  MOCK_METHOD1(OnReceivedResponse,
               void(const content::ResourceResponseInfo& info));
  MOCK_METHOD2(OnDownloadedData, void(int len, int encoded_data_length));
  MOCK_METHOD3(OnReceivedData, void(const char* data,
                                    int data_length,
                                    int encoded_data_length));
  MOCK_METHOD6(OnCompletedRequest, void(
      int error_code,
      bool was_ignored_by_handler,
      bool stale_copy_in_cache,
      const std::string& security_info,
      const base::TimeTicks& completion_time,
      int64_t total_transfer_size));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockResourceLoaderBridgePeer);
};

class ExtensionLocalizationPeerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    sender_.reset(new MockIpcMessageSender());
    original_peer_.reset(new MockResourceLoaderBridgePeer());
    filter_peer_.reset(
        ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
            original_peer_.get(), sender_.get(), "text/css",
            GURL(kExtensionUrl_1)));
  }

  ExtensionLocalizationPeer* CreateExtensionLocalizationPeer(
      const std::string& mime_type,
      const GURL& request_url) {
    return ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
        original_peer_.get(), sender_.get(), mime_type, request_url);
  }

  std::string GetData(ExtensionLocalizationPeer* filter_peer) {
    EXPECT_TRUE(NULL != filter_peer);
    return filter_peer->data_;
  }

  void SetData(ExtensionLocalizationPeer* filter_peer,
               const std::string& data) {
    EXPECT_TRUE(NULL != filter_peer);
    filter_peer->data_ = data;
  }

  scoped_ptr<MockIpcMessageSender> sender_;
  scoped_ptr<MockResourceLoaderBridgePeer> original_peer_;
  scoped_ptr<ExtensionLocalizationPeer> filter_peer_;
};

TEST_F(ExtensionLocalizationPeerTest, CreateWithWrongMimeType) {
  filter_peer_.reset(
      CreateExtensionLocalizationPeer("text/html", GURL(kExtensionUrl_1)));
  EXPECT_TRUE(NULL == filter_peer_.get());
}

TEST_F(ExtensionLocalizationPeerTest, CreateWithValidInput) {
  EXPECT_TRUE(NULL != filter_peer_.get());
}

TEST_F(ExtensionLocalizationPeerTest, OnReceivedData) {
  EXPECT_TRUE(GetData(filter_peer_.get()).empty());

  const std::string data_chunk("12345");
  filter_peer_->OnReceivedData(data_chunk.c_str(), data_chunk.length(), -1);

  EXPECT_EQ(data_chunk, GetData(filter_peer_.get()));

  filter_peer_->OnReceivedData(data_chunk.c_str(), data_chunk.length(), -1);
  EXPECT_EQ(data_chunk + data_chunk, GetData(filter_peer_.get()));
}

MATCHER_P(IsURLRequestEqual, status, "") { return arg.status() == status; }

TEST_F(ExtensionLocalizationPeerTest, OnCompletedRequestBadURLRequestStatus) {
  // It will self-delete once it exits OnCompletedRequest.
  ExtensionLocalizationPeer* filter_peer = filter_peer_.release();

  EXPECT_CALL(*original_peer_, OnReceivedResponse(_));
  EXPECT_CALL(*original_peer_, OnCompletedRequest(
      net::ERR_ABORTED, false, false, "", base::TimeTicks(), -1));

  filter_peer->OnCompletedRequest(
      net::ERR_FAILED, false, false, std::string(), base::TimeTicks(), -1);
}

TEST_F(ExtensionLocalizationPeerTest, OnCompletedRequestEmptyData) {
  // It will self-delete once it exits OnCompletedRequest.
  ExtensionLocalizationPeer* filter_peer = filter_peer_.release();

  EXPECT_CALL(*original_peer_, OnReceivedData(_, _, _)).Times(0);
  EXPECT_CALL(*sender_, Send(_)).Times(0);

  EXPECT_CALL(*original_peer_, OnReceivedResponse(_));
  EXPECT_CALL(*original_peer_, OnCompletedRequest(
      net::OK, false, false, "", base::TimeTicks(), -1));

  filter_peer->OnCompletedRequest(
      net::OK, false, false, std::string(), base::TimeTicks(), -1);
}

TEST_F(ExtensionLocalizationPeerTest, OnCompletedRequestNoCatalogs) {
  // It will self-delete once it exits OnCompletedRequest.
  ExtensionLocalizationPeer* filter_peer = filter_peer_.release();

  SetData(filter_peer, "some text");

  EXPECT_CALL(*sender_, Send(_));

  std::string data = GetData(filter_peer);
  EXPECT_CALL(*original_peer_,
              OnReceivedData(StrEq(data.data()), data.length(), -1)).Times(2);

  EXPECT_CALL(*original_peer_, OnReceivedResponse(_)).Times(2);
  EXPECT_CALL(*original_peer_, OnCompletedRequest(
      net::OK, false, false, "", base::TimeTicks(), -1)).Times(2);

  filter_peer->OnCompletedRequest(
      net::OK, false, false, std::string(), base::TimeTicks(), -1);

  // Test if Send gets called again (it shouldn't be) when first call returned
  // an empty dictionary.
  filter_peer =
      CreateExtensionLocalizationPeer("text/css", GURL(kExtensionUrl_1));
  SetData(filter_peer, "some text");
  filter_peer->OnCompletedRequest(
      net::OK, false, false, std::string(), base::TimeTicks(), -1);
}

TEST_F(ExtensionLocalizationPeerTest, OnCompletedRequestWithCatalogs) {
  // It will self-delete once it exits OnCompletedRequest.
  ExtensionLocalizationPeer* filter_peer =
      CreateExtensionLocalizationPeer("text/css", GURL(kExtensionUrl_2));

  extensions::L10nMessagesMap messages;
  messages.insert(std::make_pair("text", "new text"));
  extensions::ExtensionToL10nMessagesMap& l10n_messages_map =
      *extensions::GetExtensionToL10nMessagesMap();
  l10n_messages_map["some_id2"] = messages;

  SetData(filter_peer, "some __MSG_text__");

  // We already have messages in memory, Send will be skipped.
  EXPECT_CALL(*sender_, Send(_)).Times(0);

  // __MSG_text__ gets replaced with "new text".
  std::string data("some new text");
  EXPECT_CALL(*original_peer_,
              OnReceivedData(StrEq(data.data()), data.length(), -1));

  EXPECT_CALL(*original_peer_, OnReceivedResponse(_));
  EXPECT_CALL(*original_peer_, OnCompletedRequest(
      net::OK, false, false, "", base::TimeTicks(), -1));

  filter_peer->OnCompletedRequest(
      net::OK, false, false, std::string(), base::TimeTicks(), -1);
}

TEST_F(ExtensionLocalizationPeerTest, OnCompletedRequestReplaceMessagesFails) {
  // It will self-delete once it exits OnCompletedRequest.
  ExtensionLocalizationPeer* filter_peer =
      CreateExtensionLocalizationPeer("text/css", GURL(kExtensionUrl_3));

  extensions::L10nMessagesMap messages;
  messages.insert(std::make_pair("text", "new text"));
  extensions::ExtensionToL10nMessagesMap& l10n_messages_map =
      *extensions::GetExtensionToL10nMessagesMap();
  l10n_messages_map["some_id3"] = messages;

  std::string message("some __MSG_missing_message__");
  SetData(filter_peer, message);

  // We already have messages in memory, Send will be skipped.
  EXPECT_CALL(*sender_, Send(_)).Times(0);

  // __MSG_missing_message__ is missing, so message stays the same.
  EXPECT_CALL(*original_peer_,
              OnReceivedData(StrEq(message.data()), message.length(), -1));

  EXPECT_CALL(*original_peer_, OnReceivedResponse(_));
  EXPECT_CALL(*original_peer_, OnCompletedRequest(
      net::OK, false, false, "", base::TimeTicks(), -1));

  filter_peer->OnCompletedRequest(
      net::OK, false, false, std::string(), base::TimeTicks(), -1);
}
