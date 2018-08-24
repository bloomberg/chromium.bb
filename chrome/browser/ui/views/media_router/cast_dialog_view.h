// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/media_router/cast_dialog_controller.h"
#include "chrome/browser/ui/views/media_router/cast_dialog_metrics.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_runner.h"

class Browser;

namespace gfx {
class Canvas;
}  // namespace gfx

namespace media_router {

class CastDialogSinkButton;
struct UIMediaSink;

// View component of the Cast dialog that allows users to start and stop Casting
// to devices. The list of devices used to populate the dialog is supplied by
// CastDialogModel.
class CastDialogView : public views::BubbleDialogDelegateView,
                       public views::ButtonListener,
                       public CastDialogController::Observer,
                       public ui::SimpleMenuModel::Delegate {
 public:
  // Shows the singleton dialog anchored to the Cast toolbar icon. Requires that
  // BrowserActionsContainer exists for |browser|.
  static void ShowDialogWithToolbarAction(CastDialogController* controller,
                                          Browser* browser,
                                          const base::Time& start_time);

  // Shows the singleton dialog anchored to the top-center of the browser
  // window.
  static void ShowDialogTopCentered(CastDialogController* controller,
                                    Browser* browser,
                                    const base::Time& start_time);

  // No-op if the dialog is currently not shown.
  static void HideDialog();

  static bool IsShowing();

  // Returns nullptr if the dialog is currently not shown.
  static views::Widget* GetCurrentDialogWidget();

  // views::WidgetDelegateView:
  bool ShouldShowCloseButton() const override;

  // views::WidgetDelegate:
  base::string16 GetWindowTitle() const override;

  // ui::DialogModel:
  int GetDialogButtons() const override;

  // views::DialogDelegate:
  views::View* CreateExtraView() override;
  bool Close() override;

  // CastDialogController::Observer:
  void OnModelUpdated(const CastDialogModel& model) override;
  void OnControllerInvalidated() override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void OnPaint(gfx::Canvas* canvas) override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // Called by tests.
  const std::vector<CastDialogSinkButton*>& sink_buttons_for_test() const {
    return sink_buttons_;
  }
  views::ScrollView* scroll_view_for_test() { return scroll_view_; }
  views::View* no_sinks_view_for_test() { return no_sinks_view_; }
  views::Button* sources_button_for_test() { return sources_button_; }
  ui::SimpleMenuModel* sources_menu_model_for_test() {
    return sources_menu_model_.get();
  }
  views::MenuRunner* sources_menu_runner_for_test() {
    return sources_menu_runner_.get();
  }

 private:
  friend class CastDialogViewTest;
  FRIEND_TEST_ALL_PREFIXES(CastDialogViewTest, ShowAndHideDialog);

  // Instantiates and shows the singleton dialog. The dialog must not be
  // currently shown.
  static void ShowDialog(views::View* anchor_view,
                         views::BubbleBorder::Arrow anchor_position,
                         CastDialogController* controller,
                         Browser* browser,
                         const base::Time& start_time);

  CastDialogView(views::View* anchor_view,
                 views::BubbleBorder::Arrow anchor_position,
                 CastDialogController* controller,
                 Browser* browser,
                 const base::Time& start_time);
  ~CastDialogView() override;

  // views::BubbleDialogDelegateView:
  void Init() override;
  void WindowClosing() override;

  void ShowNoSinksView();
  void ShowScrollView();

  // Applies the stored scroll state.
  void RestoreSinkListState();

  // Populates the scroll view containing sinks using the data in |model|.
  void PopulateScrollView(const std::vector<UIMediaSink>& sinks);

  // Shows the sources menu that allows the user to choose a source to cast.
  void ShowSourcesMenu();

  void SinkPressed(size_t index);

  void MaybeSizeToContents();

  // Returns the cast mode that is selected in the sources menu and supported by
  // |sink|. Returns nullopt if no such cast mode exists.
  base::Optional<MediaCastMode> GetCastModeToUse(const UIMediaSink& sink) const;

  // Disables sink buttons for sinks that do not support the currently selected
  // source.
  void DisableUnsupportedSinks();

  // Posts a delayed task to record the number of sinks shown with the metrics
  // recorder.
  void RecordSinkCountWithDelay();

  // Records the number of sinks shown with the metrics recorder.
  void RecordSinkCount();

  // The singleton dialog instance. This is a nullptr when a dialog is not
  // shown.
  static CastDialogView* instance_;

  // Title shown at the top of the dialog.
  base::string16 dialog_title_;

  // The source selected in the sources menu. This defaults to "tab"
  // (presentation or tab mirroring). "Tab" is represented by a single item in
  // the sources menu.
  int selected_source_;

  // Contains references to sink buttons in the order they appear.
  std::vector<CastDialogSinkButton*> sink_buttons_;

  CastDialogController* controller_;

  // ScrollView containing the list of sink buttons.
  views::ScrollView* scroll_view_ = nullptr;

  // View shown while there are no sinks.
  views::View* no_sinks_view_ = nullptr;

  Browser* const browser_;

  // How much |scroll_view_| is scrolled downwards in pixels. Whenever the sink
  // list is updated the scroll position gets reset, so we must manually restore
  // it to this value.
  int scroll_position_ = 0;

  // The sources menu allows the user to choose a source to cast.
  views::Button* sources_button_ = nullptr;
  std::unique_ptr<ui::SimpleMenuModel> sources_menu_model_;
  std::unique_ptr<views::MenuRunner> sources_menu_runner_;

  // Records UMA metrics for the dialog's behavior.
  CastDialogMetrics metrics_;

  base::WeakPtrFactory<CastDialogView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CastDialogView);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_VIEWS_MEDIA_ROUTER_CAST_DIALOG_VIEW_H_
