// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/output_configurator.h"

#include <cmath>
#include <cstdarg>
#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

// Strings returned by TestDelegate::GetActionsAndClear() to describe various
// actions that were performed.
const char kInitXRandR[] = "init";
const char kUpdateXRandR[] = "update";
const char kGrab[] = "grab";
const char kUngrab[] = "ungrab";
const char kSync[] = "sync";
const char kForceDPMS[] = "dpms";
const char kProjectingOn[] = "projecting";
const char kProjectingOff[] = "not_projecting";

// String returned by TestDelegate::GetActionsAndClear() if no actions were
// requested.
const char kNoActions[] = "";

// Returns a string describing a TestDelegate::SetBackgroundColor() call.
std::string GetBackgroundAction(uint32 color_argb) {
  return base::StringPrintf("background(0x%x)", color_argb);
}

// Returns a string describing a TestDelegate::AddOutputMode() call.
std::string GetAddOutputModeAction(RROutput output, RRMode mode) {
  return base::StringPrintf("add_mode(output=%lu,mode=%lu)", output, mode);
}

// Returns a string describing a TestDelegate::ConfigureCrtc() call.
std::string GetCrtcAction(RRCrtc crtc,
                          int x,
                          int y,
                          RRMode mode,
                          RROutput output) {
  return base::StringPrintf("crtc(crtc=%lu,x=%d,y=%d,mode=%lu,output=%lu)",
                            crtc, x, y, mode, output);
}

// Returns a string describing a TestDelegate::CreateFramebuffer() call.
std::string GetFramebufferAction(int width,
                                 int height,
                                 RRCrtc crtc1,
                                 RRCrtc crtc2) {
  return base::StringPrintf(
      "framebuffer(width=%d,height=%d,crtc1=%lu,crtc2=%lu)",
      width, height, crtc1, crtc2);
}

// Returns a string describing a TestDelegate::ConfigureCTM() call.
std::string GetCTMAction(
    int device_id,
    const OutputConfigurator::CoordinateTransformation& ctm) {
  return base::StringPrintf("ctm(id=%d,transform=(%f,%f,%f,%f))", device_id,
      ctm.x_scale, ctm.x_offset, ctm.y_scale, ctm.y_offset);
}

// Returns a string describing a TestDelegate::SetHDCPState() call.
std::string GetSetHDCPStateAction(RROutput id, HDCPState state) {
  return base::StringPrintf("set_hdcp(id=%lu,state=%d)", id, state);
}

// Joins a sequence of strings describing actions (e.g. kScreenDim) such
// that they can be compared against a string returned by
// TestDelegate::GetActionsAndClear().  The list of actions must be
// terminated by a NULL pointer.
std::string JoinActions(const char* action, ...) {
  std::string actions;

  va_list arg_list;
  va_start(arg_list, action);
  while (action) {
    if (!actions.empty())
      actions += ",";
    actions += action;
    action = va_arg(arg_list, const char*);
  }
  va_end(arg_list);
  return actions;
}

class TestDelegate : public OutputConfigurator::Delegate {
 public:
  static const int kXRandREventBase = 10;

  TestDelegate()
      : configure_crtc_result_(true),
        hdcp_state_(HDCP_STATE_UNDESIRED) {}
  virtual ~TestDelegate() {}

  const std::vector<OutputConfigurator::OutputSnapshot>& outputs() const {
    return outputs_;
  }
  void set_outputs(
      const std::vector<OutputConfigurator::OutputSnapshot>& outputs) {
    outputs_ = outputs;
  }

  void set_configure_crtc_result(bool result) {
    configure_crtc_result_ = result;
  }

  void set_hdcp_state(HDCPState state) { hdcp_state_ = state; }

  // Returns a comma-separated string describing the actions that were
  // requested since the previous call to GetActionsAndClear() (i.e.
  // results are non-repeatable).
  std::string GetActionsAndClear() {
    std::string actions = actions_;
    actions_.clear();
    return actions;
  }

  const OutputConfigurator::CoordinateTransformation& get_ctm(
      int touch_device_id) {
    return ctms_[touch_device_id];
  }

  // OutputConfigurator::Delegate overrides:
  virtual void InitXRandRExtension(int* event_base) OVERRIDE {
    AppendAction(kInitXRandR);
    *event_base = kXRandREventBase;
  }
  virtual void UpdateXRandRConfiguration(
      const base::NativeEvent& event) OVERRIDE { AppendAction(kUpdateXRandR); }
  virtual void GrabServer() OVERRIDE { AppendAction(kGrab); }
  virtual void UngrabServer() OVERRIDE { AppendAction(kUngrab); }
  virtual void SyncWithServer() OVERRIDE { AppendAction(kSync); }
  virtual void SetBackgroundColor(uint32 color_argb) OVERRIDE {
    AppendAction(GetBackgroundAction(color_argb));
  }
  virtual void ForceDPMSOn() OVERRIDE { AppendAction(kForceDPMS); }
  virtual std::vector<OutputConfigurator::OutputSnapshot> GetOutputs()
      OVERRIDE {
    return outputs_;
  }
  virtual void AddOutputMode(RROutput output, RRMode mode) OVERRIDE {
    AppendAction(GetAddOutputModeAction(output, mode));
  }
  virtual bool ConfigureCrtc(RRCrtc crtc,
                             RRMode mode,
                             RROutput output,
                             int x,
                             int y) OVERRIDE {
    AppendAction(GetCrtcAction(crtc, x, y, mode, output));
    return configure_crtc_result_;
  }
  virtual void CreateFrameBuffer(
      int width,
      int height,
      const std::vector<OutputConfigurator::OutputSnapshot>& outputs) OVERRIDE {
    AppendAction(
        GetFramebufferAction(width,
                             height,
                             outputs.size() >= 1 ? outputs[0].crtc : 0,
                             outputs.size() >= 2 ? outputs[1].crtc : 0));
  }
  virtual void ConfigureCTM(
      int touch_device_id,
      const OutputConfigurator::CoordinateTransformation& ctm) OVERRIDE {
    AppendAction(GetCTMAction(touch_device_id, ctm));
    ctms_[touch_device_id] = ctm;
  }
  virtual void SendProjectingStateToPowerManager(bool projecting) OVERRIDE {
    AppendAction(projecting ? kProjectingOn : kProjectingOff);
  }

  virtual bool GetHDCPState(RROutput id, HDCPState* state) OVERRIDE {
    *state = hdcp_state_;
    return true;
  }

  virtual bool SetHDCPState(RROutput id, HDCPState state) OVERRIDE {
    AppendAction(GetSetHDCPStateAction(id, state));
    return true;
  }

 private:
  struct ModeDetails {
    ModeDetails() : width(0), height(0), interlaced(false) {}
    ModeDetails(int width, int height, bool interlaced)
        : width(width),
          height(height),
          interlaced(interlaced) {}

    int width;
    int height;
    bool interlaced;
  };

  void AppendAction(const std::string& action) {
    if (!actions_.empty())
      actions_ += ",";
    actions_ += action;
  }

  std::map<RRMode, ModeDetails> modes_;

  // Most-recently-configured transformation matrices, keyed by touch device ID.
  std::map<int, OutputConfigurator::CoordinateTransformation> ctms_;

  // Outputs to be returned by GetOutputs().
  std::vector<OutputConfigurator::OutputSnapshot> outputs_;

  std::string actions_;

  // Return value returned by ConfigureCrtc().
  bool configure_crtc_result_;

  // Result value of GetHDCPState().
  HDCPState hdcp_state_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class TestObserver : public OutputConfigurator::Observer {
 public:
  explicit TestObserver(OutputConfigurator* configurator)
      : configurator_(configurator) {
    Reset();
    configurator_->AddObserver(this);
  }
  virtual ~TestObserver() {
    configurator_->RemoveObserver(this);
  }

  int num_changes() const { return num_changes_; }
  int num_failures() const { return num_failures_; }
  const std::vector<OutputConfigurator::OutputSnapshot>& latest_outputs()
      const {
    return latest_outputs_;
  }
  OutputState latest_failed_state() const { return latest_failed_state_; }

  void Reset() {
    num_changes_ = 0;
    num_failures_ = 0;
    latest_outputs_.clear();
    latest_failed_state_ = STATE_INVALID;
  }

  // OutputConfigurator::Observer overrides:
  virtual void OnDisplayModeChanged(
      const std::vector<OutputConfigurator::OutputSnapshot>& outputs) OVERRIDE {
    num_changes_++;
    latest_outputs_ = outputs;
  }

  virtual void OnDisplayModeChangeFailed(OutputState failed_new_state)
      OVERRIDE {
    num_failures_++;
    latest_failed_state_ = failed_new_state;
  }

 private:
  OutputConfigurator* configurator_;  // Not owned.

  // Number of times that OnDisplayMode*() has been called.
  int num_changes_;
  int num_failures_;

  // Parameters most recently passed to OnDisplayMode*().
  std::vector<OutputConfigurator::OutputSnapshot> latest_outputs_;
  OutputState latest_failed_state_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestStateController : public OutputConfigurator::StateController {
 public:
  TestStateController() : state_(STATE_DUAL_EXTENDED) {}
  virtual ~TestStateController() {}

  void set_state(OutputState state) { state_ = state; }

  // OutputConfigurator::StateController overrides:
  virtual OutputState GetStateForDisplayIds(
      const std::vector<int64>& outputs) const OVERRIDE { return state_; }
  virtual bool GetResolutionForDisplayId(
      int64 display_id,
      int *width,
      int *height) const OVERRIDE {
    return false;
  }

 private:
  OutputState state_;

  DISALLOW_COPY_AND_ASSIGN(TestStateController);
};

class TestMirroringController
    : public OutputConfigurator::SoftwareMirroringController {
 public:
  TestMirroringController() : software_mirroring_enabled_(false) {}
  virtual ~TestMirroringController() {}

  virtual void SetSoftwareMirroring(bool enabled) OVERRIDE {
    software_mirroring_enabled_ = enabled;
  }

  bool software_mirroring_enabled() const {
    return software_mirroring_enabled_;
  }

 private:
  bool software_mirroring_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestMirroringController);
};

class OutputConfiguratorTest : public testing::Test {
 public:
  // Predefined modes that can be used by outputs.
  static const RRMode kSmallModeId;
  static const int kSmallModeWidth;
  static const int kSmallModeHeight;

  static const RRMode kBigModeId;
  static const int kBigModeWidth;
  static const int kBigModeHeight;

  OutputConfiguratorTest()
      : observer_(&configurator_),
        test_api_(&configurator_, TestDelegate::kXRandREventBase) {}
  virtual ~OutputConfiguratorTest() {}

  virtual void SetUp() OVERRIDE {
    delegate_ = new TestDelegate();
    configurator_.SetDelegateForTesting(
        scoped_ptr<OutputConfigurator::Delegate>(delegate_));
    configurator_.set_state_controller(&state_controller_);
    configurator_.set_mirroring_controller(&mirroring_controller_);

    OutputConfigurator::ModeInfo small_mode_info;
    small_mode_info.width = kSmallModeWidth;
    small_mode_info.height = kSmallModeHeight;

    OutputConfigurator::ModeInfo big_mode_info;
    big_mode_info.width = kBigModeWidth;
    big_mode_info.height = kBigModeHeight;

    OutputConfigurator::OutputSnapshot* o = &outputs_[0];
    o->output = 1;
    o->crtc = 10;
    o->current_mode = kSmallModeId;
    o->native_mode = kSmallModeId;
    o->is_internal = true;
    o->type = OUTPUT_TYPE_INTERNAL;
    o->is_aspect_preserving_scaling = true;
    o->mode_infos[kSmallModeId] = small_mode_info;
    o->has_display_id = true;
    o->display_id = 123;
    o->index = 0;

    o = &outputs_[1];
    o->output = 2;
    o->crtc = 11;
    o->current_mode = kBigModeId;
    o->native_mode = kBigModeId;
    o->is_internal = false;
    o->type = OUTPUT_TYPE_HDMI;
    o->is_aspect_preserving_scaling = true;
    o->mode_infos[kSmallModeId] = small_mode_info;
    o->mode_infos[kBigModeId] = big_mode_info;
    o->has_display_id = true;
    o->display_id = 456;
    o->index = 1;

    UpdateOutputs(2, false);
  }

 protected:
  // Configures |delegate_| to return the first |num_outputs| entries from
  // |outputs_|. If |send_events| is true, also sends screen-change and
  // output-change events to |configurator_| and triggers the configure
  // timeout if one was scheduled.
  void UpdateOutputs(size_t num_outputs, bool send_events) {
    ASSERT_LE(num_outputs, arraysize(outputs_));
    std::vector<OutputConfigurator::OutputSnapshot> outputs;
    for (size_t i = 0; i < num_outputs; ++i)
      outputs.push_back(outputs_[i]);
    delegate_->set_outputs(outputs);

    if (send_events) {
      test_api_.SendScreenChangeEvent();
      for (size_t i = 0; i < arraysize(outputs_); ++i) {
        const OutputConfigurator::OutputSnapshot output = outputs_[i];
        bool connected = i < num_outputs;
        test_api_.SendOutputChangeEvent(
            output.output, output.crtc, output.current_mode, connected);
      }
      test_api_.TriggerConfigureTimeout();
    }
  }

  // Initializes |configurator_| with a single internal display.
  void InitWithSingleOutput() {
    UpdateOutputs(1, false);
    EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
    configurator_.Init(false);
    EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
    configurator_.Start(0);
    EXPECT_EQ(JoinActions(kGrab, kInitXRandR,
                          GetFramebufferAction(kSmallModeWidth,
                              kSmallModeHeight, outputs_[0].crtc, 0).c_str(),
                          GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                                        outputs_[0].output).c_str(),
                          kForceDPMS, kUngrab, kProjectingOff, NULL),
              delegate_->GetActionsAndClear());
  }

  base::MessageLoop message_loop_;
  TestStateController state_controller_;
  TestMirroringController mirroring_controller_;
  OutputConfigurator configurator_;
  TestObserver observer_;
  TestDelegate* delegate_;  // not owned
  OutputConfigurator::TestApi test_api_;

  OutputConfigurator::OutputSnapshot outputs_[2];

 private:
  DISALLOW_COPY_AND_ASSIGN(OutputConfiguratorTest);
};

const RRMode OutputConfiguratorTest::kSmallModeId = 20;
const int OutputConfiguratorTest::kSmallModeWidth = 1366;
const int OutputConfiguratorTest::kSmallModeHeight = 768;

const RRMode OutputConfiguratorTest::kBigModeId = 21;
const int OutputConfiguratorTest::kBigModeWidth = 2560;
const int OutputConfiguratorTest::kBigModeHeight = 1600;

}  // namespace

TEST_F(OutputConfiguratorTest, FindOutputModeMatchingSize) {
  OutputConfigurator::OutputSnapshot output;

  // Fields are width, height, interlaced, refresh rate.
  output.mode_infos[11] = OutputConfigurator::ModeInfo(1920, 1200, false, 60.0);
  // Different rates.
  output.mode_infos[12] = OutputConfigurator::ModeInfo(1920, 1080, false, 30.0);
  output.mode_infos[13] = OutputConfigurator::ModeInfo(1920, 1080, false, 50.0);
  output.mode_infos[14] = OutputConfigurator::ModeInfo(1920, 1080, false, 40.0);
  output.mode_infos[15] = OutputConfigurator::ModeInfo(1920, 1080, false, 0.0);
  // Interlaced vs non-interlaced.
  output.mode_infos[16] = OutputConfigurator::ModeInfo(1280, 720, true, 60.0);
  output.mode_infos[17] = OutputConfigurator::ModeInfo(1280, 720, false, 40.0);
  // Interlaced only.
  output.mode_infos[18] = OutputConfigurator::ModeInfo(1024, 768, true, 0.0);
  output.mode_infos[19] = OutputConfigurator::ModeInfo(1024, 768, true, 40.0);
  output.mode_infos[20] = OutputConfigurator::ModeInfo(1024, 768, true, 60.0);
  // Mixed.
  output.mode_infos[21] = OutputConfigurator::ModeInfo(1024, 600, true, 60.0);
  output.mode_infos[22] = OutputConfigurator::ModeInfo(1024, 600, false, 40.0);
  output.mode_infos[23] = OutputConfigurator::ModeInfo(1024, 600, false, 50.0);
  // Just one interlaced mode.
  output.mode_infos[24] = OutputConfigurator::ModeInfo(640, 480, true, 60.0);
  // Refresh rate not available.
  output.mode_infos[25] = OutputConfigurator::ModeInfo(320, 200, false, 0.0);

  EXPECT_EQ(11u, OutputConfigurator::FindOutputModeMatchingSize(output,
                                                                1920, 1200));

  // Should pick highest refresh rate.
  EXPECT_EQ(13u, OutputConfigurator::FindOutputModeMatchingSize(output,
                                                                1920, 1080));

  // Should pick non-interlaced mode.
  EXPECT_EQ(17u, OutputConfigurator::FindOutputModeMatchingSize(output,
                                                                1280, 720));

  // Interlaced only. Should pick one with the highest refresh rate in
  // interlaced mode.
  EXPECT_EQ(20u, OutputConfigurator::FindOutputModeMatchingSize(output,
                                                                1024, 768));

  // Mixed: Should pick one with the highest refresh rate in
  // interlaced mode.
  EXPECT_EQ(23u, OutputConfigurator::FindOutputModeMatchingSize(output,
                                                                1024, 600));

  // Just one interlaced mode.
  EXPECT_EQ(24u, OutputConfigurator::FindOutputModeMatchingSize(output,
                                                                640, 480));

  // Refresh rate not available.
  EXPECT_EQ(25u, OutputConfigurator::FindOutputModeMatchingSize(output,
                                                                320, 200));

  // No mode found.
  EXPECT_EQ(0u, OutputConfigurator::FindOutputModeMatchingSize(output,
                                                               1440, 900));
}

TEST_F(OutputConfiguratorTest, ConnectSecondOutput) {
  InitWithSingleOutput();

  // Connect a second output and check that the configurator enters
  // extended mode.
  observer_.Reset();
  state_controller_.set_state(STATE_DUAL_EXTENDED);
  UpdateOutputs(2, true);
  const int kDualHeight =
      kSmallModeHeight + OutputConfigurator::kVerticalGap + kBigModeHeight;
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kBigModeWidth, kDualHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0,
                            kSmallModeHeight + OutputConfigurator::kVerticalGap,
                            kBigModeId, outputs_[1].output).c_str(),
                        kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  observer_.Reset();
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_DUAL_MIRROR));
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kUngrab, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Disconnect the second output.
  observer_.Reset();
  UpdateOutputs(1, true);
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        kUngrab, kProjectingOff, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Get rid of shared modes to force software mirroring.
  outputs_[1].mode_infos.erase(kSmallModeId);
  state_controller_.set_state(STATE_DUAL_EXTENDED);
  UpdateOutputs(2, true);
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kBigModeWidth, kDualHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0,
                            kSmallModeHeight + OutputConfigurator::kVerticalGap,
                            kBigModeId, outputs_[1].output).c_str(),
                        kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.software_mirroring_enabled());

  observer_.Reset();
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_DUAL_MIRROR));
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), delegate_->GetActionsAndClear());
  EXPECT_EQ(STATE_DUAL_EXTENDED, configurator_.output_state());
  EXPECT_TRUE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Setting STATE_DUAL_MIRROR should try to reconfigure.
  observer_.Reset();
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_DUAL_EXTENDED));
  EXPECT_EQ(JoinActions(NULL), delegate_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Set back to software mirror mode.
  observer_.Reset();
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_DUAL_MIRROR));
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_EQ(STATE_DUAL_EXTENDED, configurator_.output_state());
  EXPECT_TRUE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Disconnect the second output.
  observer_.Reset();
  UpdateOutputs(1, true);
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        kUngrab, kProjectingOff, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());
}

TEST_F(OutputConfiguratorTest, SetDisplayPower) {
  InitWithSingleOutput();

  state_controller_.set_state(STATE_DUAL_MIRROR);
  observer_.Reset();
  UpdateOutputs(2, true);
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_FALSE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turning off the internal display should switch the external display to
  // its native mode.
  observer_.Reset();
  configurator_.SetDisplayPower(DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
                                OutputConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kBigModeWidth, kBigModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, 0,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kBigModeId,
                            outputs_[1].output).c_str(),
                        kForceDPMS, kUngrab, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_EQ(STATE_SINGLE, configurator_.output_state());
  EXPECT_EQ(1, observer_.num_changes());

  // When all displays are turned off, the framebuffer should switch back
  // to the mirrored size.
  observer_.Reset();
  configurator_.SetDisplayPower(DISPLAY_POWER_ALL_OFF,
                                OutputConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, 0,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, 0,
                            outputs_[1].output).c_str(),
                        kUngrab, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_EQ(STATE_DUAL_MIRROR, configurator_.output_state());
  EXPECT_FALSE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turn all displays on and check that mirroring is still used.
  observer_.Reset();
  configurator_.SetDisplayPower(DISPLAY_POWER_ALL_ON,
                                OutputConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kForceDPMS, kUngrab, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_EQ(STATE_DUAL_MIRROR, configurator_.output_state());
  EXPECT_FALSE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Get rid of shared modes to force software mirroring.
  outputs_[1].mode_infos.erase(kSmallModeId);
  state_controller_.set_state(STATE_DUAL_MIRROR);
  observer_.Reset();
  UpdateOutputs(2, true);
  const int kDualHeight =
      kSmallModeHeight + OutputConfigurator::kVerticalGap + kBigModeHeight;
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kBigModeWidth, kDualHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0,
                            kSmallModeHeight + OutputConfigurator::kVerticalGap,
                            kBigModeId, outputs_[1].output).c_str(),
                        kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_EQ(STATE_DUAL_EXTENDED, configurator_.output_state());
  EXPECT_TRUE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turning off the internal display should switch the external display to
  // its native mode.
  observer_.Reset();
  configurator_.SetDisplayPower(DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
                                OutputConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kBigModeWidth, kBigModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, 0,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kBigModeId,
                            outputs_[1].output).c_str(),
                        kForceDPMS, kUngrab, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_EQ(STATE_SINGLE, configurator_.output_state());
  EXPECT_FALSE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // When all displays are turned off, the framebuffer should switch back
  // to the extended + software mirroring.
  observer_.Reset();
  configurator_.SetDisplayPower(DISPLAY_POWER_ALL_OFF,
                                OutputConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kBigModeWidth, kDualHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, 0,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0,
                            kSmallModeHeight + OutputConfigurator::kVerticalGap,
                            0, outputs_[1].output).c_str(),
                        kUngrab, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_EQ(STATE_DUAL_EXTENDED, configurator_.output_state());
  EXPECT_TRUE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());

  // Turn all displays on and check that mirroring is still used.
  observer_.Reset();
  configurator_.SetDisplayPower(DISPLAY_POWER_ALL_ON,
                                OutputConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kBigModeWidth, kDualHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0,
                            kSmallModeHeight + OutputConfigurator::kVerticalGap,
                            kBigModeId, outputs_[1].output).c_str(),
                        kForceDPMS, kUngrab, NULL),
            delegate_->GetActionsAndClear());
  EXPECT_EQ(STATE_DUAL_EXTENDED, configurator_.output_state());
  EXPECT_TRUE(mirroring_controller_.software_mirroring_enabled());
  EXPECT_EQ(1, observer_.num_changes());
}

TEST_F(OutputConfiguratorTest, SuspendAndResume) {
  InitWithSingleOutput();

  // No preparation is needed before suspending when the display is already
  // on.  The configurator should still reprobe on resume in case a display
  // was connected while suspended.
  configurator_.SuspendDisplays();
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
  configurator_.ResumeDisplays();
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        kForceDPMS, kUngrab, NULL),
            delegate_->GetActionsAndClear());

  // Now turn the display off before suspending and check that the
  // configurator turns it back on and syncs with the server.
  configurator_.SetDisplayPower(DISPLAY_POWER_ALL_OFF,
                                OutputConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, 0,
                            outputs_[0].output).c_str(),
                        kUngrab, NULL),
            delegate_->GetActionsAndClear());

  configurator_.SuspendDisplays();
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        kForceDPMS, kUngrab, kSync, NULL),
            delegate_->GetActionsAndClear());

  configurator_.ResumeDisplays();
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        kForceDPMS, kUngrab, NULL),
            delegate_->GetActionsAndClear());

  // If a second, external display is connected, the displays shouldn't be
  // powered back on before suspending.
  state_controller_.set_state(STATE_DUAL_MIRROR);
  UpdateOutputs(2, true);
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());

  configurator_.SetDisplayPower(DISPLAY_POWER_ALL_OFF,
                                OutputConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, 0,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, 0,
                            outputs_[1].output).c_str(),
                        kUngrab, NULL),
            delegate_->GetActionsAndClear());

  configurator_.SuspendDisplays();
  EXPECT_EQ(JoinActions(kGrab, kUngrab, kSync, NULL),
            delegate_->GetActionsAndClear());

  // If a display is disconnected while suspended, the configurator should
  // pick up the change.
  UpdateOutputs(1, false);
  configurator_.ResumeDisplays();
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, 0,
                            outputs_[0].output).c_str(),
                        kUngrab, NULL),
            delegate_->GetActionsAndClear());
}

TEST_F(OutputConfiguratorTest, Headless) {
  UpdateOutputs(0, false);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
  configurator_.Init(false);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
  configurator_.Start(0);
  EXPECT_EQ(JoinActions(kGrab, kInitXRandR, kForceDPMS, kUngrab,
                        kProjectingOff, NULL),
            delegate_->GetActionsAndClear());

  // Not much should happen when the display power state is changed while
  // no displays are connected.
  configurator_.SetDisplayPower(DISPLAY_POWER_ALL_OFF,
                                OutputConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), delegate_->GetActionsAndClear());
  configurator_.SetDisplayPower(DISPLAY_POWER_ALL_ON,
                                OutputConfigurator::kSetDisplayPowerNoFlags);
  EXPECT_EQ(JoinActions(kGrab, kForceDPMS, kUngrab, NULL),
            delegate_->GetActionsAndClear());

  // Connect an external display and check that it's configured correctly.
  outputs_[0] = outputs_[1];
  UpdateOutputs(1, true);
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kBigModeWidth, kBigModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kBigModeId,
                            outputs_[0].output).c_str(),
                        kUngrab, kProjectingOff, NULL),
            delegate_->GetActionsAndClear());
}

TEST_F(OutputConfiguratorTest, StartWithTwoOutputs) {
  UpdateOutputs(2, false);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
  configurator_.Init(false);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());

  state_controller_.set_state(STATE_DUAL_MIRROR);
  configurator_.Start(0);
  EXPECT_EQ(JoinActions(kGrab, kInitXRandR,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kForceDPMS, kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());
}

TEST_F(OutputConfiguratorTest, InvalidOutputStates) {
  UpdateOutputs(0, false);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
  configurator_.Init(false);
  configurator_.Start(0);
  observer_.Reset();
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_HEADLESS));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_SINGLE));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_DUAL_MIRROR));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_DUAL_EXTENDED));
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(3, observer_.num_failures());

  UpdateOutputs(1, true);
  observer_.Reset();
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_HEADLESS));
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_SINGLE));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_DUAL_MIRROR));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_DUAL_EXTENDED));
  EXPECT_EQ(1, observer_.num_changes());
  EXPECT_EQ(3, observer_.num_failures());

  state_controller_.set_state(STATE_DUAL_EXTENDED);
  UpdateOutputs(2, true);
  observer_.Reset();
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_HEADLESS));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_SINGLE));
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_DUAL_MIRROR));
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_DUAL_EXTENDED));
  EXPECT_EQ(2, observer_.num_changes());
  EXPECT_EQ(2, observer_.num_failures());
}

TEST_F(OutputConfiguratorTest, GetOutputStateForDisplaysWithoutId) {
  outputs_[0].has_display_id = false;
  UpdateOutputs(2, false);
  configurator_.Init(false);
  state_controller_.set_state(STATE_DUAL_MIRROR);
  configurator_.Start(0);
  EXPECT_EQ(STATE_DUAL_EXTENDED, configurator_.output_state());
}

TEST_F(OutputConfiguratorTest, GetOutputStateForDisplaysWithId) {
  outputs_[0].has_display_id = true;
  UpdateOutputs(2, false);
  configurator_.Init(false);
  state_controller_.set_state(STATE_DUAL_MIRROR);
  configurator_.Start(0);
  EXPECT_EQ(STATE_DUAL_MIRROR, configurator_.output_state());
}

TEST_F(OutputConfiguratorTest, AvoidUnnecessaryProbes) {
  InitWithSingleOutput();

  // X sends several events just after the configurator starts. Check that
  // the output change events don't trigger an additional probe, which can
  // block the UI thread.
  test_api_.SendScreenChangeEvent();
  EXPECT_EQ(kUpdateXRandR, delegate_->GetActionsAndClear());

  test_api_.SendOutputChangeEvent(
      outputs_[0].output, outputs_[0].crtc, outputs_[0].current_mode, true);
  test_api_.SendOutputChangeEvent(
      outputs_[1].output, outputs_[1].crtc, outputs_[1].current_mode, false);
  EXPECT_FALSE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());

  // Send an event stating that the second output is connected and check
  // that it gets updated.
  state_controller_.set_state(STATE_DUAL_MIRROR);
  UpdateOutputs(2, false);
  test_api_.SendOutputChangeEvent(
      outputs_[1].output, outputs_[1].crtc, outputs_[1].current_mode, true);
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());

  // An event about the second output changing modes should trigger another
  // reconfigure.
  test_api_.SendOutputChangeEvent(
      outputs_[1].output, outputs_[1].crtc, outputs_[1].native_mode, true);
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());

  // Disconnect the second output.
  UpdateOutputs(1, true);
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        kUngrab, kProjectingOff, NULL),
            delegate_->GetActionsAndClear());

  // An additional event about the second output being disconnected should
  // be ignored.
  test_api_.SendOutputChangeEvent(
      outputs_[1].output, outputs_[1].crtc, outputs_[1].current_mode, false);
  EXPECT_FALSE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());

  // Tell the delegate to report failure, which should result in the
  // second output sticking with its native mode.
  delegate_->set_configure_crtc_result(false);
  UpdateOutputs(2, true);
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());

  // An change event reporting a mode change on the second output should
  // trigger another reconfigure.
  delegate_->set_configure_crtc_result(true);
  test_api_.SendOutputChangeEvent(
      outputs_[1].output, outputs_[1].crtc, outputs_[1].mirror_mode, true);
  EXPECT_TRUE(test_api_.TriggerConfigureTimeout());
  EXPECT_EQ(JoinActions(kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());
}

TEST_F(OutputConfiguratorTest, UpdateCachedOutputsEvenAfterFailure) {
  InitWithSingleOutput();
  const std::vector<OutputConfigurator::OutputSnapshot>* cached =
      &test_api_.cached_outputs();
  ASSERT_EQ(static_cast<size_t>(1), cached->size());
  EXPECT_EQ(outputs_[0].current_mode, (*cached)[0].current_mode);

  // After connecting a second output, check that it shows up in
  // |cached_outputs_| even if an invalid state is requested.
  state_controller_.set_state(STATE_SINGLE);
  UpdateOutputs(2, true);
  cached = &test_api_.cached_outputs();
  ASSERT_EQ(static_cast<size_t>(2), cached->size());
  EXPECT_EQ(outputs_[0].current_mode, (*cached)[0].current_mode);
  EXPECT_EQ(outputs_[1].current_mode, (*cached)[1].current_mode);
}

TEST_F(OutputConfiguratorTest, PanelFitting) {
  // Configure the internal display to support only the big mode and the
  // external display to support only the small mode.
  outputs_[0].current_mode = kBigModeId;
  outputs_[0].native_mode = kBigModeId;
  outputs_[0].mode_infos.clear();
  outputs_[0].mode_infos[kBigModeId] = OutputConfigurator::ModeInfo(
      kBigModeWidth, kBigModeHeight, false, 60.0);

  outputs_[1].current_mode = kSmallModeId;
  outputs_[1].native_mode = kSmallModeId;
  outputs_[1].mode_infos.clear();
  outputs_[1].mode_infos[kSmallModeId] = OutputConfigurator::ModeInfo(
      kSmallModeWidth, kSmallModeHeight, false, 60.0);

  // The small mode should be added to the internal output when requesting
  // mirrored mode.
  UpdateOutputs(2, false);
  state_controller_.set_state(STATE_DUAL_MIRROR);
  configurator_.Init(true /* is_panel_fitting_enabled */);
  configurator_.Start(0);
  EXPECT_EQ(STATE_DUAL_MIRROR, configurator_.output_state());
  EXPECT_EQ(JoinActions(kGrab, kInitXRandR,
                        GetAddOutputModeAction(
                            outputs_[0].output, kSmallModeId).c_str(),
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kForceDPMS, kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());

  // Both outputs should be using the small mode.
  ASSERT_EQ(1, observer_.num_changes());
  ASSERT_EQ(static_cast<size_t>(2), observer_.latest_outputs().size());
  EXPECT_EQ(kSmallModeId, observer_.latest_outputs()[0].mirror_mode);
  EXPECT_EQ(kSmallModeId, observer_.latest_outputs()[0].current_mode);
  EXPECT_EQ(kSmallModeId, observer_.latest_outputs()[1].mirror_mode);
  EXPECT_EQ(kSmallModeId, observer_.latest_outputs()[1].current_mode);

  // Also check that the newly-added small mode is present in the internal
  // snapshot that was passed to the observer (http://crbug.com/289159).
  const OutputConfigurator::ModeInfo* info = OutputConfigurator::GetModeInfo(
      observer_.latest_outputs()[0], kSmallModeId);
  ASSERT_TRUE(info);
  EXPECT_EQ(kSmallModeWidth, info->width);
  EXPECT_EQ(kSmallModeHeight, info->height);
}

TEST_F(OutputConfiguratorTest, OutputProtection) {
  configurator_.Init(false);
  configurator_.Start(0);
  EXPECT_NE(kNoActions, delegate_->GetActionsAndClear());

  OutputConfigurator::OutputProtectionClientId id =
      configurator_.RegisterOutputProtectionClient();
  EXPECT_NE(0u, id);

  // One output.
  UpdateOutputs(1, true);
  EXPECT_NE(kNoActions, delegate_->GetActionsAndClear());
  uint32_t link_mask = 0;
  uint32_t protection_mask = 0;
  EXPECT_TRUE(configurator_.QueryOutputProtectionStatus(id,
                                                        outputs_[0].display_id,
                                                        &link_mask,
                                                        &protection_mask));
  EXPECT_EQ(static_cast<uint32_t>(OUTPUT_TYPE_INTERNAL), link_mask);
  EXPECT_EQ(static_cast<uint32_t>(OUTPUT_PROTECTION_METHOD_NONE),
            protection_mask);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());

  // Two outputs.
  UpdateOutputs(2, true);
  EXPECT_NE(kNoActions, delegate_->GetActionsAndClear());
  EXPECT_TRUE(configurator_.QueryOutputProtectionStatus(id,
                                                        outputs_[1].display_id,
                                                        &link_mask,
                                                        &protection_mask));
  EXPECT_EQ(static_cast<uint32_t>(OUTPUT_TYPE_HDMI),
            link_mask);
  EXPECT_EQ(static_cast<uint32_t>(OUTPUT_PROTECTION_METHOD_NONE),
            protection_mask);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());

  EXPECT_TRUE(
      configurator_.EnableOutputProtection(id,
                                           outputs_[1].display_id,
                                           OUTPUT_PROTECTION_METHOD_HDCP));
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1].output, HDCP_STATE_DESIRED),
            delegate_->GetActionsAndClear());

  // Enable protection.
  delegate_->set_hdcp_state(HDCP_STATE_ENABLED);
  EXPECT_TRUE(configurator_.QueryOutputProtectionStatus(id,
                                                        outputs_[1].display_id,
                                                        &link_mask,
                                                        &protection_mask));
  EXPECT_EQ(static_cast<uint32_t>(OUTPUT_TYPE_HDMI), link_mask);
  EXPECT_EQ(static_cast<uint32_t>(OUTPUT_PROTECTION_METHOD_HDCP),
            protection_mask);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());

  // Protections should be disabled after unregister.
  configurator_.UnregisterOutputProtectionClient(id);
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1].output, HDCP_STATE_UNDESIRED),
            delegate_->GetActionsAndClear());
}

TEST_F(OutputConfiguratorTest, OutputProtectionTwoClients) {
  OutputConfigurator::OutputProtectionClientId client1 =
      configurator_.RegisterOutputProtectionClient();
  OutputConfigurator::OutputProtectionClientId client2 =
      configurator_.RegisterOutputProtectionClient();
  EXPECT_NE(client1, client2);

  configurator_.Init(false);
  configurator_.Start(0);
  UpdateOutputs(2, true);
  EXPECT_NE(kNoActions, delegate_->GetActionsAndClear());

  // Clients never know state enableness for methods that they didn't request.
  EXPECT_TRUE(
      configurator_.EnableOutputProtection(client1,
                                           outputs_[1].display_id,
                                           OUTPUT_PROTECTION_METHOD_HDCP));
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1].output,
                                  HDCP_STATE_DESIRED).c_str(),
            delegate_->GetActionsAndClear());
  delegate_->set_hdcp_state(HDCP_STATE_ENABLED);

  uint32_t link_mask = 0;
  uint32_t protection_mask = 0;
  EXPECT_TRUE(configurator_.QueryOutputProtectionStatus(client1,
                                                        outputs_[1].display_id,
                                                        &link_mask,
                                                        &protection_mask));
  EXPECT_EQ(static_cast<uint32_t>(OUTPUT_TYPE_HDMI), link_mask);
  EXPECT_EQ(OUTPUT_PROTECTION_METHOD_HDCP, protection_mask);

  EXPECT_TRUE(configurator_.QueryOutputProtectionStatus(client2,
                                                        outputs_[1].display_id,
                                                        &link_mask,
                                                        &protection_mask));
  EXPECT_EQ(static_cast<uint32_t>(OUTPUT_TYPE_HDMI), link_mask);
  EXPECT_EQ(OUTPUT_PROTECTION_METHOD_NONE, protection_mask);

  // Protections will be disabled only if no more clients request them.
  EXPECT_TRUE(
      configurator_.EnableOutputProtection(client2,
                                           outputs_[1].display_id,
                                           OUTPUT_PROTECTION_METHOD_NONE));
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1].output,
                                  HDCP_STATE_DESIRED).c_str(),
            delegate_->GetActionsAndClear());
  EXPECT_TRUE(
      configurator_.EnableOutputProtection(client1,
                                           outputs_[1].display_id,
                                           OUTPUT_PROTECTION_METHOD_NONE));
  EXPECT_EQ(GetSetHDCPStateAction(outputs_[1].output,
                                  HDCP_STATE_UNDESIRED).c_str(),
            delegate_->GetActionsAndClear());
}

TEST_F(OutputConfiguratorTest, CTMForMultiScreens) {
  outputs_[0].touch_device_id = 1;
  outputs_[1].touch_device_id = 2;

  UpdateOutputs(2, false);
  configurator_.Init(false);
  state_controller_.set_state(STATE_DUAL_EXTENDED);
  configurator_.Start(0);

  const int kDualHeight =
      kSmallModeHeight + OutputConfigurator::kVerticalGap + kBigModeHeight;
  const int kDualWidth = kBigModeWidth;

  OutputConfigurator::CoordinateTransformation ctm1 = delegate_->get_ctm(1);
  OutputConfigurator::CoordinateTransformation ctm2 = delegate_->get_ctm(2);

  EXPECT_EQ(kSmallModeHeight - 1, round((kDualHeight - 1) * ctm1.y_scale));
  EXPECT_EQ(0, round((kDualHeight - 1) * ctm1.y_offset));

  EXPECT_EQ(kBigModeHeight - 1, round((kDualHeight - 1) * ctm2.y_scale));
  EXPECT_EQ(kSmallModeHeight + OutputConfigurator::kVerticalGap,
            round((kDualHeight - 1) * ctm2.y_offset));

  EXPECT_EQ(kSmallModeWidth - 1, round((kDualWidth - 1) * ctm1.x_scale));
  EXPECT_EQ(0, round((kDualWidth - 1) * ctm1.x_offset));

  EXPECT_EQ(kBigModeWidth - 1, round((kDualWidth - 1) * ctm2.x_scale));
  EXPECT_EQ(0, round((kDualWidth - 1) * ctm2.x_offset));
}

}  // namespace chromeos
