// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/local_discovery/privetv3_session.h"

#include "chrome/browser/local_discovery/privet_http.h"
#include "content/public/test/test_utils.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace local_discovery {

namespace {

using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::StrictMock;
using testing::_;

class PrivetV3SessionTest : public testing::Test {
 public:
  PrivetV3SessionTest() : session_(scoped_ptr<PrivetHTTPClient>()) {}

  virtual ~PrivetV3SessionTest() {}

  MOCK_METHOD2(OnInitialized,
               void(PrivetV3Session::Result,
                    const std::vector<PrivetV3Session::PairingType>&));
  MOCK_METHOD1(OnPairingStarted, void(PrivetV3Session::Result));
  MOCK_METHOD1(OnCodeConfirmed, void(PrivetV3Session::Result));
  MOCK_METHOD2(OnMessageSend,
               void(PrivetV3Session::Result,
                    const base::DictionaryValue& value));

 protected:
  virtual void SetUp() override {
    EXPECT_CALL(*this, OnInitialized(_, _)).Times(0);
    EXPECT_CALL(*this, OnPairingStarted(_)).Times(0);
    EXPECT_CALL(*this, OnCodeConfirmed(_)).Times(0);
  }

  PrivetV3Session session_;
  base::MessageLoop loop_;
  base::Closure quit_closure_;
};

TEST_F(PrivetV3SessionTest, Pairing) {
  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this,
                OnInitialized(PrivetV3Session::Result::STATUS_SUCCESS, _))
        .Times(1)
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    session_.Init(base::Bind(&PrivetV3SessionTest::OnInitialized,
                             base::Unretained(this)));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this,
                OnPairingStarted(PrivetV3Session::Result::STATUS_SUCCESS))
        .Times(1)
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    session_.StartPairing(PrivetV3Session::PairingType::PAIRING_TYPE_PINCODE,
                          base::Bind(&PrivetV3SessionTest::OnPairingStarted,
                                     base::Unretained(this)));
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(*this, OnCodeConfirmed(PrivetV3Session::Result::STATUS_SUCCESS))
        .Times(1)
        .WillOnce(InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    session_.ConfirmCode("1234",
                         base::Bind(&PrivetV3SessionTest::OnCodeConfirmed,
                                    base::Unretained(this)));
    run_loop.Run();
  }
}

// TODO(vitalybuka): replace PrivetHTTPClient with regular URL fetcher and
// implement SendMessage test.

}  // namespace

}  // namespace local_discovery
