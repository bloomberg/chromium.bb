// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ABOUT_CHROME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_ABOUT_CHROME_VIEW_H_
#pragma once

#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/controls/link.h"
#include "views/view.h"
#include "views/window/dialog_delegate.h"

#if defined(OS_WIN) || defined(OS_CHROMEOS)
#include "chrome/browser/google/google_update.h"
#endif
#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/version_loader.h"
#endif

namespace views {
class Textfield;
class Throbber;
class Window;
}

class Profile;

////////////////////////////////////////////////////////////////////////////////
//
// The AboutChromeView class is responsible for drawing the UI controls of the
// About Chrome dialog that allows the user to see what version is installed
// and check for updates.
//
////////////////////////////////////////////////////////////////////////////////
class AboutChromeView : public views::View,
                        public views::DialogDelegate,
                        public views::LinkController
#if defined(OS_WIN) || defined(OS_CHROMEOS)
                        , public GoogleUpdateStatusListener
#endif
                        {
 public:
  explicit AboutChromeView(Profile* profile);
  virtual ~AboutChromeView();

  // Initialize the controls on the dialog.
  void Init();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void OnPaint(gfx::Canvas* canvas);
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);

  // Overridden from views::DialogDelegate:
  virtual std::wstring GetDialogButtonLabel(
      MessageBoxFlags::DialogButton button) const;
  virtual bool IsDialogButtonEnabled(
      MessageBoxFlags::DialogButton button) const;
  virtual bool IsDialogButtonVisible(
      MessageBoxFlags::DialogButton button) const;
  virtual int GetDefaultDialogButton() const;
  virtual bool CanResize() const;
  virtual bool CanMaximize() const;
  virtual bool IsAlwaysOnTop() const;
  virtual bool HasAlwaysOnTopMenu() const;
  virtual bool IsModal() const;
  virtual std::wstring GetWindowTitle() const;
  virtual bool Accept();
  virtual views::View* GetContentsView();

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  // Overridden from GoogleUpdateStatusListener:
  virtual void OnReportResults(GoogleUpdateUpgradeResult result,
                               GoogleUpdateErrorCode error_code,
                               const std::wstring& version);
#endif

 private:
#if defined(OS_WIN) || defined(OS_CHROMEOS)
  // Update the UI to show the status of the upgrade.
  void UpdateStatus(GoogleUpdateUpgradeResult result,
                    GoogleUpdateErrorCode error_code);
#endif

#if defined(OS_CHROMEOS)
  // Callback from chromeos::VersionLoader giving the version.
  void OnOSVersion(chromeos::VersionLoader::Handle handle,
                   std::string version);
#endif


  Profile* profile_;

  // UI elements on the dialog.
  views::ImageView* about_dlg_background_logo_;
  views::Label* about_title_label_;
  views::Textfield* version_label_;
#if defined(OS_CHROMEOS)
  views::Textfield* os_version_label_;
#endif
  views::Label* copyright_label_;
  views::Label* main_text_label_;
  int main_text_label_height_;
  views::Link* chromium_url_;
  gfx::Rect chromium_url_rect_;
  views::Link* open_source_url_;
  gfx::Rect open_source_url_rect_;
  views::Link* terms_of_service_url_;
  gfx::Rect terms_of_service_url_rect_;
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
  std::wstring main_label_chunk1_;
  std::wstring main_label_chunk2_;
  std::wstring main_label_chunk3_;
  std::wstring main_label_chunk4_;
  std::wstring main_label_chunk5_;
  // Determines the order of the two links we draw in the main label.
  bool chromium_url_appears_first_;

#if defined(OS_WIN) || defined(OS_CHROMEOS)
  // The class that communicates with Google Update to find out if an update is
  // available and asks it to start an upgrade.
  scoped_refptr<GoogleUpdate> google_updater_;
#endif

  // Our current version.
  std::string current_version_;

  // Additional information about the version (channel and build number).
  std::string version_details_;

  // The version Google Update reports is available to us.
  std::wstring new_version_available_;

  // Whether text direction is left-to-right or right-to-left.
  bool text_direction_is_rtl_;

#if defined(OS_CHROMEOS)
  // Handles asynchronously loading the version.
  chromeos::VersionLoader loader_;

  // Used to request the version.
  CancelableRequestConsumer consumer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AboutChromeView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ABOUT_CHROME_VIEW_H_
