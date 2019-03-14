// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_event_matcher.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_graphics_model.h"
#include "chrome/browser/chromeos/arc/tracing/arc_tracing_model.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace arc {

namespace {

using GraphicsEvents = ArcTracingGraphicsModel::BufferEvents;
using GraphicsEventType = ArcTracingGraphicsModel::BufferEventType;

constexpr char kAcquireBufferQuery[] =
    "android:onMessageReceived/android:handleMessageInvalidate/"
    "android:latchBuffer/android:updateTexImage/android:acquireBuffer";

constexpr char kAttachSurfaceQueury[] =
    "toplevel:OnLibevent/exo:Surface::Attach";

constexpr char kTestEvent[] =
    R"({"pid":4640,"tid":4641,"id":"1234","ts":14241877057,"ph":"X","cat":"exo",
        "name":"Surface::Attach",
        "args":{"buffer_id":"0x7f9f5110690","app_id":"org.chromium.arc.6"},
        "dur":10,"tdur":9,"tts":1216670360})";

constexpr char kAppId[] = "app_id";
constexpr char kAppIdValue[] = "org.chromium.arc.6";
constexpr char kBufferId[] = "buffer_id";
constexpr char kBufferIdBad[] = "_buffer_id";
constexpr char kBufferIdValue[] = "0x7f9f5110690";
constexpr char kBufferIdValueBad[] = "_0x7f9f5110690";
constexpr char kDefault[] = "default";
constexpr char kExo[] = "exo";
constexpr char kExoBad[] = "_exo";
constexpr char kPhaseX = 'X';
constexpr char kPhaseP = 'P';
constexpr char kSurfaceAttach[] = "Surface::Attach";
constexpr char kSurfaceAttachBad[] = "_Surface::Attach";

// Validates that events have increasing timestamp, have only allowed types and
// each type is found at least once.
bool ValidateGrahpicsEvent(const GraphicsEvents& events,
                           const std::set<GraphicsEventType>& allowed_types) {
  if (events.empty())
    return false;
  int64_t previous_timestamp = 0;
  std::set<GraphicsEventType> used_types;
  for (const auto& event : events) {
    if (event.timestamp < previous_timestamp) {
      LOG(ERROR) << "Timestamp sequence broken: " << event.timestamp << " vs "
                 << previous_timestamp << ".";
      return false;
    }
    previous_timestamp = event.timestamp;
    if (!allowed_types.count(event.type)) {
      LOG(ERROR) << "Unexpected event type " << event.type << ".";
      return false;
    }
    used_types.insert(event.type);
  }
  if (used_types.size() != allowed_types.size()) {
    for (const auto& allowed_type : allowed_types) {
      if (!used_types.count(allowed_type))
        LOG(ERROR) << "Required event type " << allowed_type
                   << " << is not found.";
    }
    return false;
  }
  return true;
}

bool TestGraphicsModelLoad(const std::string& name) {
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  const base::FilePath tracing_path =
      base_path.Append("arc_graphics_tracing").Append(name);
  std::string json_data;
  base::ReadFileToString(tracing_path, &json_data);
  DCHECK(!json_data.empty());
  ArcTracingGraphicsModel model;
  return model.LoadFromJson(json_data);
}

}  // namespace

class ArcTracingModelTest : public testing::Test {
 public:
  ArcTracingModelTest() = default;
  ~ArcTracingModelTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcTracingModelTest);
};

// TopLevel test is performed on the data collected from the real test device.
// It tests that model can be built from the provided data and queries work.
TEST_F(ArcTracingModelTest, TopLevel) {
  base::FilePath base_path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &base_path);
  const base::FilePath tracing_path =
      base_path.Append("arc_graphics_tracing").Append("trace.dat.gz");

  std::string tracing_data_compressed;
  ASSERT_TRUE(base::ReadFileToString(tracing_path, &tracing_data_compressed));

  std::string tracing_data;
  ASSERT_TRUE(
      compression::GzipUncompress(tracing_data_compressed, &tracing_data));

  ArcTracingModel model;
  ASSERT_TRUE(model.Build(tracing_data));

  // Perform several well-known queries.
  EXPECT_FALSE(model.Select(kAcquireBufferQuery).empty());
  EXPECT_FALSE(model.Select(kAttachSurfaceQueury).empty());

  std::stringstream ss;
  model.Dump(ss);
  EXPECT_FALSE(ss.str().empty());

  // Continue in this test to avoid heavy calculations for building base model.
  // Make sure we can createe graphics model.
  ArcTracingGraphicsModel graphics_model;
  ASSERT_TRUE(graphics_model.Build(model));

  EXPECT_TRUE(ValidateGrahpicsEvent(
      graphics_model.android_top_level(),
      {GraphicsEventType::kVsync,
       GraphicsEventType::kSurfaceFlingerInvalidationStart,
       GraphicsEventType::kSurfaceFlingerInvalidationDone,
       GraphicsEventType::kSurfaceFlingerCompositionStart,
       GraphicsEventType::kSurfaceFlingerCompositionDone}));
  EXPECT_TRUE(
      ValidateGrahpicsEvent(graphics_model.chrome_top_level(),
                            {
                                GraphicsEventType::kChromeOSDraw,
                                GraphicsEventType::kChromeOSSwap,
                                GraphicsEventType::kChromeOSWaitForAck,
                                GraphicsEventType::kChromeOSPresentationDone,
                                GraphicsEventType::kChromeOSSwapDone,
                            }));
  EXPECT_FALSE(graphics_model.view_buffers().empty());
  for (const auto& view : graphics_model.view_buffers()) {
    // At least one buffer.
    EXPECT_GT(view.first.task_id, 0);
    EXPECT_NE(std::string(), view.first.activity);
    EXPECT_FALSE(view.second.empty());
    for (const auto& buffer : view.second) {
      EXPECT_TRUE(ValidateGrahpicsEvent(
          buffer, {
                      GraphicsEventType::kBufferQueueDequeueStart,
                      GraphicsEventType::kBufferQueueDequeueDone,
                      GraphicsEventType::kBufferQueueQueueStart,
                      GraphicsEventType::kBufferQueueQueueDone,
                      GraphicsEventType::kBufferQueueAcquire,
                      GraphicsEventType::kBufferQueueReleased,
                      GraphicsEventType::kExoSurfaceAttach,
                      GraphicsEventType::kExoProduceResource,
                      GraphicsEventType::kExoBound,
                      GraphicsEventType::kExoPendingQuery,
                      GraphicsEventType::kExoReleased,
                      GraphicsEventType::kChromeBarrierOrder,
                      GraphicsEventType::kChromeBarrierFlush,
                  }));
    }
  }
  EXPECT_GT(graphics_model.duration(), 0U);

  // Serialize and restore;
  const std::string graphics_model_data = graphics_model.SerializeToJson();
  EXPECT_FALSE(graphics_model_data.empty());

  ArcTracingGraphicsModel graphics_model_loaded;
  EXPECT_TRUE(graphics_model_loaded.LoadFromJson(graphics_model_data));

  // Models should match.
  EXPECT_EQ(graphics_model.android_top_level(),
            graphics_model_loaded.android_top_level());
  EXPECT_EQ(graphics_model.chrome_top_level(),
            graphics_model_loaded.chrome_top_level());
  EXPECT_EQ(graphics_model.view_buffers(),
            graphics_model_loaded.view_buffers());
  EXPECT_EQ(graphics_model.duration(), graphics_model_loaded.duration());
}

TEST_F(ArcTracingModelTest, Event) {
  const ArcTracingEvent event(base::JSONReader::ReadDeprecated(kTestEvent));

  EXPECT_EQ(4640, event.GetPid());
  EXPECT_EQ(4641, event.GetTid());
  EXPECT_EQ("1234", event.GetId());
  EXPECT_EQ(kExo, event.GetCategory());
  EXPECT_EQ(kSurfaceAttach, event.GetName());
  EXPECT_EQ(kPhaseX, event.GetPhase());
  EXPECT_EQ(14241877057L, event.GetTimestamp());
  EXPECT_EQ(10, event.GetDuration());
  EXPECT_EQ(14241877067L, event.GetEndTimestamp());
  EXPECT_NE(nullptr, event.GetDictionary());
  EXPECT_EQ(kBufferIdValue, event.GetArgAsString(kBufferId, std::string()));
  EXPECT_EQ(kDefault, event.GetArgAsString(kBufferIdBad, kDefault));
}

TEST_F(ArcTracingModelTest, EventClassification) {
  const ArcTracingEvent event(base::JSONReader::ReadDeprecated(kTestEvent));

  ArcTracingEvent event_before(base::JSONReader::ReadDeprecated(kTestEvent));
  event_before.SetTimestamp(event.GetTimestamp() - event.GetDuration());
  EXPECT_EQ(ArcTracingEvent::Position::kBefore,
            event.ClassifyPositionOf(event_before));

  ArcTracingEvent event_after(base::JSONReader::ReadDeprecated(kTestEvent));
  event_after.SetTimestamp(event.GetTimestamp() + event.GetDuration());
  EXPECT_EQ(ArcTracingEvent::Position::kAfter,
            event.ClassifyPositionOf(event_after));

  ArcTracingEvent event_inside(base::JSONReader::ReadDeprecated(kTestEvent));
  event_inside.SetTimestamp(event.GetTimestamp() + 1);
  event_inside.SetDuration(event.GetDuration() - 2);
  EXPECT_EQ(ArcTracingEvent::Position::kInside,
            event.ClassifyPositionOf(event_inside));
  EXPECT_EQ(ArcTracingEvent::Position::kInside,
            event.ClassifyPositionOf(event));

  ArcTracingEvent event_overlap(base::JSONReader::ReadDeprecated(kTestEvent));
  event_overlap.SetTimestamp(event.GetTimestamp() + 1);
  EXPECT_EQ(ArcTracingEvent::Position::kOverlap,
            event.ClassifyPositionOf(event_overlap));
  event_overlap.SetTimestamp(event.GetTimestamp());
  event_overlap.SetDuration(event.GetDuration() + 1);
  EXPECT_EQ(ArcTracingEvent::Position::kOverlap,
            event.ClassifyPositionOf(event_overlap));
  event_overlap.SetTimestamp(event.GetTimestamp() - 1);
  event_overlap.SetDuration(event.GetDuration() + 2);
  EXPECT_EQ(ArcTracingEvent::Position::kOverlap,
            event.ClassifyPositionOf(event_overlap));
}

TEST_F(ArcTracingModelTest, EventAppendChild) {
  ArcTracingEvent event(base::JSONReader::ReadDeprecated(kTestEvent));

  // Impossible to append the even that is bigger than target.
  std::unique_ptr<ArcTracingEvent> event_overlap =
      std::make_unique<ArcTracingEvent>(
          base::JSONReader::ReadDeprecated(kTestEvent));
  event_overlap->SetTimestamp(event.GetTimestamp() + 1);
  EXPECT_FALSE(event.AppendChild(std::move(event_overlap)));

  std::unique_ptr<ArcTracingEvent> event1 = std::make_unique<ArcTracingEvent>(
      base::JSONReader::ReadDeprecated(kTestEvent));
  event1->SetTimestamp(event.GetTimestamp() + 4);
  event1->SetDuration(2);
  EXPECT_TRUE(event.AppendChild(std::move(event1)));
  EXPECT_EQ(1U, event.children().size());

  // Impossible to append the event that is before last child.
  std::unique_ptr<ArcTracingEvent> event2 = std::make_unique<ArcTracingEvent>(
      base::JSONReader::ReadDeprecated(kTestEvent));
  event2->SetTimestamp(event.GetTimestamp());
  event2->SetDuration(2);
  EXPECT_FALSE(event.AppendChild(std::move(event2)));
  EXPECT_EQ(1U, event.children().size());

  // Append child to child
  std::unique_ptr<ArcTracingEvent> event3 = std::make_unique<ArcTracingEvent>(
      base::JSONReader::ReadDeprecated(kTestEvent));
  event3->SetTimestamp(event.GetTimestamp() + 5);
  event3->SetDuration(1);
  EXPECT_TRUE(event.AppendChild(std::move(event3)));
  ASSERT_EQ(1U, event.children().size());
  EXPECT_EQ(1U, event.children()[0].get()->children().size());

  // Append next immediate child.
  std::unique_ptr<ArcTracingEvent> event4 = std::make_unique<ArcTracingEvent>(
      base::JSONReader::ReadDeprecated(kTestEvent));
  event4->SetTimestamp(event.GetTimestamp() + 6);
  event4->SetDuration(2);
  EXPECT_TRUE(event.AppendChild(std::move(event4)));
  EXPECT_EQ(2U, event.children().size());
}

TEST_F(ArcTracingModelTest, EventMatcher) {
  const ArcTracingEvent event(base::JSONReader::ReadDeprecated(kTestEvent));
  // Nothing is specified. It matches any event.
  EXPECT_TRUE(ArcTracingEventMatcher().Match(event));

  // Phase
  EXPECT_TRUE(ArcTracingEventMatcher().SetPhase(kPhaseX).Match(event));
  EXPECT_FALSE(ArcTracingEventMatcher().SetPhase(kPhaseP).Match(event));

  // Category
  EXPECT_TRUE(ArcTracingEventMatcher().SetCategory(kExo).Match(event));
  EXPECT_FALSE(ArcTracingEventMatcher().SetCategory(kExoBad).Match(event));

  // Name
  EXPECT_TRUE(ArcTracingEventMatcher().SetName(kSurfaceAttach).Match(event));
  EXPECT_FALSE(
      ArcTracingEventMatcher().SetName(kSurfaceAttachBad).Match(event));

  // Arguments
  EXPECT_TRUE(ArcTracingEventMatcher()
                  .AddArgument(kBufferId, kBufferIdValue)
                  .Match(event));
  EXPECT_TRUE(ArcTracingEventMatcher()
                  .AddArgument(kBufferId, kBufferIdValue)
                  .AddArgument(kAppId, kAppIdValue)
                  .Match(event));
  EXPECT_FALSE(ArcTracingEventMatcher()
                   .AddArgument(kBufferIdBad, kBufferIdValue)
                   .Match(event));
  EXPECT_FALSE(ArcTracingEventMatcher()
                   .AddArgument(kBufferId, kBufferIdValueBad)
                   .Match(event));

  // String query
  EXPECT_TRUE(
      ArcTracingEventMatcher("exo:Surface::Attach(buffer_id=0x7f9f5110690)")
          .Match(event));
  EXPECT_FALSE(
      ArcTracingEventMatcher("_exo:_Surface::Attach(buffer_id=_0x7f9f5110690)")
          .Match(event));
}

TEST_F(ArcTracingModelTest, GraphicsModelLoad) {
  EXPECT_TRUE(TestGraphicsModelLoad("gm_good.json"));
  EXPECT_FALSE(TestGraphicsModelLoad("gm_bad_no_view_buffers.json"));
  EXPECT_FALSE(TestGraphicsModelLoad("gm_bad_no_view_desc.json"));
  EXPECT_FALSE(TestGraphicsModelLoad("gm_bad_wrong_timestamp.json"));
}

}  // namespace arc
