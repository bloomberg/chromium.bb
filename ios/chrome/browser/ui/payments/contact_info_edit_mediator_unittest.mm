// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/contact_info_edit_mediator.h"

#include "base/mac/foundation_util.h"
#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request_test_util.h"
#include "ios/chrome/browser/payments/test_payment_request.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"
#import "ios/chrome/browser/ui/payments/payment_request_edit_consumer.h"
#import "ios/chrome/browser/ui/payments/payment_request_editor_field.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/ocmock/OCMock/OCMock.h"
#include "third_party/ocmock/gtest_support.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class PaymentRequestContactInfoEditMediatorTest : public PlatformTest {
 protected:
  PaymentRequestContactInfoEditMediatorTest()
      : payment_request_(base::MakeUnique<TestPaymentRequest>(
            payment_request_test_util::CreateTestWebPaymentRequest(),
            &personal_data_manager_)) {}

  autofill::TestPersonalDataManager personal_data_manager_;
  std::unique_ptr<TestPaymentRequest> payment_request_;
};

// Tests that the expected editor fields are created when creating a profile.
TEST_F(PaymentRequestContactInfoEditMediatorTest, TestFieldsWhenCreate) {
  id check_block = ^BOOL(id value) {
    EXPECT_TRUE([value isKindOfClass:[NSArray class]]);
    NSArray* fields = base::mac::ObjCCastStrict<NSArray>(value);
    EXPECT_EQ(3U, fields.count);

    id field = fields[0];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    EditorField* editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileFullName, editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(
                            IDS_PAYMENTS_NAME_FIELD_IN_CONTACT_DETAILS)]);
    EXPECT_EQ(nil, editor_field.value);

    field = fields[1];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileHomePhoneWholeNumber,
              editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(
                            IDS_PAYMENTS_PHONE_FIELD_IN_CONTACT_DETAILS)]);
    EXPECT_EQ(nil, editor_field.value);

    field = fields[2];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileEmailAddress, editor_field.autofillUIType);
    EXPECT_EQ(EditorFieldTypeTextField, editor_field.fieldType);
    EXPECT_TRUE([editor_field.label
        isEqualToString:l10n_util::GetNSString(
                            IDS_PAYMENTS_EMAIL_FIELD_IN_CONTACT_DETAILS)]);
    EXPECT_EQ(nil, editor_field.value);

    return YES;
  };

  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentRequestEditConsumer)];
  [[consumer expect] setEditorFields:[OCMArg checkWithBlock:check_block]];

  ContactInfoEditMediator* mediator = [[ContactInfoEditMediator alloc]
      initWithPaymentRequest:payment_request_.get()
                     profile:nil];
  [mediator setConsumer:consumer];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that the expected editor fields are created when editing a profile.
TEST_F(PaymentRequestContactInfoEditMediatorTest, TestFieldsWhenEdit) {
  id check_block = ^BOOL(id value) {
    EXPECT_TRUE([value isKindOfClass:[NSArray class]]);
    NSArray* fields = base::mac::ObjCCastStrict<NSArray>(value);
    EXPECT_EQ(3U, fields.count);

    id field = fields[0];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    EditorField* editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"John H. Doe"]);

    field = fields[1];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"+1 650-211-1111"]);

    field = fields[2];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_TRUE([editor_field.value isEqualToString:@"johndoe@hades.com"]);

    return YES;
  };

  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentRequestEditConsumer)];
  [[consumer expect] setEditorFields:[OCMArg checkWithBlock:check_block]];

  autofill::AutofillProfile autofill_profile = autofill::test::GetFullProfile();
  ContactInfoEditMediator* mediator = [[ContactInfoEditMediator alloc]
      initWithPaymentRequest:payment_request_.get()
                     profile:&autofill_profile];
  [mediator setConsumer:consumer];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that the expected editor fields are created when only name is
// requested.
TEST_F(PaymentRequestContactInfoEditMediatorTest, TestFieldsRequestNameOnly) {
  id check_block = ^BOOL(id value) {
    EXPECT_TRUE([value isKindOfClass:[NSArray class]]);
    NSArray* fields = base::mac::ObjCCastStrict<NSArray>(value);
    EXPECT_EQ(1U, fields.count);

    id field = fields[0];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    EditorField* editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileFullName, editor_field.autofillUIType);

    return YES;
  };

  payment_request_->web_payment_request().options.request_payer_phone = false;
  payment_request_->web_payment_request().options.request_payer_email = false;

  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentRequestEditConsumer)];
  [[consumer expect] setEditorFields:[OCMArg checkWithBlock:check_block]];

  ContactInfoEditMediator* mediator = [[ContactInfoEditMediator alloc]
      initWithPaymentRequest:payment_request_.get()
                     profile:nil];
  [mediator setConsumer:consumer];

  EXPECT_OCMOCK_VERIFY(consumer);
}

// Tests that the expected editor fields are created when phone number and email
// are requested.
TEST_F(PaymentRequestContactInfoEditMediatorTest, TestFieldsRequestPhoneEmail) {
  id check_block = ^BOOL(id value) {
    EXPECT_TRUE([value isKindOfClass:[NSArray class]]);
    NSArray* fields = base::mac::ObjCCastStrict<NSArray>(value);
    EXPECT_EQ(2U, fields.count);

    id field = fields[0];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    EditorField* editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileHomePhoneWholeNumber,
              editor_field.autofillUIType);

    field = fields[1];
    EXPECT_TRUE([field isKindOfClass:[EditorField class]]);
    editor_field = base::mac::ObjCCastStrict<EditorField>(field);
    EXPECT_EQ(AutofillUITypeProfileEmailAddress, editor_field.autofillUIType);

    return YES;
  };

  payment_request_->web_payment_request().options.request_payer_name = false;

  // Mock the consumer.
  id consumer =
      [OCMockObject mockForProtocol:@protocol(PaymentRequestEditConsumer)];
  [[consumer expect] setEditorFields:[OCMArg checkWithBlock:check_block]];

  ContactInfoEditMediator* mediator = [[ContactInfoEditMediator alloc]
      initWithPaymentRequest:payment_request_.get()
                     profile:nil];
  [mediator setConsumer:consumer];

  EXPECT_OCMOCK_VERIFY(consumer);
}
