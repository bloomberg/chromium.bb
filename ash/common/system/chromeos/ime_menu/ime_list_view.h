// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_LIST_VIEW_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_LIST_VIEW_H_

#include "ash/ash_export.h"
#include "ash/common/system/tray/ime_info.h"
#include "ash/common/system/tray/tray_details_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {
class MaterialKeyboardStatusRowView;

// The detailed view for showing IME list.
class ImeListView : public TrayDetailsView {
 public:
  enum SingleImeBehavior {
    // Shows the IME menu if there's only one IME in system.
    SHOW_SINGLE_IME,
    // Hides the IME menu if there's only one IME in system.
    HIDE_SINGLE_IME
  };

  ImeListView(SystemTrayItem* owner);

  ~ImeListView() override;

  // Initializes the contents of a newly-instantiated ImeListView.
  void Init(bool show_keyboard_toggle, SingleImeBehavior single_ime_behavior);

  // Updates the view.
  virtual void Update(const IMEInfoList& list,
                      const IMEPropertyInfoList& property_list,
                      bool show_keyboard_toggle,
                      SingleImeBehavior single_ime_behavior);

  // Removes (and destroys) all child views.
  virtual void ResetImeListView();

  // Closes the view.
  void CloseImeListView();

  void set_last_item_selected_with_keyboard(
      bool last_item_selected_with_keyboard) {
    last_item_selected_with_keyboard_ = last_item_selected_with_keyboard;
  }

  void set_should_focus_ime_after_selection_with_keyboard(
      const bool focus_current_ime) {
    should_focus_ime_after_selection_with_keyboard_ = focus_current_ime;
  }

  bool should_focus_ime_after_selection_with_keyboard() const {
    return should_focus_ime_after_selection_with_keyboard_;
  }

  // TrayDetailsView:
  void HandleViewClicked(views::View* view) override;
  void HandleButtonPressed(views::Button* sender,
                           const ui::Event& event) override;

  // views::View:
  void VisibilityChanged(View* starting_from, bool is_visible) override;

 private:
  friend class ImeListViewTestApi;

  // Appends the IMEs to the scrollable area of the detailed view.
  void AppendIMEList(const IMEInfoList& list);

  // Appends the IME listed to the scrollable area of the detailed view.
  void AppendIMEProperties(const IMEPropertyInfoList& property_list);

  // Appends the IMEs and properties to the scrollable area  in the material
  // design IME menu.
  void AppendImeListAndProperties(const IMEInfoList& list,
                                  const IMEPropertyInfoList& property_list);

  // Appends the on-screen keyboard status to the last area of the detailed
  // view.
  void AppendKeyboardStatus();

  // Inserts the material on-screen keyboard status in the detailed view.
  void PrependMaterialKeyboardStatus();

  // Requests focus on the current IME if it was selected with keyboard so that
  // accessible text will alert the user of the IME change.
  void FocusCurrentImeIfNeeded();

  std::map<views::View*, std::string> ime_map_;
  std::map<views::View*, std::string> property_map_;
  // On-screen keyboard view which is not used in material design.
  views::View* keyboard_status_;
  // On-screen keyboard view which is only used in material design.
  MaterialKeyboardStatusRowView* material_keyboard_status_view_;

  // The id of the last item selected with keyboard. It will be empty if the
  // item is not selected with keyboard.
  std::string last_selected_item_id_;

  // True if the last item is selected with keyboard.
  bool last_item_selected_with_keyboard_;

  // True if focus should be requested after switching IMEs with keyboard in
  // order to trigger spoken feedback with ChromeVox enabled.
  bool should_focus_ime_after_selection_with_keyboard_;

  // The item view of the current selected IME.
  views::View* current_ime_view_;

  DISALLOW_COPY_AND_ASSIGN(ImeListView);
};

class ASH_EXPORT ImeListViewTestApi {
 public:
  explicit ImeListViewTestApi(ImeListView* ime_list_view);
  virtual ~ImeListViewTestApi();

  views::View* GetToggleView() const;

  const std::map<views::View*, std::string>& ime_map() const {
    return ime_list_view_->ime_map_;
  }

 private:
  ImeListView* ime_list_view_;

  DISALLOW_COPY_AND_ASSIGN(ImeListViewTestApi);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_IME_MENU_IME_LIST_VIEW_H_
