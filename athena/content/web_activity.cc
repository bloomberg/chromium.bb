// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/web_activity.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/activity/public/activity_view.h"
#include "athena/content/content_proxy.h"
#include "athena/content/media_utils.h"
#include "athena/content/public/dialogs.h"
#include "athena/content/web_activity_helpers.h"
#include "athena/input/public/accelerator_manager.h"
#include "athena/strings/grit/athena_strings.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "components/app_modal/javascript_dialog_manager.h"
#include "components/favicon_base/select_favicon_frames.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/favicon_url.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/widget.h"

namespace athena {
namespace {

class WebActivityController : public AcceleratorHandler {
 public:
  enum Command {
    CMD_BACK,
    CMD_FORWARD,
    CMD_RELOAD,
    CMD_RELOAD_IGNORE_CACHE,
    CMD_CLOSE,
    CMD_STOP,
  };

  explicit WebActivityController(WebActivity* owner_activity,
                                 views::WebView* web_view)
      : owner_activity_(owner_activity),
        web_view_(web_view),
        reserved_accelerator_enabled_(true) {}
  ~WebActivityController() override {}

  // Installs accelerators for web activity.
  void InstallAccelerators() {
    accelerator_manager_ = AcceleratorManager::CreateForFocusManager(
                               web_view_->GetFocusManager()).Pass();
    const AcceleratorData accelerator_data[] = {
        {TRIGGER_ON_PRESS, ui::VKEY_R, ui::EF_CONTROL_DOWN, CMD_RELOAD,
         AF_NONE},
        {TRIGGER_ON_PRESS, ui::VKEY_BROWSER_REFRESH, ui::EF_NONE, CMD_RELOAD,
         AF_NONE},
        {TRIGGER_ON_PRESS, ui::VKEY_BROWSER_REFRESH, ui::EF_CONTROL_DOWN,
         CMD_RELOAD_IGNORE_CACHE, AF_NONE},
        {TRIGGER_ON_PRESS, ui::VKEY_BROWSER_FORWARD, ui::EF_NONE, CMD_FORWARD,
         AF_NONE},
        {TRIGGER_ON_PRESS, ui::VKEY_BROWSER_BACK, ui::EF_NONE, CMD_BACK,
         AF_NONE},
        {TRIGGER_ON_PRESS, ui::VKEY_W, ui::EF_CONTROL_DOWN, CMD_CLOSE, AF_NONE},
        {TRIGGER_ON_PRESS, ui::VKEY_ESCAPE, ui::EF_NONE, CMD_STOP, AF_NONE},
    };
    accelerator_manager_->RegisterAccelerators(
        accelerator_data, arraysize(accelerator_data), this);
  }

  // Methods that are called before and after key events are consumed by the web
  // contents.
  // See the documentation in WebContentsDelegate: for more details.
  bool PreHandleKeyboardEvent(content::WebContents* source,
                              const content::NativeWebKeyboardEvent& event,
                              bool* is_keyboard_shortcut) {
    ui::Accelerator accelerator =
        ui::GetAcceleratorFromNativeWebKeyboardEvent(event);

    if (reserved_accelerator_enabled_ &&
        accelerator_manager_->IsRegistered(accelerator, AF_RESERVED)) {
      return web_view_->GetFocusManager()->ProcessAccelerator(accelerator);
    }
    *is_keyboard_shortcut =
        accelerator_manager_->IsRegistered(accelerator, AF_NONE);
    return false;
  }

  void HandleKeyboardEvent(content::WebContents* source,
                           const content::NativeWebKeyboardEvent& event) {
    unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, web_view_->GetFocusManager());
  }

 private:
  // AcceleratorHandler:
  bool IsCommandEnabled(int command_id) const override {
    switch (command_id) {
      case CMD_RELOAD:
      case CMD_RELOAD_IGNORE_CACHE:
        return true;
      case CMD_BACK:
        return web_view_->GetWebContents()->GetController().CanGoBack();
      case CMD_FORWARD:
        return web_view_->GetWebContents()->GetController().CanGoForward();
      case CMD_CLOSE:
        // TODO(oshima): check onbeforeunload handler.
        return true;
      case CMD_STOP:
        return web_view_->GetWebContents()->IsLoading();
    }
    return false;
  }

  bool OnAcceleratorFired(int command_id,
                          const ui::Accelerator& accelerator) override {
    switch (command_id) {
      case CMD_RELOAD:
        web_view_->GetWebContents()->GetController().Reload(false);
        return true;
      case CMD_RELOAD_IGNORE_CACHE:
        web_view_->GetWebContents()->GetController().ReloadIgnoringCache(false);
        return true;
      case CMD_BACK:
        web_view_->GetWebContents()->GetController().GoBack();
        return true;
      case CMD_FORWARD:
        web_view_->GetWebContents()->GetController().GoForward();
        return true;
      case CMD_CLOSE:
        Activity::Delete(owner_activity_);
        return true;
      case CMD_STOP:
        web_view_->GetWebContents()->Stop();
        return true;
    }
    return false;
  }

  Activity* const owner_activity_;
  views::WebView* web_view_;
  bool reserved_accelerator_enabled_;
  scoped_ptr<AcceleratorManager> accelerator_manager_;
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;

  DISALLOW_COPY_AND_ASSIGN(WebActivityController);
};

const SkColor kDefaultTitleColor = SkColorSetRGB(0xf2, 0xf2, 0xf2);
const int kIconSize = 32;
const float kDistanceShowReloadMessage = 100;
const float kDistanceReload = 150;

}  // namespace

// A web view for athena's web activity. Note that AthenaWebView will create its
// own content so that it can eject and reload it.
class AthenaWebView : public views::WebView {
 public:
  explicit AthenaWebView(content::BrowserContext* context,
                         WebActivity* owner_activity)
      : views::WebView(context),
        controller_(new WebActivityController(owner_activity, this)),
        owner_activity_(owner_activity),
        fullscreen_(false),
        overscroll_y_(0) {
    SetEmbedFullscreenWidgetMode(true);
    // TODO(skuhne): Add content observer to detect renderer crash and set
    // content status to unloaded if that happens.
  }

  AthenaWebView(content::WebContents* web_contents,
                WebActivity* owner_activity)
      : views::WebView(web_contents->GetBrowserContext()),
        controller_(new WebActivityController(owner_activity, this)),
        owner_activity_(owner_activity) {
    scoped_ptr<content::WebContents> old_contents(
        SwapWebContents(scoped_ptr<content::WebContents>(web_contents)));
  }

  ~AthenaWebView() override {}

  void InstallAccelerators() { controller_->InstallAccelerators(); }

  void EvictContent() {
    scoped_ptr<content::WebContents> old_contents(SwapWebContents(
        scoped_ptr<content::WebContents>(content::WebContents::Create(
            content::WebContents::CreateParams(browser_context())))));
    // If there is a progress bar, we need to get rid of it now since its
    // associated content, parent window and layers will disappear with evicting
    // the content.
    progress_bar_.reset();
    evicted_web_contents_.reset(
        content::WebContents::Create(content::WebContents::CreateParams(
            old_contents->GetBrowserContext())));
    evicted_web_contents_->GetController().CopyStateFrom(
        old_contents->GetController());
    // As soon as the new contents becomes visible, it should reload.
    // TODO(skuhne): This breaks script connections with other activities.
    // Even though this is the same technique as used by the TabStripModel,
    // we might want to address this cleaner since we are more likely to
    // run into this state. by unloading.
  }

  void AttachHelpers() {
    if (!IsContentEvicted())
      AttachWebActivityHelpers(GetWebContents());
    // Else: The helpers will be attached when the evicted content is reloaded.
  }

  void ReloadEvictedContent() {
    CHECK(evicted_web_contents_.get());

    // Order is important. The helpers must be attached prior to the RenderView
    // being created.
    AttachWebActivityHelpers(evicted_web_contents_.get());

    SwapWebContents(evicted_web_contents_.Pass());
  }

  // Check if the content got evicted.
  const bool IsContentEvicted() { return !!evicted_web_contents_.get(); }

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) override {
    switch(params.disposition) {
      case CURRENT_TAB: {
        DCHECK(source == web_contents());
        content::NavigationController::LoadURLParams load_url_params(
            params.url);
        load_url_params.referrer = params.referrer;
        load_url_params.frame_tree_node_id = params.frame_tree_node_id;
        load_url_params.transition_type = params.transition;
        load_url_params.extra_headers = params.extra_headers;
        load_url_params.should_replace_current_entry =
            params.should_replace_current_entry;
        load_url_params.is_renderer_initiated = params.is_renderer_initiated;
        load_url_params.transferred_global_request_id =
            params.transferred_global_request_id;
        web_contents()->GetController().LoadURLWithParams(load_url_params);
        return web_contents();
      }
      case NEW_FOREGROUND_TAB:
      case NEW_BACKGROUND_TAB:
      case NEW_POPUP:
      case NEW_WINDOW: {
        Activity* activity = ActivityFactory::Get()->CreateWebActivity(
            browser_context(), base::string16(), params.url);
        Activity::Show(activity);
        break;
      }
      default:
        break;
    }
    // nullptr is returned if the URL wasn't opened immediately.
    return nullptr;
  }

  bool CanOverscrollContent() const override {
    const std::string value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kOverscrollHistoryNavigation);
    return value != "0";
  }

  void OverscrollUpdate(float delta_y) override {
    overscroll_y_ = delta_y;
    if (overscroll_y_ > kDistanceShowReloadMessage) {
      if (!reload_message_)
        CreateReloadMessage();
      reload_message_->Show();
      float opacity = 1.0f;
      if (overscroll_y_ < kDistanceReload) {
        opacity = (overscroll_y_ - kDistanceShowReloadMessage) /
            (kDistanceReload - kDistanceShowReloadMessage);
      }
      reload_message_->GetLayer()->SetOpacity(opacity);
    } else if (reload_message_) {
      reload_message_->Hide();
    }
  }

  void OverscrollComplete() override {
    if (overscroll_y_ >= kDistanceReload)
      GetWebContents()->GetController().Reload(false);
    if (reload_message_)
      reload_message_->Hide();
    overscroll_y_ = 0;
  }

  void AddNewContents(content::WebContents* source,
                      content::WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_pos,
                      bool user_gesture,
                      bool* was_blocked) override {
    Activity* activity =
        ActivityFactory::Get()->CreateWebActivity(new_contents);
    Activity::Show(activity);
  }

  bool PreHandleKeyboardEvent(content::WebContents* source,
                              const content::NativeWebKeyboardEvent& event,
                              bool* is_keyboard_shortcut) override {
    return controller_->PreHandleKeyboardEvent(
        source, event, is_keyboard_shortcut);
  }

  void HandleKeyboardEvent(
      content::WebContents* source,
      const content::NativeWebKeyboardEvent& event) override {
    controller_->HandleKeyboardEvent(source, event);
  }

  void ToggleFullscreenModeForTab(content::WebContents* web_contents,
                                  bool enter_fullscreen) override {
    fullscreen_ = enter_fullscreen;
    GetWidget()->SetFullscreen(fullscreen_);
  }

  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) const override {
    return fullscreen_;
  }

  void LoadingStateChanged(content::WebContents* source,
                           bool to_different_document) override {
    bool has_stopped = source == nullptr || !source->IsLoading();
    LoadProgressChanged(source, has_stopped ? 1 : 0);
  }

  void LoadProgressChanged(content::WebContents* source,
                           double progress) override {
    if (!progress)
      return;

    if (!progress_bar_) {
      CreateProgressBar();
      source->GetNativeView()->layer()->Add(progress_bar_.get());
    }
    progress_bar_->SetBounds(gfx::Rect(
        0, 0, progress * progress_bar_->parent()->bounds().width(), 3));
    if (progress < 1)
      return;

    ui::ScopedLayerAnimationSettings settings(progress_bar_->GetAnimator());
    settings.SetTweenType(gfx::Tween::EASE_IN);
    ui::Layer* layer = progress_bar_.get();
    settings.AddObserver(new ui::ClosureAnimationObserver(
        base::Bind(&base::DeletePointer<ui::Layer>, progress_bar_.release())));
    layer->SetOpacity(0.f);
  }

  content::JavaScriptDialogManager* GetJavaScriptDialogManager(
      content::WebContents* contents) override {
    return app_modal::JavaScriptDialogManager::GetInstance();
  }

  content::ColorChooser* OpenColorChooser(
      content::WebContents* web_contents,
      SkColor color,
      const std::vector<content::ColorSuggestion>& suggestions) override {
    return athena::OpenColorChooser(web_contents, color, suggestions);
  }

  // Called when a file selection is to be done.
  void RunFileChooser(content::WebContents* web_contents,
                      const content::FileChooserParams& params) override {
    return athena::OpenFileChooser(web_contents, params);
  }

  void CloseContents(content::WebContents* contents) override {
    Activity::Delete(owner_activity_);
  }

 private:
  void CreateProgressBar() {
    CHECK(!progress_bar_);
    progress_bar_.reset(new ui::Layer(ui::LAYER_SOLID_COLOR));
    progress_bar_->SetColor(SkColorSetRGB(0x17, 0x59, 0xcd));
  }

  void CreateReloadMessage() {
    CHECK(!reload_message_);
    reload_message_.reset(new views::Widget);
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = GetWidget()->GetNativeView();
    reload_message_->Init(params);

    views::Label* label = new views::Label(
        l10n_util::GetStringUTF16(IDS_ATHENA_PULL_TO_RELOAD_MESSAGE));
    label->SetBackgroundColor(SK_ColorGRAY);
    label->set_background(
        views::Background::CreateSolidBackground(SK_ColorGRAY));

    reload_message_->SetContentsView(label);
    reload_message_->SetBounds(ConvertRectToWidget(
        gfx::Rect(0, 0, width(), label->GetPreferredSize().height())));
  }

  scoped_ptr<WebActivityController> controller_;

  Activity* const owner_activity_;

  // If the activity got evicted, this is the web content which holds the known
  // state of the content before eviction.
  scoped_ptr<content::WebContents> evicted_web_contents_;

  scoped_ptr<ui::Layer> progress_bar_;

  scoped_ptr<views::Widget> reload_message_;

  // TODO(oshima): Find out if we should support window fullscreen.
  // It may still useful when a user is in split mode.
  bool fullscreen_;

  // The distance that the user has overscrolled vertically.
  float overscroll_y_;

  DISALLOW_COPY_AND_ASSIGN(AthenaWebView);
};

WebActivity::WebActivity(content::BrowserContext* browser_context,
                         const base::string16& title,
                         const GURL& url)
    : browser_context_(browser_context),
      web_view_(new AthenaWebView(browser_context, this)),
      title_(title),
      title_color_(kDefaultTitleColor),
      current_state_(ACTIVITY_UNLOADED),
      activity_view_(nullptr),
      weak_ptr_factory_(this) {
  // Order is important. The web activity helpers must be attached prior to the
  // RenderView being created.
  SetCurrentState(ACTIVITY_INVISIBLE);
  web_view_->LoadInitialURL(url);
}

WebActivity::WebActivity(content::WebContents* contents)
    : browser_context_(contents->GetBrowserContext()),
      web_view_(new AthenaWebView(contents, this)),
      title_color_(kDefaultTitleColor),
      current_state_(ACTIVITY_UNLOADED),
      activity_view_(nullptr),
      weak_ptr_factory_(this) {
  // If the activity was created as a result of
  // WebContentsDelegate::AddNewContents(), web activity helpers may not be
  // created prior to the RenderView being created. Desktop Chrome has a
  // similar problem.
  SetCurrentState(ACTIVITY_INVISIBLE);
}

WebActivity::~WebActivity() {
  // It is not required to change the activity state to UNLOADED - unless we
  // would add state observers.
}

ActivityViewModel* WebActivity::GetActivityViewModel() {
  return this;
}

void WebActivity::SetCurrentState(Activity::ActivityState state) {
  DCHECK_NE(state, current_state_);
  if (current_state_ == ACTIVITY_UNLOADED) {
    web_view_->AttachHelpers();
    if (web_view_->IsContentEvicted())
      web_view_->ReloadEvictedContent();
    Observe(web_view_->GetWebContents());
  }

  switch (state) {
    case ACTIVITY_VISIBLE:
      HideContentProxy();
      break;
    case ACTIVITY_INVISIBLE:
      if (current_state_ == ACTIVITY_VISIBLE)
        ShowContentProxy();

      break;
    case ACTIVITY_BACKGROUND_LOW_PRIORITY:
      DCHECK(ACTIVITY_VISIBLE == current_state_ ||
             ACTIVITY_INVISIBLE == current_state_);
      // TODO(skuhne): Do this.
      break;
    case ACTIVITY_PERSISTENT:
      DCHECK_EQ(ACTIVITY_BACKGROUND_LOW_PRIORITY, current_state_);
      // TODO(skuhne): Do this. As soon as the new resource management is
      // agreed upon - or remove otherwise.
      break;
    case ACTIVITY_UNLOADED:
      DCHECK_NE(ACTIVITY_UNLOADED, current_state_);
      if (content_proxy_)
        content_proxy_->ContentWillUnload();
      Observe(nullptr);
      web_view_->EvictContent();
      break;
  }
  // Remember the last requested state.
  current_state_ = state;
}

Activity::ActivityState WebActivity::GetCurrentState() {
  // If the content is evicted, the state has to be UNLOADED.
  DCHECK(!web_view_->IsContentEvicted() ||
         current_state_ == ACTIVITY_UNLOADED);
  return current_state_;
}

bool WebActivity::IsVisible() {
  return web_view_->visible() && current_state_ != ACTIVITY_UNLOADED;
}

Activity::ActivityMediaState WebActivity::GetMediaState() {
  return current_state_ == ACTIVITY_UNLOADED ?
      Activity::ACTIVITY_MEDIA_STATE_NONE :
      GetActivityMediaState(GetWebContents());
}

aura::Window* WebActivity::GetWindow() {
  return web_view_->GetWidget() ? web_view_->GetWidget()->GetNativeWindow()
                                : nullptr;
}

content::WebContents* WebActivity::GetWebContents() {
  return web_view_->GetWebContents();
}

void WebActivity::Init() {
  web_view_->InstallAccelerators();
}

SkColor WebActivity::GetRepresentativeColor() const {
  return title_color_;
}

base::string16 WebActivity::GetTitle() const {
  if (!title_.empty())
    return title_;
  const base::string16& title = web_view_->GetWebContents()->GetTitle();
  if (!title.empty())
    return title;
  return base::UTF8ToUTF16(web_view_->GetWebContents()->GetVisibleURL().host());
}

gfx::ImageSkia WebActivity::GetIcon() const {
  return icon_;
}

void WebActivity::SetActivityView(ActivityView* view) {
  DCHECK(!activity_view_);
  activity_view_ = view;
}

bool WebActivity::UsesFrame() const {
  return true;
}

views::View* WebActivity::GetContentsView() {
  return web_view_;
}

gfx::ImageSkia WebActivity::GetOverviewModeImage() {
  if (content_proxy_.get())
    return content_proxy_->GetContentImage();
  return gfx::ImageSkia();
}

void WebActivity::PrepareContentsForOverview() {
  // Turn on fast resizing to avoid re-laying out the web contents when
  // entering / exiting overview mode and the content is visible.
  if (!content_proxy_.get())
    web_view_->SetFastResize(true);
}

void WebActivity::ResetContentsView() {
  // Turn on fast resizing to avoid re-laying out the web contents when
  // entering / exiting overview mode and the content is visible.
  if (!content_proxy_.get()) {
    web_view_->SetFastResize(false);
    web_view_->Layout();
  }
}

void WebActivity::TitleWasSet(content::NavigationEntry* entry,
                              bool explicit_set) {
  if (activity_view_)
    activity_view_->UpdateTitle();
}

void WebActivity::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Prevent old image requests from calling back to OnDidDownloadFavicon().
  weak_ptr_factory_.InvalidateWeakPtrs();

  icon_ = gfx::ImageSkia();
  if (activity_view_)
    activity_view_->UpdateIcon();
}

void WebActivity::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  // Pick an arbitrary favicon of type FAVICON to use.
  // TODO(pkotwicz): Do something better once the favicon code is componentized.
  // (crbug.com/401997)
  weak_ptr_factory_.InvalidateWeakPtrs();
  for (size_t i = 0; i < candidates.size(); ++i) {
    if (candidates[i].icon_type == content::FaviconURL::FAVICON) {
      web_view_->GetWebContents()->DownloadImage(
          candidates[i].icon_url,
          true,
          0,
          base::Bind(&WebActivity::OnDidDownloadFavicon,
                     weak_ptr_factory_.GetWeakPtr()));
      break;
    }
  }
}

void WebActivity::OnDidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& original_bitmap_sizes) {
  icon_ = CreateFaviconImageSkia(
      bitmaps, original_bitmap_sizes, kIconSize, nullptr);
  if (activity_view_)
    activity_view_->UpdateIcon();
}

void WebActivity::DidChangeThemeColor(SkColor theme_color) {
  title_color_ = theme_color;
  if (activity_view_)
    activity_view_->UpdateRepresentativeColor();
}

void WebActivity::HideContentProxy() {
  if (content_proxy_.get())
    content_proxy_.reset(nullptr);
}

void WebActivity::ShowContentProxy() {
  if (!content_proxy_.get())
    content_proxy_.reset(new ContentProxy(web_view_));
}

}  // namespace athena
