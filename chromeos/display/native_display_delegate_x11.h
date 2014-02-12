// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DISPLAY_NATIVE_DISPLAY_DELEGATE_X11_H_
#define CHROMEOS_DISPLAY_NATIVE_DISPLAY_DELEGATE_X11_H_

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

class NativeDisplayDelegateX11
    : public OutputConfigurator::NativeDisplayDelegate {
 public:
  NativeDisplayDelegateX11();
  virtual ~NativeDisplayDelegateX11();

  // OutputConfigurator::Delegate overrides:
  virtual void InitXRandRExtension(int* event_base) OVERRIDE;
  virtual void UpdateXRandRConfiguration(const base::NativeEvent& event)
      OVERRIDE;
  virtual void GrabServer() OVERRIDE;
  virtual void UngrabServer() OVERRIDE;
  virtual void SyncWithServer() OVERRIDE;
  virtual void SetBackgroundColor(uint32 color_argb) OVERRIDE;
  virtual void ForceDPMSOn() OVERRIDE;
  virtual std::vector<OutputConfigurator::OutputSnapshot> GetOutputs() OVERRIDE;
  virtual void AddOutputMode(RROutput output, RRMode mode) OVERRIDE;
  virtual bool ConfigureCrtc(RRCrtc crtc,
                             RRMode mode,
                             RROutput output,
                             int x,
                             int y) OVERRIDE;
  virtual void CreateFrameBuffer(
      int width,
      int height,
      const std::vector<OutputConfigurator::OutputSnapshot>& outputs) OVERRIDE;
  virtual bool GetHDCPState(RROutput id, ui::HDCPState* state) OVERRIDE;
  virtual bool SetHDCPState(RROutput id, ui::HDCPState state) OVERRIDE;

 private:
  // Initializes |mode_info| to contain details corresponding to |mode|. Returns
  // true on success.
  bool InitModeInfo(RRMode mode, OutputConfigurator::ModeInfo* mode_info);

  // Helper method for GetOutputs() that returns an OutputSnapshot struct based
  // on the passed-in information. Further initialization is required (e.g.
  // |selected_mode|, |mirror_mode|, and |touch_device_id|).
  OutputConfigurator::OutputSnapshot InitOutputSnapshot(RROutput id,
                                                        XRROutputInfo* info,
                                                        RRCrtc* last_used_crtc,
                                                        int index);

  // Destroys unused CRTCs and parks used CRTCs in a way which allows a
  // framebuffer resize. This is faster than turning them off, resizing,
  // then turning them back on.
  void DestroyUnusedCrtcs(
      const std::vector<OutputConfigurator::OutputSnapshot>& outputs);

  // Returns whether |id| is configured to preserve aspect when scaling.
  bool IsOutputAspectPreservingScaling(RROutput id);

  Display* display_;
  Window window_;

  // Initialized when the server is grabbed and freed when it's ungrabbed.
  XRRScreenResources* screen_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateX11);
};

}  // namespace chromeos

#endif  // CHROMEOS_DISPLAY_NATIVE_DISPLAY_DELEGATE_X11_H_
