// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_DIALOG_PLATE_LOGO_VIEW_H_
#define ASH_ASSISTANT_UI_DIALOG_PLATE_LOGO_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/image_view.h"

namespace ash {

// TODO(b/78191547): Remove this class once the actual LogoView library port
// has been completed. This class is only a temporary placeholder.
class LogoView : public views::ImageView {
 public:
  enum class State {
    kUndefined,
    kListening,
    kMic,
  };

  LogoView();
  ~LogoView() override;

  void SetState(State state);

 private:
  DISALLOW_COPY_AND_ASSIGN(LogoView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_DIALOG_PLATE_LOGO_VIEW_H_
