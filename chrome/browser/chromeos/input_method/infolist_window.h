// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "ui/gfx/font_list.h"
#include "ui/views/widget/widget_delegate.h"

namespace chromeos {
namespace input_method {

class InfolistEntryView;

// TODO(mukai): move this model to another place, src/chromeos/ime or
// src/ui/base/ime.
struct InfolistEntry {
  base::string16 title;
  base::string16 body;
  bool highlighted;

  InfolistEntry(const base::string16& title, const base::string16& body);
  bool operator==(const InfolistEntry& entry) const;
  bool operator!=(const InfolistEntry& entry) const;
};

// A widget delegate representing the infolist window UI.
class InfolistWindow : public views::WidgetDelegateView {
 public:
  explicit InfolistWindow(const std::vector<InfolistEntry>& entries);
  virtual ~InfolistWindow();
  void InitWidget(gfx::NativeWindow parent, const gfx::Rect& initial_bounds);

  // Updates infolist contents with |entries|.
  void Relayout(const std::vector<InfolistEntry>& entries);

  // Show/hide itself with a delay.
  void ShowWithDelay();
  void HideWithDelay();

  // Show/hide without delays.
  void ShowImmediately();
  void HideImmediately();

 private:
  // views::WidgetDelegate implementation.
  virtual views::View* GetContentsView() OVERRIDE;
  virtual void WindowClosing() OVERRIDE;

  // The list of visible entries. Owned by views hierarchy.
  std::vector<InfolistEntryView*> entry_views_;

  // Information title font.
  gfx::FontList title_font_;

  // Information description font.
  gfx::FontList description_font_;

  base::OneShotTimer<views::Widget> show_hide_timer_;

  DISALLOW_COPY_AND_ASSIGN(InfolistWindow);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INFOLIST_WINDOW_H_
