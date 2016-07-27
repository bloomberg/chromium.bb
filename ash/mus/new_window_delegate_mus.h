// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MUS_NEW_WINDOW_DELEGATE_MUS_H_
#define ASH_MUS_NEW_WINDOW_DELEGATE_MUS_H_

#include "ash/common/new_window_delegate.h"
#include "base/macros.h"

namespace ash {
namespace mus {

class NewWindowDelegateMus : public NewWindowDelegate {
 public:
  NewWindowDelegateMus();
  ~NewWindowDelegateMus() override;

  // NewWindowDelegate:
  void NewTab() override;
  void NewWindow(bool incognito) override;
  void OpenFileManager() override;
  void OpenCrosh() override;
  void OpenGetHelp() override;
  void RestoreTab() override;
  void ShowKeyboardOverlay() override;
  void ShowTaskManager() override;
  void OpenFeedbackPage() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWindowDelegateMus);
};

}  // namespace mus
}  // namespace ash

#endif  // ASH_MUS_NEW_WINDOW_DELEGATE_MUS_H_
