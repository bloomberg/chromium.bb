// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/toplevel_window.h"
#include "ash/shell_delegate.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_item_list.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_result.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/examples/examples_window_with_content.h"

namespace ash {
namespace shell {

namespace {

// WindowTypeShelfItem is an app item of app list. It carries a window
// launch type and launches corresponding example window when activated.
class WindowTypeShelfItem : public app_list::AppListItem {
 public:
  enum Type {
    TOPLEVEL_WINDOW = 0,
    NON_RESIZABLE_WINDOW,
    LOCK_SCREEN,
    WIDGETS_WINDOW,
    EXAMPLES_WINDOW,
    LAST_TYPE,
  };

  explicit WindowTypeShelfItem(const std::string& id, Type type)
      : app_list::AppListItem(id),
        type_(type) {
    std::string title(GetTitle(type));
    SetIcon(GetIcon(type), false);
    SetName(title);
  }

  static gfx::ImageSkia GetIcon(Type type) {
    static const SkColor kColors[] = {
        SK_ColorRED,
        SK_ColorGREEN,
        SK_ColorBLUE,
        SK_ColorYELLOW,
        SK_ColorCYAN,
    };

    const int kIconSize = 128;
    SkBitmap icon;
    icon.allocN32Pixels(kIconSize, kIconSize);
    icon.eraseColor(kColors[static_cast<int>(type) % arraysize(kColors)]);
    return gfx::ImageSkia::CreateFrom1xBitmap(icon);
  }

  // The text below is not localized as this is an example code.
  static std::string GetTitle(Type type) {
    switch (type) {
      case TOPLEVEL_WINDOW:
        return "Create Window";
      case NON_RESIZABLE_WINDOW:
        return "Create Non-Resizable Window";
      case LOCK_SCREEN:
        return "Lock Screen";
      case WIDGETS_WINDOW:
        return "Show Example Widgets";
      case EXAMPLES_WINDOW:
        return "Open Views Examples Window";
      default:
        return "Unknown window type.";
    }
  }

  // The text below is not localized as this is an example code.
  static std::string GetDetails(Type type) {
    // Assigns details only to some types so that we see both one-line
    // and two-line results.
    switch (type) {
      case WIDGETS_WINDOW:
        return "Creates a window to show example widgets";
      case EXAMPLES_WINDOW:
        return "Creates a window to show views example.";
      default:
        return std::string();
    }
  }

  static void ActivateItem(Type type, int event_flags) {
     switch (type) {
      case TOPLEVEL_WINDOW: {
        ToplevelWindow::CreateParams params;
        params.can_resize = true;
        ToplevelWindow::CreateToplevelWindow(params);
        break;
      }
      case NON_RESIZABLE_WINDOW: {
        ToplevelWindow::CreateToplevelWindow(ToplevelWindow::CreateParams());
        break;
      }
      case LOCK_SCREEN: {
        Shell::GetInstance()->session_state_delegate()->LockScreen();
        break;
      }
      case WIDGETS_WINDOW: {
        CreateWidgetsWindow();
        break;
      }
      case EXAMPLES_WINDOW: {
        views::examples::ShowExamplesWindowWithContent(
            views::examples::DO_NOTHING_ON_CLOSE,
            Shell::GetInstance()->delegate()->GetActiveBrowserContext(),
            NULL);
        break;
      }
      default:
        break;
    }
  }

  // AppListItem
  virtual void Activate(int event_flags) OVERRIDE {
    ActivateItem(type_, event_flags);
  }

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeShelfItem);
};

// ExampleSearchResult is an app list search result. It provides what icon to
// show, what should title and details text look like. It also carries the
// matching window launch type so that AppListViewDelegate knows how to open
// it.
class ExampleSearchResult : public app_list::SearchResult {
 public:
  ExampleSearchResult(WindowTypeShelfItem::Type type,
                      const base::string16& query)
      : type_(type) {
    SetIcon(WindowTypeShelfItem::GetIcon(type_));

    base::string16 title =
        base::UTF8ToUTF16(WindowTypeShelfItem::GetTitle(type_));
    set_title(title);

    Tags title_tags;
    const size_t match_len = query.length();

    // Highlight matching parts in title with bold.
    // Note the following is not a proper way to handle i18n string.
    title = base::i18n::ToLower(title);
    size_t match_start = title.find(query);
    while (match_start != base::string16::npos) {
      title_tags.push_back(Tag(Tag::MATCH,
                               match_start,
                               match_start + match_len));
      match_start = title.find(query, match_start + match_len);
    }
    set_title_tags(title_tags);

    base::string16 details =
        base::UTF8ToUTF16(WindowTypeShelfItem::GetDetails(type_));
    set_details(details);
    Tags details_tags;
    details_tags.push_back(Tag(Tag::DIM, 0, details.length()));
    set_details_tags(details_tags);
  }

  WindowTypeShelfItem::Type type() const { return type_; }

 private:
  WindowTypeShelfItem::Type type_;

  DISALLOW_COPY_AND_ASSIGN(ExampleSearchResult);
};

class ExampleAppListViewDelegate : public app_list::AppListViewDelegate {
 public:
  ExampleAppListViewDelegate()
      : model_(new app_list::AppListModel) {
    PopulateApps();
    DecorateSearchBox(model_->search_box());
  }

 private:
  void PopulateApps() {
    for (int i = 0; i < static_cast<int>(WindowTypeShelfItem::LAST_TYPE); ++i) {
      WindowTypeShelfItem::Type type =
          static_cast<WindowTypeShelfItem::Type>(i);
      std::string id = base::StringPrintf("%d", i);
      scoped_ptr<WindowTypeShelfItem> shelf_item(
          new WindowTypeShelfItem(id, type));
      model_->AddItem(shelf_item.PassAs<app_list::AppListItem>());
    }
  }

  gfx::ImageSkia CreateSearchBoxIcon() {
    const base::string16 icon_text = base::ASCIIToUTF16("ash");
    const gfx::Size icon_size(32, 32);

    gfx::Canvas canvas(icon_size, 1.0f, false /* is_opaque */);
    canvas.DrawStringRectWithFlags(
        icon_text,
        gfx::FontList(),
        SK_ColorBLACK,
        gfx::Rect(icon_size),
        gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_SUBPIXEL_RENDERING);

    return gfx::ImageSkia(canvas.ExtractImageRep());
  }

  void DecorateSearchBox(app_list::SearchBoxModel* search_box_model) {
    search_box_model->SetIcon(CreateSearchBoxIcon());
    search_box_model->SetHintText(base::ASCIIToUTF16("Type to search..."));
  }

  // Overridden from app_list::AppListViewDelegate:
  virtual bool ForceNativeDesktop() const OVERRIDE {
    return false;
  }

  virtual void SetProfileByPath(const base::FilePath& profile_path) OVERRIDE {
    // Nothing needs to be done.
  }

  virtual const Users& GetUsers() const OVERRIDE {
    return users_;
  }

  virtual bool ShouldCenterWindow() const OVERRIDE {
    return false;
  }

  virtual app_list::AppListModel* GetModel() OVERRIDE { return model_.get(); }

  virtual app_list::SpeechUIModel* GetSpeechUI() OVERRIDE {
    return &speech_ui_;
  }

  virtual void GetShortcutPathForApp(
      const std::string& app_id,
      const base::Callback<void(const base::FilePath&)>& callback) OVERRIDE {
    callback.Run(base::FilePath());
  }

  virtual void OpenSearchResult(app_list::SearchResult* result,
                                bool auto_launch,
                                int event_flags) OVERRIDE {
    const ExampleSearchResult* example_result =
        static_cast<const ExampleSearchResult*>(result);
    WindowTypeShelfItem::ActivateItem(example_result->type(), event_flags);
  }

  virtual void InvokeSearchResultAction(app_list::SearchResult* result,
                                        int action_index,
                                        int event_flags) OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual base::TimeDelta GetAutoLaunchTimeout() OVERRIDE {
    return base::TimeDelta();
  }

  virtual void AutoLaunchCanceled() OVERRIDE {
  }

  virtual void StartSearch() OVERRIDE {
    base::string16 query;
    base::TrimWhitespace(model_->search_box()->text(), base::TRIM_ALL, &query);
    query = base::i18n::ToLower(query);

    model_->results()->DeleteAll();
    if (query.empty())
      return;

    for (int i = 0; i < static_cast<int>(WindowTypeShelfItem::LAST_TYPE); ++i) {
      WindowTypeShelfItem::Type type =
          static_cast<WindowTypeShelfItem::Type>(i);

      base::string16 title =
          base::UTF8ToUTF16(WindowTypeShelfItem::GetTitle(type));
      if (base::i18n::StringSearchIgnoringCaseAndAccents(
              query, title, NULL, NULL)) {
        model_->results()->Add(new ExampleSearchResult(type, query));
      }
    }
  }

  virtual void StopSearch() OVERRIDE {
    // Nothing needs to be done.
  }

  virtual void ViewInitialized() OVERRIDE {
    // Nothing needs to be done.
  }

  virtual void Dismiss() OVERRIDE {
    DCHECK(ash::Shell::HasInstance());
    Shell::GetInstance()->DismissAppList();
  }

  virtual void ViewClosing() OVERRIDE {
    // Nothing needs to be done.
  }

  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE {
    return gfx::ImageSkia();
  }

  virtual void OpenSettings() OVERRIDE {
    // Nothing needs to be done.
  }

  virtual void OpenHelp() OVERRIDE {
    // Nothing needs to be done.
  }

  virtual void OpenFeedback() OVERRIDE {
    // Nothing needs to be done.
  }

  virtual void ToggleSpeechRecognition() OVERRIDE {
    NOTIMPLEMENTED();
  }

  virtual void ShowForProfileByPath(
      const base::FilePath& profile_path) OVERRIDE {
    // Nothing needs to be done.
  }

  virtual views::View* CreateStartPageWebView(const gfx::Size& size) OVERRIDE {
    return NULL;
  }

  virtual std::vector<views::View*> CreateCustomPageWebViews(
      const gfx::Size& size) OVERRIDE {
    return std::vector<views::View*>();
  }

  virtual bool IsSpeechRecognitionEnabled() OVERRIDE {
    return false;
  }

  scoped_ptr<app_list::AppListModel> model_;
  app_list::SpeechUIModel speech_ui_;
  Users users_;

  DISALLOW_COPY_AND_ASSIGN(ExampleAppListViewDelegate);
};

}  // namespace

app_list::AppListViewDelegate* CreateAppListViewDelegate() {
  return new ExampleAppListViewDelegate;
}

}  // namespace shell
}  // namespace ash
