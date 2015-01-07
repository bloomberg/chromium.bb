// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>

#include "gpu/command_buffer/service/gles2_cmd_decoder_mock.h"
#include "gpu/command_buffer/service/gpu_service_test.h"
#include "gpu/command_buffer/service/gpu_tracer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_context_stub.h"
#include "ui/gl/gl_mock.h"
#include "ui/gl/gl_surface_stub.h"

namespace gpu {
namespace gles2 {

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Exactly;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;

class FakeCPUTime : public CPUTime {
 public:
  FakeCPUTime()
      : current_cpu_time_(0) {
  }

  int64 GetCurrentTime() override {
    return current_cpu_time_;
  }

  void SetFakeCPUTime(int64 cpu_time) {
    current_cpu_time_ = cpu_time;
  }

 protected:
  ~FakeCPUTime() override {}

  int64 current_cpu_time_;
};

class MockOutputter : public Outputter {
 public:
  MockOutputter() {}
  MOCK_METHOD4(TraceDevice,
               void(const std::string& category, const std::string& name,
                    int64 start_time, int64 end_time));

  MOCK_METHOD3(TraceServiceBegin,
               void(const std::string& category, const std::string& name,
                    void* id));

  MOCK_METHOD3(TraceServiceEnd,
               void(const std::string& category, const std::string& name,
                    void* id));

 protected:
  ~MockOutputter() {}
};

class GlFakeQueries {
 public:
  GlFakeQueries() {}

  void Reset() {
    current_time_ = 0;
    next_query_id_ = 23;
    alloced_queries_.clear();
    query_timestamp_.clear();
  }

  void SetCurrentGLTime(GLint64 current_time) { current_time_ = current_time; }
  void SetDisjoint() { disjointed_ = true; }

  void GenQueriesARB(GLsizei n, GLuint* ids) {
    for (GLsizei i = 0; i < n; i++) {
      ids[i] = next_query_id_++;
      alloced_queries_.insert(ids[i]);
    }
  }

  void DeleteQueriesARB(GLsizei n, const GLuint* ids) {
    for (GLsizei i = 0; i < n; i++) {
      alloced_queries_.erase(ids[i]);
      query_timestamp_.erase(ids[i]);
    }
  }

  void GetQueryObjectiv(GLuint id, GLenum pname, GLint* params) {
    switch (pname) {
      case GL_QUERY_RESULT_AVAILABLE: {
        std::map<GLuint, GLint64>::iterator it = query_timestamp_.find(id);
        if (it != query_timestamp_.end() && it->second <= current_time_)
          *params = 1;
        else
          *params = 0;
        break;
      }
      default:
        FAIL() << "Invalid variable passed to GetQueryObjectiv: " << pname;
    }
  }

  void QueryCounter(GLuint id, GLenum target) {
    switch (target) {
      case GL_TIMESTAMP:
        ASSERT_TRUE(alloced_queries_.find(id) != alloced_queries_.end());
        query_timestamp_[id] = current_time_;
        break;
      default:
        FAIL() << "Invalid variable passed to QueryCounter: " << target;
    }
  }

  void GetInteger64v(GLenum pname, GLint64 * data) {
    switch (pname) {
      case GL_TIMESTAMP:
        *data = current_time_;
        break;
      default:
        FAIL() << "Invalid variable passed to GetInteger64v: " << pname;
    }
  }

  void GetQueryObjectui64v(GLuint id, GLenum pname, GLuint64* params) {
    switch (pname) {
      case GL_QUERY_RESULT:
        ASSERT_TRUE(query_timestamp_.find(id) != query_timestamp_.end());
        *params = query_timestamp_.find(id)->second;
        break;
      default:
        FAIL() << "Invalid variable passed to GetQueryObjectui64v: " << pname;
    }
  }

  void GetIntegerv(GLenum pname, GLint* params) {
    switch (pname) {
      case GL_GPU_DISJOINT_EXT:
        *params = static_cast<GLint>(disjointed_);
        disjointed_ = false;
        break;
      default:
        FAIL() << "Invalid variable passed to GetIntegerv: " << pname;
    }
  }

  void Finish() {
  }

  GLenum GetError() {
    return GL_NO_ERROR;
  }

 protected:
  bool disjointed_;
  GLint64 current_time_;
  GLuint next_query_id_;
  std::set<GLuint> alloced_queries_;
  std::map<GLuint, GLint64> query_timestamp_;
};

class GPUTracerTester : public GPUTracer {
 public:
  GPUTracerTester(GpuTracerType tracer_type, gles2::GLES2Decoder* decoder)
      : GPUTracer(decoder),
        tracing_enabled_(0),
        test_tracer_type_(tracer_type) {
    // Force tracing to be dependent on our mock variable here.
    gpu_trace_srv_category = &tracing_enabled_;
    gpu_trace_dev_category = &tracing_enabled_;
  }

  ~GPUTracerTester() override {}

  void SetTracingEnabled(bool enabled) {
    tracing_enabled_ = enabled ? 1 : 0;
  }

  void SetOutputter(scoped_refptr<Outputter> outputter) {
    set_outputter_ = outputter;
  }

  void SetCPUTime(scoped_refptr<CPUTime> cputime) {
    set_cputime_ = cputime;
  }

 protected:
  scoped_refptr<Outputter> CreateOutputter(const std::string& name) override {
    if (set_outputter_.get()) {
      return set_outputter_;
    }
    return new MockOutputter();
  }

  scoped_refptr<CPUTime> CreateCPUTime() override {
    if (set_cputime_.get()) {
      return set_cputime_;
    }
    return new FakeCPUTime();
  }

  GpuTracerType DetermineTracerType() override {
    return test_tracer_type_;
  }

  void PostTask() override {
    // Process synchronously.
    Process();
  }

  unsigned char tracing_enabled_;
  GpuTracerType test_tracer_type_;

  scoped_refptr<Outputter> set_outputter_;
  scoped_refptr<CPUTime> set_cputime_;
};

class BaseGpuTest : public GpuServiceTest {
 public:
  BaseGpuTest(GpuTracerType test_tracer_type)
      : test_tracer_type_(test_tracer_type) {
  }

 protected:
  void SetUp() override {
    GpuServiceTest::SetUp();
    gl_fake_queries_.Reset();
    gl_surface_ = new gfx::GLSurfaceStub();
    gl_context_ = new gfx::GLContextStub();
    gl_context_->MakeCurrent(gl_surface_.get());

    outputter_ref_ = new MockOutputter();
    cpu_time_ref_ = new FakeCPUTime;
  }

  void TearDown() override {
    outputter_ref_ = NULL;
    cpu_time_ref_ = NULL;

    gl_context_->ReleaseCurrent(gl_surface_.get());
    gl_context_ = NULL;
    gl_surface_ = NULL;

    gl_.reset();
    gl_fake_queries_.Reset();
    GpuServiceTest::TearDown();
  }

  void ExpectTraceQueryMocks() {
    if (GetTracerType() != kTracerTypeInvalid) {
      // Delegate query APIs used by GPUTrace to a GlFakeQueries
      EXPECT_CALL(*gl_, GenQueriesARB(2, NotNull())).Times(AtLeast(1))
          .WillRepeatedly(
              Invoke(&gl_fake_queries_, &GlFakeQueries::GenQueriesARB));

      EXPECT_CALL(*gl_, GetQueryObjectiv(_, GL_QUERY_RESULT_AVAILABLE,
                                         NotNull()))
          .WillRepeatedly(
               Invoke(&gl_fake_queries_, &GlFakeQueries::GetQueryObjectiv));

      if (GetTracerType() == kTracerTypeDisjointTimer) {
        EXPECT_CALL(*gl_, GetInteger64v(GL_TIMESTAMP, _))
          .WillRepeatedly(
               Invoke(&gl_fake_queries_, &GlFakeQueries::GetInteger64v));
      }

      EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP)).Times(AtLeast(2))
          .WillRepeatedly(
               Invoke(&gl_fake_queries_, &GlFakeQueries::QueryCounter));

      EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
          .WillRepeatedly(
               Invoke(&gl_fake_queries_,
                      &GlFakeQueries::GetQueryObjectui64v));

      EXPECT_CALL(*gl_, DeleteQueriesARB(2, NotNull())).Times(AtLeast(1))
          .WillRepeatedly(
               Invoke(&gl_fake_queries_, &GlFakeQueries::DeleteQueriesARB));
    }
  }

  void ExpectOutputterBeginMocks(MockOutputter* outputter,
                                 const std::string& category,
                                 const std::string& name) {
    EXPECT_CALL(*outputter,
                TraceServiceBegin(category, name, NotNull()));
  }

  void ExpectOutputterEndMocks(MockOutputter* outputter,
                               const std::string& category,
                               const std::string& name, int64 expect_start_time,
                               int64 expect_end_time,
                               bool trace_device) {
    EXPECT_CALL(*outputter,
                TraceServiceEnd(category, name, NotNull()));

    if (trace_device) {
      EXPECT_CALL(*outputter,
                  TraceDevice(category, name,
                              expect_start_time, expect_end_time))
          .Times(Exactly(1));
    } else {
      EXPECT_CALL(*outputter, TraceDevice(category, name,
                                          expect_start_time, expect_end_time))
          .Times(Exactly(0));
    }
  }

  void ExpectOutputterMocks(MockOutputter* outputter,
                            const std::string& category,
                            const std::string& name, int64 expect_start_time,
                            int64 expect_end_time) {
    ExpectOutputterBeginMocks(outputter, category, name);
    ExpectOutputterEndMocks(outputter, category, name,
                            expect_start_time, expect_end_time,
                            GetTracerType() != kTracerTypeInvalid);
  }

  void ExpectTracerOffsetQueryMocks() {
    // Disjoint check should only be called by kTracerTypeDisjointTimer type.
    if (GetTracerType() == kTracerTypeDisjointTimer) {
      EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, _)).Times(AtLeast(1))
          .WillRepeatedly(
              Invoke(&gl_fake_queries_, &GlFakeQueries::GetIntegerv));
    } else {
      EXPECT_CALL(*gl_, GetIntegerv(GL_GPU_DISJOINT_EXT, _)).Times(Exactly(0));
    }

    // Timer offset calculation should only happen for the regular timer.
    if (GetTracerType() != kTracerTypeARBTimer) {
      EXPECT_CALL(*gl_, GenQueriesARB(_, NotNull())).Times(Exactly(0));
      EXPECT_CALL(*gl_, Finish()).Times(Exactly(0));
      EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP)).Times(Exactly(0));
      EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
          .Times(Exactly(0));
      EXPECT_CALL(*gl_, DeleteQueriesARB(_, NotNull())).Times(Exactly(0));
    } else {
      EXPECT_CALL(*gl_, GenQueriesARB(_, NotNull())).Times(AtLeast(1))
          .WillRepeatedly(
              Invoke(&gl_fake_queries_, &GlFakeQueries::GenQueriesARB));

      EXPECT_CALL(*gl_, Finish()).Times(AtLeast(2))
          .WillRepeatedly(
              Invoke(&gl_fake_queries_, &GlFakeQueries::Finish));

      EXPECT_CALL(*gl_, QueryCounter(_, GL_TIMESTAMP))
          .Times(AtLeast(1))
          .WillRepeatedly(
               Invoke(&gl_fake_queries_, &GlFakeQueries::QueryCounter));

      EXPECT_CALL(*gl_, GetQueryObjectui64v(_, GL_QUERY_RESULT, NotNull()))
          .Times(AtLeast(1))
          .WillRepeatedly(
               Invoke(&gl_fake_queries_,
                      &GlFakeQueries::GetQueryObjectui64v));

      EXPECT_CALL(*gl_, DeleteQueriesARB(1, NotNull()))
          .Times(AtLeast(1))
          .WillRepeatedly(
               Invoke(&gl_fake_queries_, &GlFakeQueries::DeleteQueriesARB));
    }
  }

  GpuTracerType GetTracerType() { return test_tracer_type_; }

  GpuTracerType test_tracer_type_;
  GlFakeQueries gl_fake_queries_;

  scoped_refptr<MockOutputter> outputter_ref_;
  scoped_refptr<FakeCPUTime> cpu_time_ref_;

  scoped_refptr<gfx::GLSurface> gl_surface_;
  scoped_refptr<gfx::GLContext> gl_context_;
};

// Test GPUTrace calls all the correct gl calls.
class BaseGpuTraceTest : public BaseGpuTest {
 public:
  BaseGpuTraceTest(GpuTracerType test_tracer_type)
      : BaseGpuTest(test_tracer_type) {
  }

  void DoTraceTest() {
    // Expected results
    const std::string category_name("trace_category");
    const std::string trace_name("trace_test");
    const int64 offset_time = 3231;
    const GLint64 start_timestamp = 7 * base::Time::kNanosecondsPerMicrosecond;
    const GLint64 end_timestamp = 32 * base::Time::kNanosecondsPerMicrosecond;
    const int64 expect_start_time =
        (start_timestamp / base::Time::kNanosecondsPerMicrosecond) +
        offset_time;
    const int64 expect_end_time =
        (end_timestamp / base::Time::kNanosecondsPerMicrosecond) + offset_time;

    ExpectTraceQueryMocks();
    ExpectOutputterMocks(outputter_ref_.get(), category_name, trace_name,
                         expect_start_time, expect_end_time);

    scoped_refptr<GPUTrace> trace =
        new GPUTrace(outputter_ref_, cpu_time_ref_, category_name, trace_name,
                     offset_time, GetTracerType());

    gl_fake_queries_.SetCurrentGLTime(start_timestamp);
    trace->Start(true);

    // Shouldn't be available before End() call
    gl_fake_queries_.SetCurrentGLTime(end_timestamp);
    EXPECT_FALSE(trace->IsAvailable());

    trace->End(true);

    // Shouldn't be available until the queries complete
    gl_fake_queries_.SetCurrentGLTime(end_timestamp -
                                      base::Time::kNanosecondsPerMicrosecond);
    EXPECT_FALSE(trace->IsAvailable());

    // Now it should be available
    gl_fake_queries_.SetCurrentGLTime(end_timestamp);
    EXPECT_TRUE(trace->IsAvailable());

    // Proces should output expected Trace results to MockOutputter
    trace->Process();

    outputter_ref_ = NULL;
    cpu_time_ref_ = NULL;
  }
};

class GpuARBTimerTraceTest : public BaseGpuTraceTest {
 public:
  GpuARBTimerTraceTest()
      : BaseGpuTraceTest(kTracerTypeARBTimer) {
  }
};

class GpuDisjointTimerTraceTest : public BaseGpuTraceTest {
 public:
  GpuDisjointTimerTraceTest()
      : BaseGpuTraceTest(kTracerTypeDisjointTimer) {
  }
};

TEST_F(GpuARBTimerTraceTest, ARBTimerTraceTest) {
  DoTraceTest();
}

TEST_F(GpuDisjointTimerTraceTest, DisjointTimerTraceTest) {
  DoTraceTest();
}

// Test GPUTracer calls all the correct gl calls.
class BaseGpuTracerTest : public BaseGpuTest {
 public:
  BaseGpuTracerTest(GpuTracerType test_tracer_type)
      : BaseGpuTest(test_tracer_type) {
  }

  void DoBasicTracerTest() {
    ExpectTracerOffsetQueryMocks();

    MockGLES2Decoder decoder;
    GPUTracerTester tracer(test_tracer_type_, &decoder);
    tracer.SetTracingEnabled(true);

    tracer.SetOutputter(outputter_ref_);
    tracer.SetCPUTime(cpu_time_ref_);

    ASSERT_TRUE(tracer.BeginDecoding());
    ASSERT_TRUE(tracer.EndDecoding());

    outputter_ref_ = NULL;
    cpu_time_ref_ = NULL;
  }

  void DoTracerMarkersTest() {
    ExpectTracerOffsetQueryMocks();

    EXPECT_CALL(*gl_, GetError()).Times(AtLeast(0))
          .WillRepeatedly(
               Invoke(&gl_fake_queries_, &GlFakeQueries::GetError));

    const std::string category_name("trace_category");
    const std::string trace_name("trace_test");
    const int64 offset_time = 3231;
    const GLint64 start_timestamp = 7 * base::Time::kNanosecondsPerMicrosecond;
    const GLint64 end_timestamp = 32 * base::Time::kNanosecondsPerMicrosecond;
    const int64 expect_start_time =
        (start_timestamp / base::Time::kNanosecondsPerMicrosecond) +
        offset_time;
    const int64 expect_end_time =
        (end_timestamp / base::Time::kNanosecondsPerMicrosecond) + offset_time;

    MockGLES2Decoder decoder;
    GPUTracerTester tracer(test_tracer_type_, &decoder);
    tracer.SetTracingEnabled(true);

    tracer.SetOutputter(outputter_ref_);
    tracer.SetCPUTime(cpu_time_ref_);

    gl_fake_queries_.SetCurrentGLTime(start_timestamp);
    cpu_time_ref_->SetFakeCPUTime(expect_start_time);

    ASSERT_TRUE(tracer.BeginDecoding());

    ExpectTraceQueryMocks();

    // This will test multiple marker sources which overlap one another.
    for (int i = 0; i < NUM_TRACER_SOURCES; ++i) {
      // Set times so each source has a different time.
      gl_fake_queries_.SetCurrentGLTime(
          start_timestamp +
          (i * base::Time::kNanosecondsPerMicrosecond));
      cpu_time_ref_->SetFakeCPUTime(expect_start_time + i);

      // Each trace name should be different to differentiate.
      const char num_char = static_cast<char>('0' + i);
      std::string source_category = category_name + num_char;
      std::string source_trace_name = trace_name + num_char;

      ExpectOutputterBeginMocks(outputter_ref_.get(),
                                source_category, source_trace_name);

      const GpuTracerSource source = static_cast<GpuTracerSource>(i);
      ASSERT_TRUE(tracer.Begin(source_category, source_trace_name, source));

      ASSERT_EQ(source_category, tracer.CurrentCategory());
      ASSERT_EQ(source_trace_name, tracer.CurrentName());
    }

    for (int i = 0; i < NUM_TRACER_SOURCES; ++i) {
      // Set times so each source has a different time.
      gl_fake_queries_.SetCurrentGLTime(
          end_timestamp +
          (i * base::Time::kNanosecondsPerMicrosecond));
      cpu_time_ref_->SetFakeCPUTime(expect_end_time + i);

      // Each trace name should be different to differentiate.
      const char num_char = static_cast<char>('0' + i);
      std::string source_category = category_name + num_char;
      std::string source_trace_name = trace_name + num_char;

      ExpectOutputterEndMocks(outputter_ref_.get(), source_category,
                              source_trace_name,
                              expect_start_time + i, expect_end_time + i,
                              GetTracerType() != kTracerTypeInvalid);

      const GpuTracerSource source = static_cast<GpuTracerSource>(i);
      ASSERT_TRUE(tracer.End(source));
    }

    ASSERT_TRUE(tracer.EndDecoding());

    outputter_ref_ = NULL;
    cpu_time_ref_ = NULL;
  }

  void DoDisjointTest() {
    // Cause a disjoint in a middle of a trace and expect no output calls.
    ExpectTracerOffsetQueryMocks();

    EXPECT_CALL(*gl_, GetError()).Times(AtLeast(0))
          .WillRepeatedly(
               Invoke(&gl_fake_queries_, &GlFakeQueries::GetError));

    const std::string category_name("trace_category");
    const std::string trace_name("trace_test");
    const GpuTracerSource source = static_cast<GpuTracerSource>(0);
    const int64 offset_time = 3231;
    const GLint64 start_timestamp = 7 * base::Time::kNanosecondsPerMicrosecond;
    const GLint64 end_timestamp = 32 * base::Time::kNanosecondsPerMicrosecond;
    const int64 expect_start_time =
        (start_timestamp / base::Time::kNanosecondsPerMicrosecond) +
        offset_time;
    const int64 expect_end_time =
        (end_timestamp / base::Time::kNanosecondsPerMicrosecond) + offset_time;

    MockGLES2Decoder decoder;
    GPUTracerTester tracer(test_tracer_type_, &decoder);
    tracer.SetTracingEnabled(true);

    tracer.SetOutputter(outputter_ref_);
    tracer.SetCPUTime(cpu_time_ref_);

    gl_fake_queries_.SetCurrentGLTime(start_timestamp);
    cpu_time_ref_->SetFakeCPUTime(expect_start_time);

    ASSERT_TRUE(tracer.BeginDecoding());

    ExpectTraceQueryMocks();

    ExpectOutputterBeginMocks(outputter_ref_.get(),
                              category_name, trace_name);
    ASSERT_TRUE(tracer.Begin(category_name, trace_name, source));

    gl_fake_queries_.SetCurrentGLTime(end_timestamp);
    cpu_time_ref_->SetFakeCPUTime(expect_end_time);
    gl_fake_queries_.SetDisjoint();

    ExpectOutputterEndMocks(outputter_ref_.get(), category_name, trace_name,
                            expect_start_time, expect_end_time, false);

    ASSERT_TRUE(tracer.End(source));
    ASSERT_TRUE(tracer.EndDecoding());

    outputter_ref_ = NULL;
    cpu_time_ref_ = NULL;
  }
};

class InvalidTimerTracerTest : public BaseGpuTracerTest {
 public:
  InvalidTimerTracerTest()
      : BaseGpuTracerTest(kTracerTypeInvalid) {
  }
};

class GpuARBTimerTracerTest : public BaseGpuTracerTest {
 public:
  GpuARBTimerTracerTest()
      : BaseGpuTracerTest(kTracerTypeARBTimer) {
  }
};

class GpuDisjointTimerTracerTest : public BaseGpuTracerTest {
 public:
  GpuDisjointTimerTracerTest()
      : BaseGpuTracerTest(kTracerTypeDisjointTimer) {
  }
};

TEST_F(InvalidTimerTracerTest, InvalidTimerBasicTracerTest) {
  DoBasicTracerTest();
}

TEST_F(GpuARBTimerTracerTest, ARBTimerBasicTracerTest) {
  DoBasicTracerTest();
}

TEST_F(GpuDisjointTimerTracerTest, DisjointTimerBasicTracerTest) {
  DoBasicTracerTest();
}

TEST_F(InvalidTimerTracerTest, InvalidTimerTracerMarkersTest) {
  DoTracerMarkersTest();
}

TEST_F(GpuARBTimerTracerTest, ARBTimerBasicTracerMarkersTest) {
  DoTracerMarkersTest();
}

TEST_F(GpuDisjointTimerTracerTest, DisjointTimerBasicTracerMarkersTest) {
  DoTracerMarkersTest();
}

TEST_F(GpuDisjointTimerTracerTest, DisjointTimerDisjointTraceTest) {
  DoDisjointTest();
}

// Test basic functionality of the GPUTracerTester.
TEST(GPUTracerTester, IsTracingTest) {
  MockGLES2Decoder decoder;
  GPUTracerTester tracer_tester(kTracerTypeInvalid, &decoder);
  EXPECT_FALSE(tracer_tester.IsTracing());
  tracer_tester.SetTracingEnabled(true);
  EXPECT_TRUE(tracer_tester.IsTracing());
}

TEST(GPUTracerTester, DecodeTest) {
  MockGLES2Decoder decoder;
  GPUTracerTester tracer_tester(kTracerTypeInvalid, &decoder);
  ASSERT_TRUE(tracer_tester.BeginDecoding());
  EXPECT_FALSE(tracer_tester.BeginDecoding());
  ASSERT_TRUE(tracer_tester.EndDecoding());
  EXPECT_FALSE(tracer_tester.EndDecoding());
}

TEST(GPUTracerTester, TraceDuringDecodeTest) {
  MockGLES2Decoder decoder;
  GPUTracerTester tracer_tester(kTracerTypeInvalid, &decoder);
  const std::string category_name("trace_category");
  const std::string trace_name("trace_test");

  EXPECT_FALSE(tracer_tester.Begin(category_name, trace_name,
                                   kTraceGroupMarker));

  ASSERT_TRUE(tracer_tester.BeginDecoding());
  EXPECT_TRUE(tracer_tester.Begin(category_name, trace_name,
                                  kTraceGroupMarker));
  ASSERT_TRUE(tracer_tester.EndDecoding());
}

}  // namespace gles2
}  // namespace gpu
