// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISPLAY_REAL_OUTPUT_CONFIGURATOR_DELEGATE_H_
#define CHROMEOS_DISPLAY_REAL_OUTPUT_CONFIGURATOR_DELEGATE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chromeos/display/output_configurator.h"

typedef XID Window;

struct _XDisplay;
typedef struct _XDisplay Display;
struct _XRROutputInfo;
typedef _XRROutputInfo XRROutputInfo;
struct _XRRScreenResources;
typedef _XRRScreenResources XRRScreenResources;

namespace chromeos {

class RealOutputConfiguratorDelegate : public OutputConfigurator::Delegate {
 public:
  RealOutputConfiguratorDelegate();
  virtual ~RealOutputConfiguratorDelegate();

  // OutputConfigurator::Delegate overrides:
  virtual void SetPanelFittingEnabled(bool enabled) OVERRIDE;
  virtual void InitXRandRExtension(int* event_base) OVERRIDE;
  virtual void UpdateXRandRConfiguration(
      const base::NativeEvent& event) OVERRIDE;
  virtual void GrabServer() OVERRIDE;
  virtual void UngrabServer() OVERRIDE;
  virtual void SyncWithServer() OVERRIDE;
  virtual void SetBackgroundColor(uint32 color_argb) OVERRIDE;
  virtual void ForceDPMSOn() OVERRIDE;
  virtual std::vector<OutputConfigurator::OutputSnapshot> GetOutputs() OVERRIDE;
  virtual bool GetModeDetails(
      RRMode mode,
      int* width,
      int* height,
      bool* interlaced) OVERRIDE;
  virtual void ConfigureCrtc(OutputConfigurator::CrtcConfig* config) OVERRIDE;
  virtual void CreateFrameBuffer(
      int width,
      int height,
      const std::vector<OutputConfigurator::CrtcConfig>& configs) OVERRIDE;
  virtual void ConfigureCTM(
      int touch_device_id,
      const OutputConfigurator::CoordinateTransformation& ctm) OVERRIDE;
  virtual void SendProjectingStateToPowerManager(bool projecting) OVERRIDE;

 private:
  // Destroys unused CRTCs and parks used CRTCs in a way which allows a
  // framebuffer resize. This is faster than turning them off, resizing,
  // then turning them back on.
  void DestroyUnusedCrtcs(
      const std::vector<OutputConfigurator::CrtcConfig>& configs);

  // Returns whether |id| is configured to preserve aspect when scaling.
  bool IsOutputAspectPreservingScaling(RROutput id);

  // Looks for a mode on internal and external outputs having same
  // resolution.  |internal_info| and |external_info| are used to search
  // for the modes.  |internal_output_id| is used to create a new mode, if
  // applicable.  |try_creating|=true will enable creating panel-fitting
  // mode on the |internal_info| output instead of only searching for a
  // matching mode.  Note: it may lead to a crash, if |internal_info| is
  // not capable of panel fitting.  |preserve_aspect|=true will limit the
  // search / creation only to the modes having the native aspect ratio of
  // |external_info|.  |internal_mirror_mode| and |external_mirror_mode|
  // are the out-parameters for the modes on the two outputs which will
  // have the same resolution.  Returns false if no mode appropriate for
  // mirroring has been found/created.
  bool FindOrCreateMirrorMode(XRROutputInfo* internal_info,
                              XRROutputInfo* external_info,
                              RROutput internal_output_id,
                              bool try_creating,
                              bool preserve_aspect,
                              RRMode* internal_mirror_mode,
                              RRMode* external_mirror_mode);

  // Searches for touchscreens among input devices,
  // and tries to match them up to screens in |outputs|.
  // |outputs| is an array of detected screens.
  // If a touchscreen with same resolution as an output's native mode
  // is detected, its id will be stored in this output.
  void GetTouchscreens(
      std::vector<OutputConfigurator::OutputSnapshot>* outputs);

  Display* display_;
  Window window_;

  // Initialized when the server is grabbed and freed when it's ungrabbed.
  XRRScreenResources* screen_;

  // Used to enable modes which rely on panel fitting.
  bool is_panel_fitting_enabled_;

  DISALLOW_COPY_AND_ASSIGN(RealOutputConfiguratorDelegate);
};

}  // namespace chromeos

#endif  // CHROMEOS_DISPLAY_REAL_OUTPUT_CONFIGURATOR_DELEGATE_H_
