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
#include "base/message_loop.h"
#include "base/stringprintf.h"
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

  TestDelegate() {}
  virtual ~TestDelegate() {}

  void set_outputs(
      const std::vector<OutputConfigurator::OutputSnapshot>& outputs) {
    outputs_ = outputs;
  }

  // Returns a comma-separated string describing the actions that were
  // requested since the previous call to GetActionsAndClear() (i.e.
  // results are non-repeatable).
  std::string GetActionsAndClear() {
    std::string actions = actions_;
    actions_.clear();
    return actions;
  }

  // Adds a mode to be returned by GetModeDetails().
  void AddMode(RRMode mode, int width, int height, bool interlaced) {
    modes_[mode] = ModeDetails(width, height, interlaced);
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
  virtual std::vector<OutputConfigurator::OutputSnapshot> GetOutputs()
      OVERRIDE {
    return outputs_;
  }
  virtual bool GetModeDetails(
      RRMode mode,
      int* width,
      int* height,
      bool* interlaced) OVERRIDE {
    std::map<RRMode, ModeDetails>::const_iterator it = modes_.find(mode);
    if (it == modes_.end())
      return false;

    if (width)
      *width = it->second.width;
    if (height)
      *height = it->second.height;
    if (interlaced)
      *interlaced = it->second.interlaced;
    return true;
  }
  virtual void ConfigureCrtc(OutputConfigurator::CrtcConfig* config) OVERRIDE {
    AppendAction(GetCrtcAction(config->crtc, config->x, config->y, config->mode,
                               config->output));
  }
  virtual void CreateFrameBuffer(
      int width,
      int height,
      const std::vector<OutputConfigurator::CrtcConfig>& configs) OVERRIDE {
    AppendAction(
        GetFramebufferAction(width,
                             height,
                             configs.size() >= 1 ? configs[0].crtc : 0,
                             configs.size() >= 2 ? configs[1].crtc : 0));
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

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class TestStateController : public OutputConfigurator::StateController {
 public:
  TestStateController() : state_(STATE_DUAL_EXTENDED) {}
  virtual ~TestStateController() {}

  void set_state(OutputState state) { state_ = state; }

  // OutputConfigurator::StateController overrides:
  virtual OutputState GetStateForOutputs(
      const std::vector<OutputInfo>& outputs) const OVERRIDE { return state_; }

 private:
  OutputState state_;

  DISALLOW_COPY_AND_ASSIGN(TestStateController);
};

class OutputConfiguratorTest : public testing::Test {
 public:
  OutputConfiguratorTest()
      : test_api_(&configurator_, TestDelegate::kXRandREventBase) {}
  virtual ~OutputConfiguratorTest() {}

  virtual void SetUp() OVERRIDE {
    delegate_ = new TestDelegate();
    configurator_.SetDelegateForTesting(
        scoped_ptr<OutputConfigurator::Delegate>(delegate_));
    configurator_.set_state_controller(&state_controller_);

    OutputConfigurator::OutputSnapshot* o = &outputs_[0];
    o->output = 1;
    o->crtc = 10;
    o->current_mode = kSmallModeId;
    o->native_mode = kSmallModeId;
    o->mirror_mode = kSmallModeId;
    o->y = 0;
    o->height = kSmallModeHeight;
    o->is_internal = true;
    o->is_aspect_preserving_scaling = true;
    o->touch_device_id = 0;

    o = &outputs_[1];
    o->output = 2;
    o->crtc = 11;
    o->current_mode = kBigModeId;
    o->native_mode = kBigModeId;
    o->mirror_mode = kSmallModeId;
    o->y = 0;
    o->height = kBigModeHeight;
    o->is_internal = false;
    o->is_aspect_preserving_scaling = true;
    o->touch_device_id = 0;

    UpdateOutputs(2);
    delegate_->AddMode(kSmallModeId, kSmallModeWidth, kSmallModeHeight, false);
    delegate_->AddMode(kBigModeId, kBigModeWidth, kBigModeHeight, false);
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
  // |outputs_|.
  virtual void UpdateOutputs(size_t num_outputs) {
    ASSERT_LE(num_outputs, arraysize(outputs_));
    std::vector<OutputConfigurator::OutputSnapshot> outputs;
    for (size_t i = 0; i < num_outputs; ++i)
      outputs.push_back(outputs_[i]);
    delegate_->set_outputs(outputs);
  }

  // Initializes |configurator_| with a single internal display.
  virtual void InitWithSingleOutput() {
    UpdateOutputs(1);
    EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
    configurator_.Init(false, 0);
    EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL),
              delegate_->GetActionsAndClear());
    configurator_.Start();
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
  OutputConfigurator configurator_;
  TestDelegate* delegate_;  // not owned
  OutputConfigurator::TestApi test_api_;

  OutputConfigurator::OutputSnapshot outputs_[2];

 private:
  DISALLOW_COPY_AND_ASSIGN(OutputConfiguratorTest);
};

}  // namespace

TEST_F(OutputConfiguratorTest, IsInternalOutputName) {
  EXPECT_TRUE(OutputConfigurator::IsInternalOutputName("LVDS"));
  EXPECT_TRUE(OutputConfigurator::IsInternalOutputName("eDP"));
  EXPECT_TRUE(OutputConfigurator::IsInternalOutputName("LVDSxx"));
  EXPECT_TRUE(OutputConfigurator::IsInternalOutputName("eDPzz"));

  EXPECT_FALSE(OutputConfigurator::IsInternalOutputName("xyz"));
  EXPECT_FALSE(OutputConfigurator::IsInternalOutputName("abcLVDS"));
  EXPECT_FALSE(OutputConfigurator::IsInternalOutputName("cdeeDP"));
  EXPECT_FALSE(OutputConfigurator::IsInternalOutputName("LVD"));
  EXPECT_FALSE(OutputConfigurator::IsInternalOutputName("eD"));
}

TEST_F(OutputConfiguratorTest, ConnectSecondOutput) {
  InitWithSingleOutput();

  // Connect a second output and check that the configurator enters
  // extended mode.
  UpdateOutputs(2);
  const int kDualHeight =
      kSmallModeHeight + OutputConfigurator::kVerticalGap + kBigModeHeight;
  state_controller_.set_state(STATE_DUAL_EXTENDED);
  EXPECT_TRUE(test_api_.SendOutputChangeEvents(true));
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

  // Disconnect the second output.
  UpdateOutputs(1);
  EXPECT_TRUE(test_api_.SendOutputChangeEvents(false));
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        kUngrab, kProjectingOff, NULL),
            delegate_->GetActionsAndClear());
}

TEST_F(OutputConfiguratorTest, SetDisplayPower) {
  InitWithSingleOutput();

  UpdateOutputs(2);
  state_controller_.set_state(STATE_DUAL_MIRROR);
  EXPECT_TRUE(test_api_.SendOutputChangeEvents(true));
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kSmallModeWidth, kSmallModeHeight,
                            outputs_[0].crtc, outputs_[1].crtc).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kSmallModeId,
                            outputs_[0].output).c_str(),
                        GetCrtcAction(outputs_[1].crtc, 0, 0, kSmallModeId,
                            outputs_[1].output).c_str(),
                        kUngrab, kProjectingOn, NULL),
            delegate_->GetActionsAndClear());

  // Turning off the internal display should switch the external display to
  // its native mode.
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

  // When all displays are turned off, the framebuffer should switch back
  // to the mirrored size.
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

  // Turn all displays on and check that mirroring is still used.
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
  UpdateOutputs(2);
  state_controller_.set_state(STATE_DUAL_MIRROR);
  EXPECT_TRUE(test_api_.SendOutputChangeEvents(true));
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

  // If a display is disconnected while resuming, the configurator should
  // pick up the change.
  UpdateOutputs(1);
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
  UpdateOutputs(0);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
  configurator_.Init(false, 0);
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), delegate_->GetActionsAndClear());
  configurator_.Start();
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
  outputs_[0].is_internal = false;
  outputs_[0].native_mode = kBigModeId;
  UpdateOutputs(1);
  EXPECT_TRUE(test_api_.SendOutputChangeEvents(true));
  EXPECT_EQ(JoinActions(kUpdateXRandR, kGrab,
                        GetFramebufferAction(kBigModeWidth, kBigModeHeight,
                            outputs_[0].crtc, 0).c_str(),
                        GetCrtcAction(outputs_[0].crtc, 0, 0, kBigModeId,
                            outputs_[0].output).c_str(),
                        kUngrab, kProjectingOff, NULL),
            delegate_->GetActionsAndClear());
}

TEST_F(OutputConfiguratorTest, StartWithTwoOutputs) {
  UpdateOutputs(2);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
  configurator_.Init(false, 0);
  EXPECT_EQ(JoinActions(kGrab, kUngrab, NULL), delegate_->GetActionsAndClear());

  state_controller_.set_state(STATE_DUAL_MIRROR);
  configurator_.Start();
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
  UpdateOutputs(0);
  EXPECT_EQ(kNoActions, delegate_->GetActionsAndClear());
  configurator_.Init(false, 0);
  configurator_.Start();
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_HEADLESS));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_SINGLE));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_DUAL_MIRROR));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_DUAL_EXTENDED));

  UpdateOutputs(1);
  EXPECT_TRUE(test_api_.SendOutputChangeEvents(true));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_HEADLESS));
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_SINGLE));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_DUAL_MIRROR));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_DUAL_EXTENDED));

  UpdateOutputs(2);
  state_controller_.set_state(STATE_DUAL_EXTENDED);
  EXPECT_TRUE(test_api_.SendOutputChangeEvents(true));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_HEADLESS));
  EXPECT_FALSE(configurator_.SetDisplayMode(STATE_SINGLE));
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_DUAL_MIRROR));
  EXPECT_TRUE(configurator_.SetDisplayMode(STATE_DUAL_EXTENDED));
}

}  // namespace chromeos
