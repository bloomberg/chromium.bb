// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_UI_INTERFACE_H_
#define CHROME_BROWSER_VR_UI_INTERFACE_H_

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/vr/browser_ui_interface.h"
#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/keyboard_ui_interface.h"

namespace gfx {
class Point3F;
class PointF;
class Transform;
}

namespace vr {

class BrowserUiInterface;
class InputEvent;
class PlatformUiInputDelegate;
struct ControllerModel;
struct RenderInfo;
struct ReticleModel;
enum class UserFriendlyElementName;

using InputEventList = std::vector<std::unique_ptr<InputEvent>>;

// This interface represents the methods that should be called by its owner, and
// also serves to make all such methods virtual for the sake of separating a UI
// feature module.
class UiInterface : public BrowserUiInterface, public KeyboardUiInterface {
 public:
  ~UiInterface() override {}

  virtual base::WeakPtr<BrowserUiInterface> GetBrowserUiWeakPtr() = 0;

  // TODO(ymalik): We expose this to stop sending VSync to the WebVR page until
  // the splash screen has been visible for its minimum duration. The visibility
  // logic currently lives in the UI, and it'd be much cleaner if the UI didn't
  // have to worry about this, and if it were told to hide the splash screen
  // like other WebVR phases (e.g. OnWebVrFrameAvailable below).
  virtual bool CanSendWebVrVSync() = 0;
  virtual void SetAlertDialogEnabled(bool enabled,
                                     PlatformUiInputDelegate* delegate,
                                     float width,
                                     float height) = 0;
  virtual void SetContentOverlayAlertDialogEnabled(
      bool enabled,
      PlatformUiInputDelegate* delegate,
      float width_percentage,
      float height_percentage) = 0;
  virtual void SetAlertDialogSize(float width, float height) = 0;
  virtual void SetContentOverlayAlertDialogSize(float width_percentage,
                                                float height_percentage) = 0;
  virtual void SetDialogLocation(float x, float y) = 0;
  virtual void SetDialogFloating(bool floating) = 0;
  virtual void ShowPlatformToast(const base::string16& text) = 0;
  virtual void CancelPlatformToast() = 0;
  virtual bool ShouldRenderWebVr() = 0;
  virtual void OnGlInitialized(unsigned int content_texture_id,
                               GlTextureLocation content_location,
                               unsigned int content_overlay_texture_id,
                               GlTextureLocation content_overlay_location,
                               unsigned int ui_texture_id) = 0;
  virtual void OnPause() = 0;
  virtual void OnControllerUpdated(const ControllerModel& controller_model,
                                   const ReticleModel& reticle_model) = 0;
  virtual void OnProjMatrixChanged(const gfx::Transform& proj_matrix) = 0;
  virtual void OnWebVrFrameAvailable() = 0;
  virtual void OnWebVrTimedOut() = 0;
  virtual void OnWebVrTimeoutImminent() = 0;
  virtual bool IsControllerVisible() const = 0;
  virtual bool SkipsRedrawWhenNotDirty() const = 0;
  virtual void OnSwapContents(int new_content_id) = 0;
  virtual void OnContentBoundsChanged(int width, int height) = 0;
  virtual void AcceptDoffPromptForTesting() = 0;
  virtual gfx::Point3F GetTargetPointForTesting(
      UserFriendlyElementName element_name,
      const gfx::PointF& position) = 0;
  virtual bool IsContentVisibleAndOpaque() = 0;
  virtual void SetContentUsesQuadLayer(bool uses_quad_buffers) = 0;
  virtual gfx::Transform GetContentWorldSpaceTransform() = 0;
  virtual bool OnBeginFrame(const base::TimeTicks&, const gfx::Transform&) = 0;
  virtual bool SceneHasDirtyTextures() const = 0;
  virtual void UpdateSceneTextures() = 0;
  virtual void Draw(const RenderInfo&) = 0;
  virtual void DrawContent(const float (&uv_transform)[16],
                           float xborder,
                           float yborder) = 0;
  virtual void DrawWebXr(int texture_data_handle,
                         const float (&uv_transform)[16]) = 0;
  virtual void DrawWebVrOverlayForeground(const RenderInfo&) = 0;
  virtual bool HasWebXrOverlayElementsToDraw() = 0;
  virtual void HandleInput(base::TimeTicks current_time,
                           const RenderInfo& render_info,
                           const ControllerModel& controller_model,
                           ReticleModel* reticle_model,
                           InputEventList* input_event_list) = 0;
  virtual void HandleMenuButtonEvents(InputEventList* input_event_list) = 0;
  virtual void RequestFocus(int element_id) = 0;
  virtual void RequestUnfocus(int element_id) = 0;

  // This function calculates the minimal FOV (in degrees) which covers all
  // visible overflow elements as if it was viewing from fov_recommended. For
  // example, if fov_recommended is {20.f, 20.f, 20.f, 20.f}. And all elements
  // appear on screen within a FOV of {-11.f, 19.f, 9.f, 9.f} if we use
  // fov_recommended. Ideally, the calculated minimal FOV should be the same. In
  // practice, the elements might get clipped near the edge sometimes due to
  // float precision. To fix this, we add a small margin (1 degree) to all
  // directions. So the |out_fov| set by this function should be {-10.f, 20.f,
  // 10.f, 10.f} in the example case.
  // Using a smaller FOV could improve the performance a lot while we are
  // showing UIs on top of WebVR content.
  struct FovRectangle {
    float left;
    float right;
    float bottom;
    float top;
  };
  virtual std::pair<FovRectangle, FovRectangle>
  GetMinimalFovForWebXrOverlayElements(
      const gfx::Transform& left_view,
      const FovRectangle& fov_recommended_left,
      const gfx::Transform& right_view,
      const FovRectangle& fov_recommended_right,
      float z_near) = 0;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_UI_INTERFACE_H_
