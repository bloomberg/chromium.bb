// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_IMAGES_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_IMAGES_VIEW_H_
#pragma once

#include "views/controls/button/button.h"
#include "views/view.h"

#include <vector>

class SkBitmap;

namespace chromeos {

class UserImageButton;

// View used for selecting user image on OOBE screen.
class DefaultImagesView : public views::View,
                          public views::ButtonListener {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called when user clicks on capture button.
    virtual void OnCaptureButtonClicked() = 0;

    // Called when user selects an image.
    virtual void OnImageSelected(int image_index) = 0;
  };

  explicit DefaultImagesView(Delegate* delegate);
  virtual ~DefaultImagesView();

  // Initializes this view, its children and layout.
  void Init();

  // Returns the index of the selected default image or -1 if there's no
  // selected image.
  int GetDefaultImageIndex() const;

  // Allows to specify the selected image index specifically.
  void SetDefaultImageIndex(int index);

  // Unselects the selected image if there's one.
  void ClearSelection();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 private:
  // Resizes and sets image with specified resource id for the button.
  void InitButton(int resource_id, UserImageButton* button) const;

  // Initializes layout manager for this view.
  void InitLayout();

  // Vector of image buttons corresponding to default images and take photo
  // button.
  std::vector<UserImageButton*> default_images_;

  // Index of the currently selected image or -1.
  int selected_image_index_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(DefaultImagesView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEFAULT_IMAGES_VIEW_H_


