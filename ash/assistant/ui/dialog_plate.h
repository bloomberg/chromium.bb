// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_DIALOG_PLATE_H_
#define ASH_ASSISTANT_UI_DIALOG_PLATE_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class DialogPlate : public views::View {
 public:
  DialogPlate();
  ~DialogPlate() override;

 private:
  void InitLayout();

  DISALLOW_COPY_AND_ASSIGN(DialogPlate);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_DIALOG_PLATE_H_
