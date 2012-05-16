// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/test/test_switches.h"
#include "base/test/trace_event_analyzer.h"
#include "base/threading/platform_thread.h"
#include "base/timer.h"
#include "base/version.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/tracing.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/perf/browser_perf_test.h"
#include "chrome/test/perf/perf_test.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/test/gpu/gpu_test_config.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

// Run with --vmodule=latency_tests=1 to print verbose latency info.

// How is latency measured?
//
// The test injects mouse moves many times per frame from the browser via
// RenderWidgetHost. Each input has a unique x coordinate. When the javascript
// handler receives the input, it stores the coordinate for later use in the
// requestAnimationFrame callback. In RAF, the test paints using the x
// coordinate as a color (in software, it sets the color of a table; in webgl,
// it executes a glClearColor). Trace events emit the color when it is picked up
// by either UpdateRect for software or gles2_cmd_decoder/glClear for webgl.
//
// Each UpdateRect (software) or SwapBuffers (webgl) is considered to be a frame
// boundary that will be used to measure latency in number of frames. Starting
// from a frame boundary Y, the test first determines what mouse x coordinate
// was represented by the color at that frame boundary. Then, the test walks
// backward through the trace events to find the input event matching that
// x coordinate. Then, the test find the nearest frame boundary X to the input
// event (may be before or after). The number of frame boundaries is then
// counted between X and Y to determine the input latency.
//
// By injecting mouse moves many times per frame, we reduce flakiness in the
// finding of the nearest frame boundary.
//
// This test only measures the latency introduced by chrome code -- it does not
// measure latency introduced by mouse drivers or the GL driver or the OS window
// manager. The actual latency seen by a user is more than what is reported by
// this test.
//
// Current modes:
// - Software RAF
// - WebGL RAF

namespace {

using trace_analyzer::CountMatches;
using trace_analyzer::FindClosest;
using trace_analyzer::FindLastOf;
using trace_analyzer::RateStats;
using trace_analyzer::Query;
using trace_analyzer::TraceAnalyzer;
using trace_analyzer::TraceEvent;
using trace_analyzer::TraceEventVector;

enum LatencyTestMode {
  kWebGL,
  kSoftware
};

enum LatencyTestFlags {
  kInputHeavy   = 1 << 0,
  kInputDirty   = 1 << 1,
  kRafHeavy     = 1 << 2,
  kPaintHeavy   = 1 << 3
};

const int kWebGLCanvasWidth = 10;
const int kNumFrames = 80;
const int kInputsPerFrame = 16;
// Magic number to identify certain glClear events.
const int kClearColorGreen = 137;
const int kMouseY = 5;

// Don't analyze begin frames that may be inaccurate. Latencies can be as high
// as 5 frames or so, so skip the first 6 frames to get more accurate results.
const int kIgnoreBeginFrames = 6;
// Don't analyze end frames that may be inaccurate.
const int kIgnoreEndFrames = 4;
// Minimum frames to produce an answer.
const int kMinimumFramesForAnalysis = 5;

class LatencyTest
    : public BrowserPerfTest,
      public ::testing::WithParamInterface<int> {
 public:
  LatencyTest() :
      query_instant_(Query::EventPhase() ==
                     Query::Phase(TRACE_EVENT_PHASE_INSTANT)),
      // These queries are initialized in RunTest.
      query_swaps_(Query::Bool(false)),
      query_inputs_(Query::Bool(false)),
      query_blits_(Query::Bool(false)),
      query_clears_(Query::Bool(false)),
      mouse_x_(0),
      tab_width_(0),
      mode_(kWebGL),
      delay_time_us_(0),
      num_frames_(0),
      verbose_(false),
      test_flags_(0) {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE;

  std::vector<int> GetAllBehaviors();

  // Run test with specified |mode| and |behaviors|.
  // |mode| can be webgl or software.
  // |behaviors| is a list of combinations of LatencyTestFlags.
  void RunTest(LatencyTestMode mode, const std::vector<int>& behaviors);

 private:
  void RunTestInternal(const std::string& test_url,
                       bool send_inputs,
                       int input_delay_us);

  double CalculateLatency();

  std::string GetModeString() {
    switch (mode_) {
      case kWebGL:
        return "webgl";
      case kSoftware:
        return "software";
      default:
        NOTREACHED() << "invalid mode";
        return "";
    }
  }

  std::string GetTraceName(int flags);

  std::string GetUrlModeString(int flags);

  std::string GetUrl(int flags);

  void GetMeanFrameTimeMicros(int* frame_time) const;

  void SendInput();

  void PrintEvents(const TraceEventVector& events);

  // Path to html file.
  FilePath test_path_;

  // Query INSTANT events.
  Query query_instant_;

  // Query "swaps" which is SwapBuffers for GL and UpdateRect for software.
  Query query_swaps_;

  // Query mouse input entry events in browser process (ForwardMouseEvent).
  Query query_inputs_;

  // Query GL blits for the WebGL canvas -- represents the compositor consuming
  // the WebGL contents for display.
  Query query_blits_;

  // Query glClear calls with mouse coordinate as clear color.
  Query query_clears_;

  // For searching trace data.
  scoped_ptr<TraceAnalyzer> analyzer_;

  // Current mouse x coordinate for injecting events.
  int mouse_x_;

  // Width of window containing our tab.
  int tab_width_;

  // Timer for injecting mouse events periodically.
  base::RepeatingTimer<LatencyTest> timer_;

  // Mode: webgl or software.
  LatencyTestMode mode_;

  // Delay time for javascript test code. Typically 2 x frame duration. Used
  // to spin-wait in the javascript input handler and requestAnimationFrame.
  int delay_time_us_;

  // Number of frames to render from the html test code.
  int num_frames_;

  // Map from test flags combination to the calculated mean latency.
  std::map<int, double> latencies_;

  // Whether to print more verbose output.
  bool verbose_;

  // Current test flags combination, determining the behavior of the test.
  int test_flags_;
};

void LatencyTest::SetUpCommandLine(CommandLine* command_line) {
  BrowserPerfTest::SetUpCommandLine(command_line);
  // This enables DOM automation for tab contents.
  EnableDOMAutomation();
  if (CommandLine::ForCurrentProcess()->
      HasSwitch(switches::kEnableThreadedCompositing)) {
    command_line->AppendSwitch(switches::kEnableThreadedCompositing);
  }
  // Default behavior is to thumbnail the tab after 0.5 seconds, causing
  // a nasty frame hitch and disturbing the latency test. Fix that:
  command_line->AppendSwitch(switches::kEnableInBrowserThumbnailing);
  command_line->AppendSwitch(switches::kDisableBackgroundNetworking);
}

std::vector<int> LatencyTest::GetAllBehaviors() {
  std::vector<int> behaviors;
  int max_behaviors = kInputHeavy | kInputDirty | kRafHeavy | kPaintHeavy;
  for (int i = 0; i <= max_behaviors; ++i)
    behaviors.push_back(i);
  return behaviors;
}

void LatencyTest::RunTest(LatencyTestMode mode,
                          const std::vector<int>& behaviors) {
  mode_ = mode;
  verbose_ = (logging::GetVlogLevel("latency_tests") > 0);

  // Linux Intel uses mesa driver, where multisampling is not supported.
  // Multisampling is also not supported on virtualized mac os.
  // The latency test uses the multisampling blit trace event to determine when
  // the compositor is consuming the webgl context, so it currently doesn't work
  // without multisampling. Since the Latency test does not depend much on the
  // GPU, let's just skip testing on Intel since the data is redundant with
  // other non-Intel bots.
  GPUTestBotConfig test_bot;
  test_bot.LoadCurrentConfig(NULL);
  const std::vector<uint32>& gpu_vendor = test_bot.gpu_vendor();
#if defined(OS_LINUX)
  if (gpu_vendor.size() == 1 && gpu_vendor[0] == 0x8086)
    return;
#endif  // defined(OS_LINUX)
#if defined(OS_MACOSX)
  if (gpu_vendor.size() == 1 && gpu_vendor[0] == 0x15AD)
    return;
#endif  // defined(OS_MACOSX)

#if defined(OS_WIN)
  // Latency test doesn't work on WinXP. crbug.com/128066
  if (base::win::OSInfo::GetInstance()->version() == base::win::VERSION_XP)
    return;
#endif

  // Construct queries for searching trace events via TraceAnalyzer.
  if (mode_ == kWebGL) {
    query_swaps_ = query_instant_ &&
        Query::EventName() == Query::String("SwapBuffers") &&
        Query::EventArg("width") != Query::Int(kWebGLCanvasWidth);
  } else if (mode_ == kSoftware) {
    // Software updates need to have x=0 and y=0 to contain the input color.
    query_swaps_ = query_instant_ &&
        Query::EventName() == Query::String("UpdateRect") &&
        Query::EventArg("x+y") == Query::Int(0);
  }
  query_inputs_ = query_instant_ &&
      Query::EventName() == Query::String("MouseEventBegin");
  query_blits_ = query_instant_ &&
      Query::EventName() == Query::String("DoBlit") &&
      Query::EventArg("width") == Query::Int(kWebGLCanvasWidth);
  query_clears_ = query_instant_ &&
      Query::EventName() == Query::String("DoClear") &&
      Query::EventArg("green") == Query::Int(kClearColorGreen);
  Query query_width_swaps = query_swaps_;
  if (mode_ == kSoftware) {
    query_width_swaps = query_instant_ &&
        Query::EventName() == Query::String("UpdateRectWidth") &&
        Query::EventArg("width") > Query::Int(kWebGLCanvasWidth);
  }

  // Set path to test html.
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_path_));
  test_path_ = test_path_.Append(FILE_PATH_LITERAL("perf"));
  test_path_ = test_path_.Append(FILE_PATH_LITERAL("latency_suite.html"));
  ASSERT_TRUE(file_util::PathExists(test_path_))
      << "Missing test file: " << test_path_.value();

  // Run once with defaults to measure the frame times.
  delay_time_us_ = 0;
  // kNumFrames may be very high, but we only need a few frames to measure
  // average frame times.
  num_frames_ = 30;
  int initial_flags = 0;
  if (mode_ == kSoftware) {
    // For the first run, run software with kPaintHeavy (which toggles the
    // background color every frame) to force an update each RAF. Otherwise it
    // won't trigger an UpdateRect each frame and we won't be able to measure
    // framerate, because there are no inputs during the first run.
    initial_flags = static_cast<int>(kPaintHeavy);
  }
  RunTestInternal(GetUrl(initial_flags), false, 0);

  // Get width of tab so that we know the limit of x coordinates for the
  // injected mouse inputs.
  const TraceEvent* swap_event = analyzer_->FindOneEvent(query_width_swaps);
  ASSERT_TRUE(swap_event);
  tab_width_ = swap_event->GetKnownArgAsInt("width");
  // Keep printf output clean by limiting input coords to three digits:
  tab_width_ = (tab_width_ < 1000) ? tab_width_ : 999;
  // Sanity check the tab_width -- it should be more than 100 pixels.
  EXPECT_GT(tab_width_, 100);

  int mean_frame_time_us = 0;
  GetMeanFrameTimeMicros(&mean_frame_time_us);
  if (verbose_)
    printf("Mean frame time micros = %d\n", mean_frame_time_us);
  // Delay time is 2x the average frame time.
  delay_time_us_ = 2 * mean_frame_time_us;
  // Calculate delay time between inputs based on the measured frame time.
  // This prevents flooding the browser with more events than we need if the
  // test is running very slow (such as on a VM).
  int delay_us = mean_frame_time_us / kInputsPerFrame;

  // Reset num_frames_ for actual test runs.
  num_frames_ = kNumFrames;

  // Run input latency test with each requested behavior.
  for (size_t i = 0; i < behaviors.size(); ++i) {
    test_flags_ = behaviors[i];
    std::string url = GetUrl(test_flags_);
    printf("=============================================================\n");
    if (verbose_)
      printf("Mode: %s\n", GetUrlModeString(i).c_str());
    printf("URL: %s\n", url.c_str());

    // Do the actual test with input events.
    RunTestInternal(url, true, delay_us);
    latencies_[test_flags_] = CalculateLatency();
  }

  // Print summary if more than 1 behavior was tested in this run. This is only
  // for manual test runs for human reabable results, not for perf bots.
  if (behaviors.size() > 1) {
    printf("#############################################################\n");
    printf("## %s\n", GetModeString().c_str());
    if (verbose_) {
      printf("Latency, behavior:\n");
      for (size_t i = 0; i < behaviors.size(); ++i) {
        printf("%.1f, %s%s%s%s\n", latencies_[behaviors[i]],
               (i & kInputHeavy) ? "InputHeavy " : "",
               (i & kInputDirty) ? "InputDirty " : "",
               (i & kRafHeavy) ? "RafHeavy " : "",
               (i & kPaintHeavy) ? "PaintHeavy " : "");
      }
    }
    printf("Latencies for tests: ");
    for (size_t i = 0; i < behaviors.size(); ++i) {
      printf("%.1f%s", latencies_[behaviors[i]],
             (i < behaviors.size() - 1) ? ", " : "");
    }
    printf("\n");
    printf("#############################################################\n");
  }
}

void LatencyTest::RunTestInternal(const std::string& test_url,
                                  bool send_inputs,
                                  int input_delay_us) {
  mouse_x_ = 0;

  ASSERT_TRUE(tracing::BeginTracing("test_gpu,test_latency"));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(test_url), CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);

  // Start sending mouse inputs.
  if (send_inputs) {
    // Round input_delay_us down to nearest milliseconds. The rounding in timer
    // code rounds up from us to ms, so we need to do our own rounding here.
    int input_delay_ms = input_delay_us / 1000;
    input_delay_ms = (input_delay_ms <= 0) ? 1 : input_delay_ms;
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(input_delay_ms),
                 this, &LatencyTest::SendInput);
  }

  // Wait for message indicating the test has finished running.
  ui_test_utils::DOMMessageQueue message_queue;
  ASSERT_TRUE(message_queue.WaitForMessage(NULL));

  timer_.Stop();

  std::string json_events;
  ASSERT_TRUE(tracing::EndTracing(&json_events));

  analyzer_.reset(TraceAnalyzer::Create(json_events));
  analyzer_->AssociateBeginEndEvents();
  analyzer_->MergeAssociatedEventArgs();
}

double LatencyTest::CalculateLatency() {
  TraceEventVector events;
  if (mode_ == kWebGL) {
    // Search for three types of events in WebGL mode:
    //  - onscreen swaps.
    //  - DoClear calls that contain the mouse x coordinate.
    //  - mouse events.
    analyzer_->FindEvents(query_swaps_ || query_inputs_ ||
                          query_blits_ || query_clears_, &events);
  } else if (mode_ == kSoftware) {
    analyzer_->FindEvents(query_swaps_ || query_inputs_, &events);
  } else {
    NOTREACHED() << "invalid mode";
  }

  if (verbose_)
    PrintEvents(events);

  int swap_count = 0;
  size_t previous_blit_pos = 0;
  swap_count = 0;
  std::vector<int> latencies;
  printf("Measured latency (in number of frames) for each frame:\n");
  for (size_t i = 0; i < events.size(); ++i) {
    if (query_swaps_.Evaluate(*events[i])) {
      // Compositor context swap buffers.
      ++swap_count;
      // Don't analyze first few swaps, because they are filling the rendering
      // pipeline and may be unstable.
      if (swap_count > kIgnoreBeginFrames) {
        int mouse_x = 0;
        if (mode_ == kWebGL) {
          // Trace backwards through the events to find the input event that
          // matches the glClear that was presented by this SwapBuffers.

          // Step 1: Find the last blit (which will be the WebGL blit).
          size_t blit_pos = 0;
          EXPECT_TRUE(FindLastOf(events, query_blits_, i, &blit_pos));
          // Skip this SwapBuffers if the blit has already been consumed by a
          // previous SwapBuffers. This means the current frame did not receive
          // an update from WebGL.
          EXPECT_GT(blit_pos, previous_blit_pos);
          if (blit_pos == previous_blit_pos) {
            if (verbose_)
              printf(" %03d: ERROR\n", swap_count);
            else
              printf(" ERROR");
            continue;
          }
          previous_blit_pos = blit_pos;

          // Step 2: find the last clear from the WebGL blit. This will be the
          // value of the latest mouse input that has affected this swap.
          size_t clear_pos = 0;
          EXPECT_TRUE(FindLastOf(events, query_clears_, blit_pos, &clear_pos));
          mouse_x = events[clear_pos]->GetKnownArgAsInt("red");
        } else if (mode_ == kSoftware) {
          // The software path gets the mouse_x directly from the DIB colors.
          mouse_x = events[i]->GetKnownArgAsInt("color");
        }

        // Find the corresponding mouse input.
        size_t input_pos = 0;
        Query query_mouse_event = query_inputs_ &&
            Query::EventArg("x") == Query::Int(mouse_x);
        EXPECT_TRUE(FindLastOf(events, query_mouse_event, i, &input_pos));

        // Step 4: Find the nearest onscreen SwapBuffers to this input event.
        size_t closest_swap = 0;
        size_t second_closest_swap = 0;
        EXPECT_TRUE(FindClosest(events, query_swaps_, input_pos,
                                       &closest_swap, &second_closest_swap));
        int latency = CountMatches(events, query_swaps_, closest_swap, i);
        latencies.push_back(latency);
        if (verbose_)
          printf(" %03d: %d\n", swap_count, latency);
        else
          printf(" %d", latency);
      }
    }
  }
  printf("\n");

  size_t ignoreEndFrames = static_cast<size_t>(kIgnoreEndFrames);
  bool haveEnoughFrames = latencies.size() >
      ignoreEndFrames + static_cast<size_t>(kMinimumFramesForAnalysis);
  EXPECT_TRUE(haveEnoughFrames);
  if (!haveEnoughFrames)
    return 0.0;

  double mean_latency = 0.0;
  // Skip last few frames, because they may be unreliable.
  size_t num_consider = latencies.size() - ignoreEndFrames;
  for (size_t i = 0; i < num_consider; ++i)
    mean_latency += static_cast<double>(latencies[i]);
  mean_latency /= static_cast<double>(num_consider);
  printf("Mean latency = %f\n", mean_latency);

  double mean_error = 0.0;
  for (size_t i = 0; i < num_consider; ++i) {
    double offset = fabs(mean_latency - static_cast<double>(latencies[i]));
    mean_error = (offset > mean_error) ? offset : mean_error;
  }

  std::string trace_name = GetTraceName(test_flags_);
  std::string mean_and_error = base::StringPrintf("%f,%f", mean_latency,
                                                  mean_error);
  perf_test::PrintResultMeanAndError(GetModeString(), "", trace_name,
                                     mean_and_error, "frames", true);
  return mean_latency;
}

std::string LatencyTest::GetTraceName(int flags) {
  if (flags == 0)
    return "simple";
  std::string name;
  if (flags & kInputHeavy)
    name += "ih";
  if (flags & kInputDirty)
    name += std::string(name.empty()? "" : "_") + "id";
  if (flags & kRafHeavy)
    name += std::string(name.empty()? "" : "_") + "rh";
  if (flags & kPaintHeavy)
    name += std::string(name.empty()? "" : "_") + "ph";
  return name;
}

std::string LatencyTest::GetUrlModeString(int flags) {
  std::string mode = "&mode=" + GetModeString();
  if (flags & kInputHeavy)
    mode += "&inputHeavy";
  if (flags & kInputDirty)
    mode += "&inputDirty";
  if (flags & kRafHeavy)
    mode += "&rafHeavy";
  if (flags & kPaintHeavy)
    mode += "&paintHeavy";
  return mode;
}

std::string LatencyTest::GetUrl(int flags) {
  std::string test_url =
      net::FilePathToFileURL(test_path_).possibly_invalid_spec();
  test_url += "?numFrames=" + base::IntToString(num_frames_);
  test_url += "&canvasWidth=" + base::IntToString(kWebGLCanvasWidth);
  test_url += "&clearColorGreen=" + base::IntToString(kClearColorGreen);
  test_url += "&delayTimeMS=" + base::IntToString(delay_time_us_ / 1000);
  test_url += "&y=" + base::IntToString(kMouseY);
  return test_url + GetUrlModeString(flags);
}

void LatencyTest::GetMeanFrameTimeMicros(int* frame_time) const {
  TraceEventVector events;
  // Search for compositor swaps (or UpdateRects in the software path).
  analyzer_->FindEvents(query_swaps_, &events);
  RateStats stats;
  ASSERT_TRUE(GetRateStats(events, &stats, NULL));

  // Check that the number of swaps is close to kNumFrames.
  EXPECT_LT(num_frames_ - num_frames_ / 4, static_cast<int>(events.size()));
  *frame_time = static_cast<int>(stats.mean_us);
}

void LatencyTest::SendInput() {
  content::RenderViewHost* rvh = browser()->GetSelectedTabContentsWrapper()->
      web_contents()->GetRenderViewHost();
  WebKit::WebMouseEvent mouse_event;
  mouse_event.movementX = 1;
  mouse_x_ += mouse_event.movementX;
  // Wrap mouse_x_ when it's near the edge of the tab.
  if (mouse_x_ > tab_width_ - 5)
    mouse_x_ = 1;
  mouse_event.x = mouse_event.windowX = mouse_x_;
  // Set y coordinate to be a few pixels down from the top of the window,
  // so that it is between the top and bottom of the canvas.
  mouse_event.y = mouse_event.windowY = 5;
  mouse_event.type = WebKit::WebInputEvent::MouseMove;
  TRACE_EVENT_INSTANT1("test_latency", "MouseEventBegin", "x", mouse_x_);
  rvh->ForwardMouseEvent(mouse_event);
}

void LatencyTest::PrintEvents(const TraceEventVector& events) {
  bool is_software = (mode_ == kSoftware);
  int swap_count = 0;
  for (size_t i = 0; i < events.size(); ++i) {
    if (events[i]->name == "MouseEventBegin") {
      printf("%03d ", events[i]->GetKnownArgAsInt("x"));
    } else if (events[i]->name == "DoClear") {
      printf("Clr%03d ", events[i]->GetKnownArgAsInt("red"));
    } else if (events[i]->name == "DoBlit") {
      // WebGL context swap buffers.
      printf("BLT ");
    } else if (events[i]->name == "SwapBuffers") {
      // Compositor context swap buffers.
      ++swap_count;
      printf("|\nframe %03d: ", swap_count + 1);
    } else if (is_software && events[i]->name == "UpdateRect") {
      ++swap_count;
      printf("(%d)|\nframe %03d: ",
              events[i]->GetKnownArgAsInt("color"), swap_count + 1);
    }
  }
  printf("\n");
}

////////////////////////////////////////////////////////////////////////////////
/// Tests

using ::testing::Values;

// For manual testing only, run all input latency tests and print summary.
IN_PROC_BROWSER_TEST_F(LatencyTest, DISABLED_LatencyWebGLAll) {
  RunTest(kWebGL, GetAllBehaviors());
}

// For manual testing only, run all input latency tests and print summary.
IN_PROC_BROWSER_TEST_F(LatencyTest, DISABLED_LatencySoftwareAll) {
  RunTest(kSoftware, GetAllBehaviors());
}

IN_PROC_BROWSER_TEST_P(LatencyTest, LatencySoftware) {
  RunTest(kSoftware, std::vector<int>(1, GetParam()));
}

IN_PROC_BROWSER_TEST_P(LatencyTest, LatencyWebGL) {
  RunTest(kWebGL, std::vector<int>(1, GetParam()));
}

INSTANTIATE_TEST_CASE_P(, LatencyTest, ::testing::Values(
    0,
    kInputHeavy,
    kInputHeavy | kInputDirty | kRafHeavy,
    kInputHeavy | kInputDirty | kRafHeavy | kPaintHeavy,
    kInputDirty | kPaintHeavy,
    kInputDirty | kRafHeavy | kPaintHeavy
    ));

}  // namespace
