// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_

#include "base/macros.h"
#include "chrome/browser/media/webrtc/desktop_media_picker.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Checkbox;
class TabbedPane;
}  // namespace views

class DesktopMediaListView;
class DesktopMediaSourceView;
class DesktopMediaPickerViews;

// Dialog view used for DesktopMediaPickerViews.
class DesktopMediaPickerDialogView : public views::DialogDelegateView,
                                     public views::TabbedPaneListener {
 public:
  DesktopMediaPickerDialogView(content::WebContents* parent_web_contents,
                               gfx::NativeWindow context,
                               DesktopMediaPickerViews* parent,
                               const base::string16& app_name,
                               const base::string16& target_name,
                               std::unique_ptr<DesktopMediaList> screen_list,
                               std::unique_ptr<DesktopMediaList> window_list,
                               std::unique_ptr<DesktopMediaList> tab_list,
                               bool request_audio);
  ~DesktopMediaPickerDialogView() override;

  // Called by parent (DesktopMediaPickerViews) when it's destroyed.
  void DetachParent();

  // Called by DesktopMediaListView.
  void OnSelectionChanged();
  void OnDoubleClick();

  // views::TabbedPaneListener overrides.
  void TabSelectedAt(int index) override;

  // views::View overrides.
  gfx::Size CalculatePreferredSize() const override;

  // views::DialogDelegateView overrides.
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  views::View* GetInitiallyFocusedView() override;
  bool ShouldDefaultButtonBeBlue() const override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  View* CreateExtraView() override;
  bool Accept() override;
  void DeleteDelegate() override;

  void OnMediaListRowsChanged();

  DesktopMediaListView* GetMediaListViewForTesting() const;
  DesktopMediaSourceView* GetMediaSourceViewForTesting(int index) const;
  views::Checkbox* GetCheckboxForTesting() const;
  int GetIndexOfSourceTypeForTesting(
      content::DesktopMediaID::Type source_type) const;
  views::TabbedPane* GetPaneForTesting() const;

 private:
  void SwitchSourceType(int index);

  DesktopMediaPickerViews* parent_;

  views::Label* description_label_;

  views::Checkbox* audio_share_checkbox_;

  views::TabbedPane* pane_;
  std::vector<DesktopMediaListView*> list_views_;
  std::vector<content::DesktopMediaID::Type> source_types_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaPickerDialogView);
};

// Implementation of DesktopMediaPicker for Views.
class DesktopMediaPickerViews : public DesktopMediaPicker {
 public:
  DesktopMediaPickerViews();
  ~DesktopMediaPickerViews() override;

  void NotifyDialogResult(content::DesktopMediaID source);

  // DesktopMediaPicker overrides.
  void Show(content::WebContents* web_contents,
            gfx::NativeWindow context,
            gfx::NativeWindow parent,
            const base::string16& app_name,
            const base::string16& target_name,
            std::unique_ptr<DesktopMediaList> screen_list,
            std::unique_ptr<DesktopMediaList> window_list,
            std::unique_ptr<DesktopMediaList> tab_list,
            bool request_audio,
            const DoneCallback& done_callback) override;

  DesktopMediaPickerDialogView* GetDialogViewForTesting() const {
    return dialog_;
  }

 private:
  DoneCallback callback_;

  // The |dialog_| is owned by the corresponding views::Widget instance.
  // When DesktopMediaPickerViews is destroyed the |dialog_| is destroyed
  // asynchronously by closing the widget.
  DesktopMediaPickerDialogView* dialog_;

  DISALLOW_COPY_AND_ASSIGN(DesktopMediaPickerViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_DESKTOP_CAPTURE_DESKTOP_MEDIA_PICKER_VIEWS_H_
