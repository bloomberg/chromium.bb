// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/notifications/notification_id_generator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

const int kRenderProcessId = 42;
const int64_t kPersistentNotificationId = 430;
const int kNonPersistentNotificationId = 5400;
const char kExampleTag[] = "example";

class NotificationIdGeneratorTest : public ::testing::Test {
 public:
  NotificationIdGeneratorTest()
      : origin_(url::Origin::Create(GURL("https://example.com"))) {}

 protected:
  url::Origin origin_;
  NotificationIdGenerator generator_;
};

// -----------------------------------------------------------------------------
// Persistent and non-persistent notifications

// Two calls to the generator with exactly the same information should result
// in exactly the same notification ids being generated.
TEST_F(NotificationIdGeneratorTest, DeterministicGeneration) {
  // Persistent notifications.
  EXPECT_EQ(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, kPersistentNotificationId));

  EXPECT_EQ(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), "" /* tag */, kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_.GetURL(), "" /* tag */, kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_EQ(generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), kExampleTag, kNonPersistentNotificationId,
                kRenderProcessId),
            generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), kExampleTag, kNonPersistentNotificationId,
                kRenderProcessId));

  EXPECT_EQ(generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), "" /* tag */, kNonPersistentNotificationId,
                kRenderProcessId),
            generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), "" /* tag */, kNonPersistentNotificationId,
                kRenderProcessId));
}

// The origin of the notification will impact the generated notification id.
TEST_F(NotificationIdGeneratorTest, DifferentOrigins) {
  url::Origin different_origin(
      url::Origin::Create(GURL("https://example2.com")));

  // Persistent notifications.
  EXPECT_NE(
      generator_.GenerateForPersistentNotification(
          origin_.GetURL(), kExampleTag, kPersistentNotificationId),
      generator_.GenerateForPersistentNotification(
          different_origin.GetURL(), kExampleTag, kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_NE(generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), kExampleTag, kNonPersistentNotificationId,
                kRenderProcessId),
            generator_.GenerateForNonPersistentNotification(
                different_origin.GetURL(), kExampleTag,
                kNonPersistentNotificationId, kRenderProcessId));
}

// The tag, when non-empty, will impact the generated notification id.
TEST_F(NotificationIdGeneratorTest, DifferentTags) {
  const std::string& different_tag = std::string(kExampleTag) + "2";

  // Persistent notifications.
  EXPECT_NE(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_.GetURL(), different_tag, kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_NE(generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), kExampleTag, kNonPersistentNotificationId,
                kRenderProcessId),
            generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), different_tag, kNonPersistentNotificationId,
                kRenderProcessId));
}

// The persistent or non-persistent notification id will impact the generated
// notification id when the tag is empty.
TEST_F(NotificationIdGeneratorTest, DifferentIds) {
  // Persistent notifications.
  EXPECT_NE(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), "" /* tag */, kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_.GetURL(), "" /* tag */, kPersistentNotificationId + 1));

  // Non-persistent notifications.
  EXPECT_NE(generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), "" /* tag */, kNonPersistentNotificationId,
                kRenderProcessId),
            generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), "" /* tag */,
                kNonPersistentNotificationId + 1, kRenderProcessId));

  // Non-persistent when a tag is being used.
  EXPECT_EQ(generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), kExampleTag, kNonPersistentNotificationId,
                kRenderProcessId),
            generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), kExampleTag, kNonPersistentNotificationId,
                kRenderProcessId + 1));
}

// Using a numeric tag that could resemble a persistent notification id should
// not be equal to a notification without a tag, but with that id.
TEST_F(NotificationIdGeneratorTest, NumericTagAmbiguity) {
  // Persistent notifications.
  EXPECT_NE(
      generator_.GenerateForPersistentNotification(
          origin_.GetURL(), base::Int64ToString(kPersistentNotificationId),
          kPersistentNotificationId),
      generator_.GenerateForPersistentNotification(
          origin_.GetURL(), "" /* tag */, kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_NE(
      generator_.GenerateForNonPersistentNotification(
          origin_.GetURL(), base::IntToString(kNonPersistentNotificationId),
          kNonPersistentNotificationId, kRenderProcessId),
      generator_.GenerateForNonPersistentNotification(
          origin_.GetURL(), "" /* tag */, kNonPersistentNotificationId,
          kRenderProcessId));
}

// Using port numbers and a tag which, when concatenated, could end up being
// equal to each other if origins stop ending with slashes.
TEST_F(NotificationIdGeneratorTest, OriginPortAmbiguity) {
  GURL origin_805("https://example.com:805");
  GURL origin_8051("https://example.com:8051");

  // Persistent notifications.
  EXPECT_NE(generator_.GenerateForPersistentNotification(
                origin_805, "17", kPersistentNotificationId),
            generator_.GenerateForPersistentNotification(
                origin_8051, "7", kPersistentNotificationId));

  // Non-persistent notifications.
  EXPECT_NE(
      generator_.GenerateForNonPersistentNotification(
          origin_805, "17", kNonPersistentNotificationId, kRenderProcessId),
      generator_.GenerateForNonPersistentNotification(
          origin_8051, "7", kNonPersistentNotificationId, kRenderProcessId));
}

// Verifies that the static Is(Non)PersistentNotification helpers can correctly
// identify persistent and non-persistent notification IDs.
TEST_F(NotificationIdGeneratorTest, DetectIdFormat) {
  std::string persistent_id = generator_.GenerateForPersistentNotification(
      origin_.GetURL(), "" /* tag */, kPersistentNotificationId);

  std::string non_persistent_id =
      generator_.GenerateForNonPersistentNotification(
          origin_.GetURL(), "" /* tag */, kNonPersistentNotificationId,
          kRenderProcessId);

  std::string non_persistent_id_using_token =
      generator_.GenerateForNonPersistentMojoNotification(origin_, "token");

  EXPECT_TRUE(NotificationIdGenerator::IsPersistentNotification(persistent_id));
  EXPECT_FALSE(
      NotificationIdGenerator::IsNonPersistentNotification(persistent_id));

  EXPECT_TRUE(
      NotificationIdGenerator::IsNonPersistentNotification(non_persistent_id));
  EXPECT_FALSE(
      NotificationIdGenerator::IsPersistentNotification(non_persistent_id));

  EXPECT_TRUE(NotificationIdGenerator::IsNonPersistentNotification(
      non_persistent_id_using_token));
  EXPECT_FALSE(NotificationIdGenerator::IsPersistentNotification(
      non_persistent_id_using_token));
}

// -----------------------------------------------------------------------------
// Persistent notifications
//
// Tests covering the logic specific to persistent notifications. This kind of
// notification does not care about the renderer process that created them.

TEST_F(NotificationIdGeneratorTest, PersistentDifferentRenderProcessIds) {
  NotificationIdGenerator second_generator;

  EXPECT_EQ(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, kPersistentNotificationId),
            second_generator.GenerateForPersistentNotification(
                origin_.GetURL(), kExampleTag, kPersistentNotificationId));

  EXPECT_EQ(generator_.GenerateForPersistentNotification(
                origin_.GetURL(), "" /* tag */, kPersistentNotificationId),
            second_generator.GenerateForPersistentNotification(
                origin_.GetURL(), "" /* tag */, kPersistentNotificationId));
}

// -----------------------------------------------------------------------------
// Non-persistent notifications
//
// Tests covering the logic specific to non-persistent notifications. This kind
// of notification cares about the renderer process they were created by when
// the notification does not have a tag, since multiple renderers would restart
// the count for non-persistent notification ids.

TEST_F(NotificationIdGeneratorTest, NonPersistentDifferentRenderProcessIds) {
  EXPECT_EQ(generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), kExampleTag, kNonPersistentNotificationId,
                kRenderProcessId),
            generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), kExampleTag, kNonPersistentNotificationId,
                kRenderProcessId + 1));

  EXPECT_NE(generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), "" /* tag */, kNonPersistentNotificationId,
                kRenderProcessId),
            generator_.GenerateForNonPersistentNotification(
                origin_.GetURL(), "" /* tag */, kNonPersistentNotificationId,
                kRenderProcessId + 1));
}

// Concatenation of the render process id and the non-persistent notification
// id should not result in the generation of a duplicated notification id.
TEST_F(NotificationIdGeneratorTest, NonPersistentRenderProcessIdAmbiguity) {
  EXPECT_NE(
      generator_.GenerateForNonPersistentNotification(
          origin_.GetURL(), "" /* tag */,
          1337 /* non_persistent_notification_id */, 5 /* render_process_id */),
      generator_.GenerateForNonPersistentNotification(
          origin_.GetURL(), "" /* tag */,
          337 /* non_persistent_notification_id */,
          51 /* render_process_id */));
}

TEST_F(NotificationIdGeneratorTest, GenerateForNonPersistent_IsDeterministic) {
  EXPECT_EQ(generator_.GenerateForNonPersistentMojoNotification(
                origin_, "example-token"),
            generator_.GenerateForNonPersistentMojoNotification(
                origin_, "example-token"));
}

TEST_F(NotificationIdGeneratorTest, GenerateForNonPersistent_DifferentOrigins) {
  url::Origin different_origin(
      url::Origin::Create(GURL("https://example2.com")));

  EXPECT_NE(generator_.GenerateForNonPersistentMojoNotification(
                origin_, "example-token"),
            generator_.GenerateForNonPersistentMojoNotification(
                different_origin, "example-token"));
}

TEST_F(NotificationIdGeneratorTest, GenerateForNonPersistent_DifferentTokens) {
  EXPECT_NE(generator_.GenerateForNonPersistentMojoNotification(
                origin_, "example-token"),
            generator_.GenerateForNonPersistentMojoNotification(origin_,
                                                                "other-token"));
}

// Use port numbers and a token which, when concatenated, could end up being
// equal to each other if origins stop ending with slashes.
TEST_F(NotificationIdGeneratorTest,
       GenerateForNonPersistent_OriginPortAmbiguity) {
  auto origin_805(url::Origin::Create(GURL("https://example.com:805")));
  auto origin_8051(url::Origin::Create(GURL("https://example.com:8051")));

  EXPECT_NE(
      generator_.GenerateForNonPersistentMojoNotification(origin_805, "17"),
      generator_.GenerateForNonPersistentMojoNotification(origin_8051, "7"));
}

}  // namespace
}  // namespace content
