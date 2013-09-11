// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USE_BRLAPI
#error This test requires brlapi.
#endif

#include <deque>

#include "base/bind.h"
#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using extensions::api::braille_display_private::BrailleController;
using extensions::api::braille_display_private::BrlapiConnection;

// Data maintained by the mock BrlapiConnection.  This data lives throughout
// a test, while the api implementation takes ownership of the connection
// itself.
struct MockBrlapiConnectionData {
  bool connected;
  size_t display_size;
  brlapi_error_t error;
  std::vector<std::string> written_content;
  std::deque<brlapi_keyCode_t> pending_keys;
};

class MockBrlapiConnection : public BrlapiConnection {
 public:
  MockBrlapiConnection(MockBrlapiConnectionData* data)
      : data_(data) {}
  virtual bool Connect(const OnDataReadyCallback& on_data_ready) OVERRIDE {
    data_->connected = true;
    on_data_ready_ = on_data_ready;
    if (!data_->pending_keys.empty()) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              base::Bind(&MockBrlapiConnection::NotifyDataReady,
                                        base::Unretained(this)));
    }
    return true;
  }

  virtual void Disconnect() OVERRIDE {
    data_->connected = false;
  }

  virtual bool Connected() OVERRIDE {
    return data_->connected;
  }

  virtual brlapi_error_t* BrlapiError() OVERRIDE {
    return &data_->error;
  }

  virtual std::string BrlapiStrError() OVERRIDE {
    return data_->error.brlerrno != BRLAPI_ERROR_SUCCESS ? "Error" : "Success";
  }

  virtual bool GetDisplaySize(size_t* size) OVERRIDE {
    *size = data_->display_size;
    return true;
  }

  virtual bool WriteDots(const unsigned char* cells) OVERRIDE {
    std::string written(reinterpret_cast<const char*>(cells),
                        data_->display_size);
    data_->written_content.push_back(written);
    return true;
  }

  virtual int ReadKey(brlapi_keyCode_t* keyCode) {
    if (!data_->pending_keys.empty()) {
      *keyCode = data_->pending_keys.front();
      data_->pending_keys.pop_front();
      return 1;
    } else {
      return 0;
    }
  }

 private:

  void NotifyDataReady() {
    on_data_ready_.Run();
    if (!data_->pending_keys.empty()) {
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                              base::Bind(&MockBrlapiConnection::NotifyDataReady,
                                        base::Unretained(this)));
    }
  }

  MockBrlapiConnectionData* data_;
  OnDataReadyCallback on_data_ready_;
};

class BrailleDisplayPrivateApiTest : public ExtensionApiTest {
 public:
  virtual void SetUpInProcessBrowserTestFixture() {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    connection_data_.connected = false;
    connection_data_.display_size = 0;
    connection_data_.error.brlerrno = BRLAPI_ERROR_SUCCESS;
    BrailleController::GetInstance()->SetCreateBrlapiConnectionForTesting(
        base::Bind(
            &BrailleDisplayPrivateApiTest::CreateBrlapiConnection,
            base::Unretained(this)));
  }

 protected:
  MockBrlapiConnectionData connection_data_;

 private:
  scoped_ptr<BrlapiConnection> CreateBrlapiConnection() {
    return scoped_ptr<BrlapiConnection>(
        new MockBrlapiConnection(&connection_data_));
  }
};

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateApiTest, WriteDots) {
  connection_data_.display_size = 11;
  ASSERT_TRUE(RunComponentExtensionTest("braille_display_private/write_dots"))
      << message_;
  ASSERT_EQ(3U, connection_data_.written_content.size());
  const std::string expected_content(connection_data_.display_size, '\0');
  for (size_t i = 0; i < connection_data_.written_content.size(); ++i) {
    ASSERT_EQ(std::string(
        connection_data_.display_size,
        static_cast<char>(i)),
                 connection_data_.written_content[i])
        << "String " << i << " doesn't match";
  }
}

IN_PROC_BROWSER_TEST_F(BrailleDisplayPrivateApiTest, KeyEvents) {
  connection_data_.display_size = 11;
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_CMD | BRLAPI_KEY_CMD_LNUP);
  connection_data_.pending_keys.push_back(
      BRLAPI_KEY_TYPE_CMD | BRLAPI_KEY_CMD_LNDN);
  ASSERT_TRUE(RunComponentExtensionTest("braille_display_private/key_events"));
}
