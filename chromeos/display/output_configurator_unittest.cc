// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/display/output_configurator.h"

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

  TestDelegate() : configure_crtc_result_(true) {}
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

  // Returns a comma-separated string describing the actions that were
  // requested since the previous call to GetActionsAndClear() (i.e.
  // results are non-repeatable).
  std::string GetActionsAndClear() {
    std::string actions = actions_;
    actions_.clear();
    return actions;
  }

  // OutputConfigurator::Delegate overrides:
  virtual void SetPanelFittingEnabled(bool enabled) OVERRIDE {}
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
  virtual std::vector<OutputConfigurator::OutputSnapshot> GetOutputs(
      const OutputConfigurator::StateController* controller) OVERRIDE {
    return outputs_;
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
  }
  virtual void SendProjectingStateToPowerManager(bool projecting) OVERRIDE {
    AppendAction(projecting ? kProjectingOn : kProjectingOff);
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

  // Outputs to be returned by GetOutputs().
  std::vector<OutputConfigurator::OutputSnapshot> outputs_;

  std::string actions_;

  // Return value returned by ConfigureCrtc().
  bool configure_crtc_result_;

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
  std::vector<OutputConfigurator::OutputSnapshot> latest_outputs() const {
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
    o->selected_mode = kSmallModeId;
    o->mirror_mode = kSmallModeId;
    o->x = 0;
    o->y = 0;
    o->is_internal = true;
    o->is_aspect_preserving_scaling = true;
    o->mode_infos[kSmallModeId] = small_mode_info;
    o->touch_device_id = 0;
    o->has_display_id = true;

    o = &outputs_[1];
    o->output = 2;
    o->crtc = 11;
    o->current_mode = kBigModeId;
    o->native_mode = kBigModeId;
    o->selected_mode = kBigModeId;
    o->mirror_mode = kSmallModeId;
    o->x = 0;
    o->y = 0;
    o->is_internal = false;
    o->is_aspect_preserving_scaling = true;
    o->mode_infos[kSmallModeId] = small_mode_info;
    o->mode_infos[kBigModeId] = big_mode_info;
    o->touch_device_id = 0;
    o->has_display_id = true;

    UpdateOutputs(2, false);
  }

  void DisableNativeMirroring() {
    outputs_[0].mirror_mode = outputs_[1].mirror_mode = 0L;
  }

 protected:
  // Predefined modes that can be used by outputs.
  static const int kSmallModeId = 20;
  static const int kSmallModeWidth = 1366;
  static const int kSmallModeHeight = 768;

  static const int kBigModeId = 21;
  static const int kBigModeWidth = 2560;
  static const int kBigModeHeight = 1600;

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

}  // namespace

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

  // Software Mirroring
  DisableNativeMirroring();
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

  // Software Mirroring
  DisableNativeMirroring();
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

}  // namespace chromeos
