// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/can_make_payment_query.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/payments/core/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace payments {
namespace {

class CanMakePaymentQueryTest : public ::testing::Test {
 protected:
  CanMakePaymentQuery guard_;

 private:
  base::test::ScopedTaskEnvironment task_environment_;
};

// An HTTPS website is not allowed to query all of the networks of the cards in
// user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       SameHttpsOriginCannotQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(GURL("https://example.com"),
                              GURL("https://example.com"), {{"amex", {}}}));
  EXPECT_FALSE(guard_.CanQuery(GURL("https://example.com"),
                               GURL("https://example.com"), {{"visa", {}}}));
}

// A localhost website is not allowed to query all of the networks of the cards
// in user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       SameLocalhostOriginCannotQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(GURL("http://localhost:8080"),
                              GURL("http://localhost:8080"), {{"amex", {}}}));
  EXPECT_FALSE(guard_.CanQuery(GURL("http://localhost:8080"),
                               GURL("http://localhost:8080"), {{"visa", {}}}));
}

// A file website is not allowed to query all of the networks of the cards in
// user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       SameFileOriginCannotQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(GURL("file:///tmp/test.html"),
                              GURL("file:///tmp/test.html"), {{"amex", {}}}));
  EXPECT_FALSE(guard_.CanQuery(GURL("file:///tmp/test.html"),
                               GURL("file:///tmp/test.html"), {{"visa", {}}}));
}

// Different HTTPS websites are allowed to query different card networks in
// user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       DifferentHttpsOriginsCanQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(GURL("https://example.com"),
                              GURL("https://example.com"), {{"amex", {}}}));
  EXPECT_TRUE(guard_.CanQuery(GURL("https://not-example.com"),
                              GURL("https://not-example.com"), {{"visa", {}}}));
}

// Different localhost websites are allowed to query different card networks in
// user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       DifferentLocalhostOriginsCanQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(GURL("http://localhost:8080"),
                              GURL("http://localhost:8080"), {{"amex", {}}}));
  EXPECT_TRUE(guard_.CanQuery(GURL("http://localhost:9090"),
                              GURL("http://localhost:9090"), {{"visa", {}}}));
}

// Different file websites are allowed to query different card networks in
// user's autofill database.
TEST_F(CanMakePaymentQueryTest,
       DifferentFileOriginsCanQueryTwoDifferentCardNetworks) {
  EXPECT_TRUE(guard_.CanQuery(GURL("file:///tmp/test.html"),
                              GURL("file:///tmp/test.html"), {{"amex", {}}}));
  EXPECT_TRUE(guard_.CanQuery(GURL("file:///tmp/not-test.html"),
                              GURL("file:///tmp/not-test.html"),
                              {{"visa", {}}}));
}

// The same website is not allowed to query the same payment method with
// different parameters.
TEST_F(CanMakePaymentQueryTest,
       SameOriginCannotQueryBasicCardWithTwoDifferentCardNetworks) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kWebPaymentsPerMethodCanMakePaymentQuota);
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://alicepay.com", {"{alicePayParameter: 1}"}}}));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}}));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['amex']}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}}));
}

// Two different websites are allowed to query the same payment method with
// different parameters.
TEST_F(CanMakePaymentQueryTest,
       DifferentOriginsCanQueryBasicCardWithTwoDifferentCardNetworks) {
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}}}));
  EXPECT_TRUE(guard_.CanQuery(
      GURL("https://not-example.com"), GURL("https://not-example.com"),
      {{"basic-card", {"{supportedNetworks: ['amex']}"}}}));
}

// A website can query several different payment methods, as long as each
// payment method is queried with the same payment-method-specific data.
TEST_F(CanMakePaymentQueryTest,
       SameOriginCanQuerySeveralDifferentPaymentMethodIdentifiers) {
  base::test::ScopedFeatureList features;
  features.InitAndEnableFeature(
      features::kWebPaymentsPerMethodCanMakePaymentQuota);
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://alicepay.com", {"{alicePayParameter: 1}"}}}));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://alicepay.com", {"{alicePayParameter: 1}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}}));
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://bobpay.com", {"{bobPayParameter: 2}"}},
                       {"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

// A website cannot query several different payment methods without the
// per-method query feature, even if method-specific data remains unchanged.
TEST_F(CanMakePaymentQueryTest,
       SameOriginCannotQueryDifferentMethodsWithoutPerMethodQuota) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(
      features::kWebPaymentsPerMethodCanMakePaymentQuota);
  EXPECT_TRUE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}},
                       {"https://alicepay.com", {"{alicePayParameter: 1}"}}}));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://alicepay.com", {"{alicePayParameter: 1}"}},
                       {"https://bobpay.com", {"{bobPayParameter: 2}"}}}));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"https://bobpay.com", {"{bobPayParameter: 2}"}},
                       {"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

// A website is not allowed to query all of the networks of the cards in user's
// autofill database, even if they vary the format of the query.
TEST_F(
    CanMakePaymentQueryTest,
    SameOriginCannotQueryDifferentCardNetworksAndTypesUsingDifferentFormats) {
  EXPECT_TRUE(guard_.CanQuery(GURL("https://example.com"),
                              GURL("https://example.com"), {{"amex", {}}}));
  EXPECT_FALSE(
      guard_.CanQuery(GURL("https://example.com"), GURL("https://example.com"),
                      {{"basic-card", {"{supportedNetworks: ['visa']}"}}}));
}

}  // namespace
}  // namespace payments
