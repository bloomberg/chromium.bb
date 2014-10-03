// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_HOME_APP_LIST_VIEW_DELEGATE_H_
#define ATHENA_HOME_APP_LIST_VIEW_DELEGATE_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "ui/app_list/app_list_view_delegate.h"

namespace app_list {
class SearchProvider;
}

namespace athena {
class AppModelBuilder;

class AppListViewDelegate : public app_list::AppListViewDelegate {
 public:
  explicit AppListViewDelegate(AppModelBuilder* model_builder);
  virtual ~AppListViewDelegate();

  void RegisterSearchProvider(app_list::SearchProvider* search_provider);

 private:
  void SearchResultChanged();

  // Overridden from app_list::AppListViewDelegate:
  virtual bool ForceNativeDesktop() const override;
  virtual void SetProfileByPath(const base::FilePath& profile_path) override;
  virtual app_list::AppListModel* GetModel() override;
  virtual app_list::SpeechUIModel* GetSpeechUI() override;
  virtual void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) override;
  virtual void StartSearch() override;
  virtual void StopSearch() override;
  virtual void OpenSearchResult(app_list::SearchResult* result,
                                bool auto_launch,
                                int event_flags) override;
  virtual void InvokeSearchResultAction(app_list::SearchResult* result,
                                        int action_index,
                                        int event_flags) override;
  virtual base::TimeDelta GetAutoLaunchTimeout() override;
  virtual void AutoLaunchCanceled() override;
  virtual void ViewInitialized() override;
  virtual void Dismiss() override;
  virtual void ViewClosing() override;
  virtual gfx::ImageSkia GetWindowIcon() override;
  virtual void OpenSettings() override;
  virtual void OpenHelp() override;
  virtual void OpenFeedback() override;
  virtual void ToggleSpeechRecognition() override;
  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) override;
  virtual views::View* CreateStartPageWebView(const gfx::Size& size) override;
  virtual std::vector<views::View*> CreateCustomPageWebViews(
      const gfx::Size& size) override;
  virtual bool IsSpeechRecognitionEnabled() override;
  virtual const Users& GetUsers() const override;
  virtual bool ShouldCenterWindow() const override;

  scoped_ptr<app_list::AppListModel> model_;
  scoped_ptr<app_list::SpeechUIModel> speech_ui_;
  Users users_;

  std::vector<app_list::SearchProvider*> search_providers_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegate);
};

}  // namespace athena

#endif  // ATHENA_HOME_APP_LIST_VIEW_DELEGATE_H_
