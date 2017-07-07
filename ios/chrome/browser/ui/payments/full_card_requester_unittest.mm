// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/full_card_requester.h"

#include <string>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/payments/core/autofill_payment_instrument.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/payment_request_data_util.h"
#import "ios/chrome/browser/autofill/autofill_agent.h"
#import "ios/chrome/browser/autofill/autofill_controller.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#import "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/chrome/test/scoped_key_window.h"
#import "ios/testing/ocmock_complex_type_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FullCardRequesterConsumerMock
    : OCMockComplexTypeHelper<FullCardRequesterConsumer>
@end

@implementation FullCardRequesterConsumerMock

typedef void (^mock_full_card_request_did_succeed_with_method_name)(
    const std::string&,
    const std::string&);

- (void)fullCardRequestDidSucceedWithMethodName:(const std::string&)methodName
                             stringifiedDetails:
                                 (const std::string&)stringifiedDetails {
  return static_cast<mock_full_card_request_did_succeed_with_method_name>(
      [self blockForSelector:_cmd])(methodName, stringifiedDetails);
}

@end

class FakeResultDelegate
    : public autofill::payments::FullCardRequest::ResultDelegate {
 public:
  FakeResultDelegate() : weak_ptr_factory_(this) {}
  ~FakeResultDelegate() override {}

  void OnFullCardRequestSucceeded(const autofill::CreditCard& card,
                                  const base::string16& cvc) override {}

  void OnFullCardRequestFailed() override {}

  base::WeakPtr<FakeResultDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<FakeResultDelegate> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FakeResultDelegate);
};

class PaymentRequestFullCardRequesterTest : public ChromeWebTest {
 protected:
  PaymentRequestFullCardRequesterTest()
      : credit_card_(autofill::test::GetCreditCard()),
        chrome_browser_state_(TestChromeBrowserState::Builder().Build()) {}

  void SetUp() override {
    ChromeWebTest::SetUp();

    // Set up what is needed to have an instance of autofill::AutofillManager.
    AutofillAgent* autofill_agent =
        [[AutofillAgent alloc] initWithBrowserState:chrome_browser_state_.get()
                                           webState:web_state()];
    InfoBarManagerImpl::CreateForWebState(web_state());
    autofill_controller_ = [[AutofillController alloc]
             initWithBrowserState:chrome_browser_state_.get()
                         webState:web_state()
                    autofillAgent:autofill_agent
        passwordGenerationManager:nullptr
                  downloadEnabled:NO];
  }

  void TearDown() override {
    [autofill_controller_ detachFromWebState];

    ChromeWebTest::TearDown();
  }

  autofill::CreditCard credit_card_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  // Manages autofill for a single page.
  AutofillController* autofill_controller_;
};

// Tests that the FullCardRequester presents and dismisses the card unmask
// prompt view controller, when the full card is requested and when the user
// enters the CVC/expiration information respectively.
TEST_F(PaymentRequestFullCardRequesterTest, PresentAndDismiss) {
  UIViewController* base_view_controller = [[UIViewController alloc] init];
  ScopedKeyWindow scoped_key_window_;
  [scoped_key_window_.Get() setRootViewController:base_view_controller];

  FullCardRequester full_card_requester(nil, base_view_controller,
                                        chrome_browser_state_.get());

  EXPECT_EQ(nil, base_view_controller.presentedViewController);

  autofill::AutofillManager* autofill_manager =
      autofill::AutofillDriverIOS::FromWebState(web_state())
          ->autofill_manager();
  FakeResultDelegate* fake_result_delegate = new FakeResultDelegate;
  full_card_requester.GetFullCard(credit_card_, autofill_manager,
                                  fake_result_delegate->GetWeakPtr());

  // Spin the run loop to trigger the animation.
  base::test::ios::SpinRunLoopWithMaxDelay(base::TimeDelta::FromSecondsD(1.0));
  EXPECT_TRUE([base_view_controller.presentedViewController
      isMemberOfClass:[CardUnmaskPromptViewController class]]);

  full_card_requester.OnUnmaskVerificationResult(
      autofill::AutofillClient::SUCCESS);

  // Wait until the view controller is ordered to be dismissed and the animation
  // completes.
  WaitForCondition(^bool() {
    return !base_view_controller.presentedViewController;
  });
  EXPECT_EQ(nil, base_view_controller.presentedViewController);
}

// Tests that calling the FullCardRequester's delegate method which signals that
// the full credit card details have been successfully received, causes the
// FullCardRequester's delegate method to get called.
TEST_F(PaymentRequestFullCardRequesterTest, InstrumentDetailsReady) {
  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(FullCardRequesterConsumer)];
  id consumer_mock([[FullCardRequesterConsumerMock alloc]
      initWithRepresentedObject:consumer]);
  SEL selector =
      @selector(fullCardRequestDidSucceedWithMethodName:stringifiedDetails:);
  [consumer_mock onSelector:selector
       callBlockExpectation:^(const std::string& methodName,
                              const std::string& stringifiedDetails) {
         EXPECT_EQ("visa", methodName);

         std::string cvc;
         std::unique_ptr<base::DictionaryValue> detailsDict =
             base::DictionaryValue::From(
                 base::JSONReader::Read(stringifiedDetails));
         detailsDict->GetString("cardSecurityCode", &cvc);
         EXPECT_EQ("123", cvc);
       }];

  FullCardRequester full_card_requester(consumer_mock, nil,
                                        chrome_browser_state_.get());

  autofill::AutofillProfile billing_address;

  std::unique_ptr<base::DictionaryValue> response_value =
      payments::data_util::GetBasicCardResponseFromAutofillCreditCard(
          credit_card_, base::ASCIIToUTF16("123"), billing_address, "en-US")
          .ToDictionaryValue();
  std::string stringifiedDetails;
  base::JSONWriter::Write(*response_value, &stringifiedDetails);

  full_card_requester.OnInstrumentDetailsReady("visa", stringifiedDetails);
}
