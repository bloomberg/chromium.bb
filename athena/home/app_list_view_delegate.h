// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_APP_LIST_VIEW_DELEGATE_H_
#define ATHENA_HOME_APP_LIST_VIEW_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_view_delegate.h"

namespace app_list {
class SearchController;
}

namespace athena {
class AppModelBuilder;
class SearchControllerFactory;

class AppListViewDelegate : public app_list::AppListViewDelegate {
 public:
  AppListViewDelegate(AppModelBuilder* model_builder,
                      SearchControllerFactory* search_factory);
  ~AppListViewDelegate() override;

 private:
  // Overridden from app_list::AppListViewDelegate:
  bool ForceNativeDesktop() const override;
  void SetProfileByPath(const base::FilePath& profile_path) override;
  app_list::AppListModel* GetModel() override;
  app_list::SpeechUIModel* GetSpeechUI() override;
  void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) override;
  void StartSearch() override;
  void StopSearch() override;
  void OpenSearchResult(app_list::SearchResult* result,
                        bool auto_launch,
                        int event_flags) override;
  void InvokeSearchResultAction(app_list::SearchResult* result,
                                int action_index,
                                int event_flags) override;
  base::TimeDelta GetAutoLaunchTimeout() override;
  void AutoLaunchCanceled() override;
  void ViewInitialized() override;
  void Dismiss() override;
  void ViewClosing() override;
  gfx::ImageSkia GetWindowIcon() override;
  void OpenSettings() override;
  void OpenHelp() override;
  void OpenFeedback() override;
  void ToggleSpeechRecognition() override;
  void ShowForProfileByPath(const base::FilePath& profile_path) override;
  views::View* CreateStartPageWebView(const gfx::Size& size) override;
  std::vector<views::View*> CreateCustomPageWebViews(
      const gfx::Size& size) override;
  void CustomLauncherPageAnimationChanged(double progress) override;
  void CustomLauncherPagePopSubpage() override;
  bool IsSpeechRecognitionEnabled() override;
  const Users& GetUsers() const override;
  bool ShouldCenterWindow() const override;

  scoped_ptr<app_list::AppListModel> model_;
  scoped_ptr<app_list::SpeechUIModel> speech_ui_;
  Users users_;
  scoped_ptr<app_list::SearchController> search_controller_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegate);
};

}  // namespace athena

#endif  // ATHENA_HOME_APP_LIST_VIEW_DELEGATE_H_
