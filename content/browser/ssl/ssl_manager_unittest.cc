// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ssl/ssl_manager.h"

#include "base/macros.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/frame_messages.h"
#include "content/public/browser/ssl_status.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// A WebContentsDelegate that exposes the visible SSLStatus at the time
// of the last VisibleSecurityStateChanged() call.
class TestWebContentsDelegate : public WebContentsDelegate {
 public:
  TestWebContentsDelegate() : WebContentsDelegate() {}
  ~TestWebContentsDelegate() override {}

  const SSLStatus& last_ssl_state() { return last_ssl_state_; }

  // WebContentsDelegate:
  void VisibleSecurityStateChanged(WebContents* source) override {
    NavigationEntry* entry = source->GetController().GetVisibleEntry();
    EXPECT_TRUE(entry);
    last_ssl_state_ = entry->GetSSL();
  }

 private:
  SSLStatus last_ssl_state_;
  DISALLOW_COPY_AND_ASSIGN(TestWebContentsDelegate);
};

class SSLManagerTest : public RenderViewHostTestHarness {
 public:
  SSLManagerTest() : RenderViewHostTestHarness() {}
  ~SSLManagerTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SSLManagerTest);
};

// Tests that VisibleSecurityStateChanged() is called when a password input
// is shown on an HTTP page.
TEST_F(SSLManagerTest, NotifyVisibleSSLStateChangeOnHttpPassword) {
  TestWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);
  SSLManager manager(
      static_cast<NavigationControllerImpl*>(&web_contents()->GetController()));

  NavigateAndCommit(GURL("http://example.test"));
  EXPECT_FALSE(delegate.last_ssl_state().content_status &
               SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  web_contents()->OnPasswordInputShownOnHttp();
  EXPECT_TRUE(delegate.last_ssl_state().content_status &
              SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
}

// Tests that VisibleSecurityStateChanged() is called when a credit card input
// is shown on an HTTP page.
TEST_F(SSLManagerTest, NotifyVisibleSSLStateChangeOnHttpCreditCard) {
  TestWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);
  SSLManager manager(
      static_cast<NavigationControllerImpl*>(&web_contents()->GetController()));

  NavigateAndCommit(GURL("http://example.test"));
  EXPECT_FALSE(delegate.last_ssl_state().content_status &
               SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
  web_contents()->OnCreditCardInputShownOnHttp();
  EXPECT_TRUE(delegate.last_ssl_state().content_status &
              SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
}

// Tests that VisibleSecurityStateChanged() is called when password and
// credit card inputs are shown on an HTTP page.
TEST_F(SSLManagerTest, NotifyVisibleSSLStateChangeOnPasswordAndHttpCreditCard) {
  TestWebContentsDelegate delegate;
  web_contents()->SetDelegate(&delegate);
  SSLManager manager(
      static_cast<NavigationControllerImpl*>(&web_contents()->GetController()));

  NavigateAndCommit(GURL("http://example.test"));
  EXPECT_FALSE(delegate.last_ssl_state().content_status &
               SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  EXPECT_FALSE(delegate.last_ssl_state().content_status &
               SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
  web_contents()->OnPasswordInputShownOnHttp();
  web_contents()->OnCreditCardInputShownOnHttp();
  EXPECT_TRUE(delegate.last_ssl_state().content_status &
              SSLStatus::DISPLAYED_PASSWORD_FIELD_ON_HTTP);
  EXPECT_TRUE(delegate.last_ssl_state().content_status &
              SSLStatus::DISPLAYED_CREDIT_CARD_FIELD_ON_HTTP);
}

// Returns true if the given |sink| has received a Symantec warning console
// message, and false otherwise.
bool FindSymantecConsoleMessage(const IPC::TestSink& sink) {
  for (size_t i = 0; i < sink.message_count(); i++) {
    const IPC::Message* message = sink.GetMessageAt(i);
    if (message->type() != FrameMsg_AddMessageToConsole::ID)
      continue;
    std::tuple<ConsoleMessageLevel, std::string> params;
    FrameMsg_AddMessageToConsole::Read(message, &params);
    if (std::get<0>(params) == CONSOLE_MESSAGE_LEVEL_WARNING &&
        std::get<1>(params).find("will be distrusted") != std::string::npos) {
      return true;
    }
  }
  return false;
}

TEST_F(SSLManagerTest, SymantecConsoleMessage) {
  SSLManager manager(
      static_cast<NavigationControllerImpl*>(&web_contents()->GetController()));

  // Set up a navigation entry with a Symantec public key hash and simulate
  // navigation to that entry.
  NavigateAndCommit(GURL("https://example.test"));
  net::SHA256HashValue symantec_hash_value = {
      {0xb2, 0xde, 0xf5, 0x36, 0x2a, 0xd3, 0xfa, 0xcd, 0x04, 0xbd, 0x29,
       0x04, 0x7a, 0x43, 0x84, 0x4f, 0x76, 0x70, 0x34, 0xea, 0x48, 0x92,
       0xf8, 0x0e, 0x56, 0xbe, 0xe6, 0x90, 0x24, 0x3e, 0x25, 0x02}};
  web_contents()
      ->GetController()
      .GetLastCommittedEntry()
      ->GetSSL()
      .public_key_hashes.push_back(net::HashValue(symantec_hash_value));
  process()->sink().ClearMessages();
  // Navigate to the current entry, which now looks like a Symantec certificate.
  Reload();
  // Check that the expected console message was sent to the renderer to log.
  EXPECT_TRUE(FindSymantecConsoleMessage(process()->sink()));

  // Check that the console message is not logged when the certificate chain
  // includes an excluded subCA.
  net::SHA256HashValue google_hash_value = {
      {0xec, 0x72, 0x29, 0x69, 0xcb, 0x64, 0x20, 0x0a, 0xb6, 0x63, 0x8f,
       0x68, 0xac, 0x53, 0x8e, 0x40, 0xab, 0xab, 0x5b, 0x19, 0xa6, 0x48,
       0x56, 0x61, 0x04, 0x2a, 0x10, 0x61, 0xc4, 0x61, 0x27, 0x76}};
  web_contents()
      ->GetController()
      .GetLastCommittedEntry()
      ->GetSSL()
      .public_key_hashes.push_back(net::HashValue(google_hash_value));
  process()->sink().ClearMessages();
  // Navigate to the current entry, which now includes an excluded subCA.
  Reload();
  // Check that the expected console message was not sent to the renderer to
  // log.
  EXPECT_FALSE(FindSymantecConsoleMessage(process()->sink()));
}

}  // namespace

}  // namespace content
