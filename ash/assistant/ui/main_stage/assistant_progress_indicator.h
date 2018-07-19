// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_PROGRESS_INDICATOR_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_PROGRESS_INDICATOR_H_

#include "base/macros.h"
#include "ui/views/view.h"

namespace ash {

class AssistantProgressIndicator : public views::View {
 public:
  AssistantProgressIndicator();
  ~AssistantProgressIndicator() override;

 private:
  void InitLayout();

  DISALLOW_COPY_AND_ASSIGN(AssistantProgressIndicator);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_ASSISTANT_PROGRESS_INDICATOR_H_
