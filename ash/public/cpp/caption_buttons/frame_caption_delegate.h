// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_CAPTION_DELEGATE_H_
#define ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_CAPTION_DELEGATE_H_

namespace aura {
class Window;
}

namespace ash {

// This delegate handles actions to be performed on a top level window. The
// implementation will depend on whether the controls are running in Ash or a
// client.
class FrameCaptionDelegate {
 public:
  // The previewed snap state for a window, corresponding to the use of a
  // PhantomWindowController.
  enum class SnapDirection {
    kNone,   // No snap preview.
    kLeft,   // The phantom window controller is previewing a snap to the left.
    kRight,  // The phantom window controller is previewing a snap to the left.
  };

  // Returns whether the snapping action on the size button should be enabled.
  virtual bool CanSnap(aura::Window* window) = 0;

  // Shows a preview (phantom window) for the given snap direction.
  virtual void ShowSnapPreview(aura::Window* window, SnapDirection snap) = 0;

  // Snaps the window in the given direction, if not kNone. Destroys the preview
  // window, if any.
  virtual void CommitSnap(aura::Window* window, SnapDirection snap) = 0;

 protected:
  virtual ~FrameCaptionDelegate() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTION_BUTTONS_FRAME_CAPTION_DELEGATE_H_
