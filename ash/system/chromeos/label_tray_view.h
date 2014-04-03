// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_LABEL_TRAY_VIEW_H_
#define ASH_SYSTEM_CHROMEOS_LABEL_TRAY_VIEW_H_

#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace ash {

class ViewClickListener;

// View for simple information in tray. Automatically hides when message is
// empty. Supports multiline messages.

class LabelTrayView : public views::View {
 public:
  LabelTrayView(ViewClickListener* click_listener, int icon_resource_id);
  virtual ~LabelTrayView();
  void SetMessage(const base::string16& message);
 private:
  views::View* CreateChildView(const base::string16& message) const;

  ViewClickListener* click_listener_;
  int icon_resource_id_;
  base::string16 message_;

  DISALLOW_COPY_AND_ASSIGN(LabelTrayView);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_LABEL_TRAY_VIEW_H_
