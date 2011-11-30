// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ABOUT_CHROME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_ABOUT_CHROME_VIEW_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/link_listener.h"
#include "ui/views/view.h"
#include "ui/views/window/dialog_delegate.h"

#if defined(OS_WIN) && !defined(USE_AURA)
#include "chrome/browser/google/google_update.h"
#endif

namespace views {
class Textfield;
class Throbber;
}

class Profile;

////////////////////////////////////////////////////////////////////////////////
//
// The AboutChromeView class is responsible for drawing the UI controls of the
// About Chrome dialog that allows the user to see what version is installed
// and check for updates.
//
////////////////////////////////////////////////////////////////////////////////
class AboutChromeView : public views::DialogDelegateView,
                        public views::LinkListener
#if defined(OS_WIN) && !defined(USE_AURA)
                        , public GoogleUpdateStatusListener
#endif
                        {
 public:
  explicit AboutChromeView(Profile* profile);
  virtual ~AboutChromeView();

  // Initialize the controls on the dialog.
  void Init();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

  // Overridden from views::DialogDelegate:
  virtual string16 GetDialogButtonLabel(ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonEnabled(ui::DialogButton button) const OVERRIDE;
  virtual bool IsDialogButtonVisible(ui::DialogButton button) const OVERRIDE;
  virtual int GetDefaultDialogButton() const OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual bool Accept() OVERRIDE;
  virtual views::View* GetContentsView() OVERRIDE;

  // Overridden from views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

#if defined(OS_WIN) && !defined(USE_AURA)
  // Overridden from GoogleUpdateStatusListener:
  virtual void OnReportResults(GoogleUpdateUpgradeResult result,
                               GoogleUpdateErrorCode error_code,
                               const string16& error_message,
                               const string16& version) OVERRIDE;
#endif

 private:
#if defined(OS_WIN) && !defined(USE_AURA)
  // Update the UI to show the status of the upgrade.
  void UpdateStatus(GoogleUpdateUpgradeResult result,
                    GoogleUpdateErrorCode error_code,
                    const string16& error_message);

  // Update the size of the window containing this view to account for more
  // text being displayed (error messages, etc). Returns how many pixels the
  // window was increased by (if any).
  int EnlargeWindowSizeIfNeeded();
#endif

  Profile* profile_;

  // UI elements on the dialog.
  views::ImageView* about_dlg_background_logo_;
  views::Label* about_title_label_;
  views::Textfield* version_label_;
  views::Label* copyright_label_;
  views::Label* main_text_label_;
  int main_text_label_height_;
  views::Link* chromium_url_;
  gfx::Rect chromium_url_rect_;
  views::Link* open_source_url_;
  gfx::Rect open_source_url_rect_;
  views::Link* terms_of_service_url_;
  gfx::Rect terms_of_service_url_rect_;
  // NULL in non-official builds.
  views::Label* error_label_;
  // UI elements we add to the parent view.
  scoped_ptr<views::Throbber> throbber_;
  views::ImageView success_indicator_;
  views::ImageView update_available_indicator_;
  views::ImageView timeout_indicator_;
  views::Label update_label_;

  // The dialog dimensions.
  gfx::Size dialog_dimensions_;

  // Keeps track of the visible state of the Restart Now button.
  bool restart_button_visible_;

  // The text to display as the main label of the About box. We draw this text
  // word for word with the help of the WordIterator, and make room for URLs
  // which are drawn using views::Link. See also |url_offsets_|.
  string16 main_label_chunk1_;
  string16 main_label_chunk2_;
  string16 main_label_chunk3_;
  string16 main_label_chunk4_;
  string16 main_label_chunk5_;
  // Determines the order of the two links we draw in the main label.
  bool chromium_url_appears_first_;

#if defined(OS_WIN) && !defined(USE_AURA)
  // The class that communicates with Google Update to find out if an update is
  // available and asks it to start an upgrade.
  scoped_refptr<GoogleUpdate> google_updater_;
#endif

  // The version Google Update reports is available to us.
  string16 new_version_available_;

  // Whether text direction is left-to-right or right-to-left.
  bool text_direction_is_rtl_;

  DISALLOW_COPY_AND_ASSIGN(AboutChromeView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ABOUT_CHROME_VIEW_H_
