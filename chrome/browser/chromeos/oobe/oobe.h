// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_OOBE_OOBE_H_
#define CHROME_BROWSER_CHROMEOS_OOBE_OOBE_H_

#include "base/macros.h"
#include "ui/oobe/declarations/oobe_view_model.h"

namespace chromeos {

class Oobe : public gen::OobeViewModel {
 public:
  Oobe();
  ~Oobe() override;

  static void Register();

  // Overridden from wug::ViewModel:
  void Initialize() override;

 private:
  void OnButtonClicked() override;

  DISALLOW_COPY_AND_ASSIGN(Oobe);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_OOBE_OOBE_H_
