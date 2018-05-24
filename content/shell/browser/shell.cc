// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell.h"

#include <stddef.h>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/renderer_preferences.h"
#include "content/public/common/webrtc_ip_handling_policy.h"
#include "content/shell/browser/layout_test/blink_test_controller.h"
#include "content/shell/browser/layout_test/layout_test_bluetooth_chooser_factory.h"
#include "content/shell/browser/layout_test/layout_test_devtools_bindings.h"
#include "content/shell/browser/layout_test/layout_test_javascript_dialog_manager.h"
#include "content/shell/browser/layout_test/secondary_test_window_observer.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_frontend.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "content/shell/common/layout_test/layout_test_switches.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/web/web_presentation_receiver_flags.h"

namespace content {

const int kDefaultTestWindowWidthDip = 800;
const int kDefaultTestWindowHeightDip = 600;

std::vector<Shell*> Shell::windows_;
base::Callback<void(Shell*)> Shell::shell_created_callback_;

bool Shell::quit_message_loop_ = true;

class Shell::DevToolsWebContentsObserver : public WebContentsObserver {
 public:
  DevToolsWebContentsObserver(Shell* shell, WebContents* web_contents)
      : WebContentsObserver(web_contents),
        shell_(shell) {
  }

  // WebContentsObserver
  void WebContentsDestroyed() override {
    shell_->OnDevToolsWebContentsDestroyed();
  }

 private:
  Shell* shell_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsWebContentsObserver);
};

Shell::Shell(std::unique_ptr<WebContents> web_contents)
    : WebContentsObserver(web_contents.get()),
      web_contents_(std::move(web_contents)),
      devtools_frontend_(nullptr),
      is_fullscreen_(false),
      window_(nullptr),
#if defined(OS_MACOSX)
      url_edit_view_(NULL),
#endif
      headless_(false),
      hide_toolbar_(false) {
  web_contents_->SetDelegate(this);

  if (switches::IsRunWebTestsSwitchPresent()) {
    headless_ = true;
    // In a headless shell, disable occlusion tracking. Otherwise, WebContents
    // would always behave as if they were occluded, i.e. would not render
    // frames and would not receive input events.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableBackgroundingOccludedWindowsForTesting);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kContentShellHideToolbar))
    hide_toolbar_ = true;

  windows_.push_back(this);

  if (!shell_created_callback_.is_null()) {
    shell_created_callback_.Run(this);
    shell_created_callback_.Reset();
  }
}

Shell::~Shell() {
  PlatformCleanUp();

  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i] == this) {
      windows_.erase(windows_.begin() + i);
      break;
    }
  }

  if (windows_.empty() && quit_message_loop_) {
    if (headless_)
      PlatformExit();
    for (auto it = RenderProcessHost::AllHostsIterator(); !it.IsAtEnd();
         it.Advance()) {
      it.GetCurrentValue()->DisableKeepAliveRefCount();
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
  }

  web_contents_->SetDelegate(nullptr);
}

Shell* Shell::CreateShell(std::unique_ptr<WebContents> web_contents,
                          const gfx::Size& initial_size) {
  WebContents* raw_web_contents = web_contents.get();
  Shell* shell = new Shell(std::move(web_contents));
  shell->PlatformCreateWindow(initial_size.width(), initial_size.height());

  shell->PlatformSetContents();

  shell->PlatformResizeSubViews();

  // Note: Do not make RenderFrameHost or RenderViewHost specific state changes
  // here, because they will be forgotten after a cross-process navigation. Use
  // RenderFrameCreated or RenderViewCreated instead.
  if (switches::IsRunWebTestsSwitchPresent()) {
    raw_web_contents->GetMutableRendererPrefs()->use_custom_colors = false;
    raw_web_contents->GetRenderViewHost()->SyncRendererPrefs();
  }

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceWebRtcIPHandlingPolicy)) {
    raw_web_contents->GetMutableRendererPrefs()->webrtc_ip_handling_policy =
        command_line->GetSwitchValueASCII(
            switches::kForceWebRtcIPHandlingPolicy);
  }

  return shell;
}

void Shell::CloseAllWindows() {
  base::AutoReset<bool> auto_reset(&quit_message_loop_, false);
  DevToolsAgentHost::DetachAllClients();
  std::vector<Shell*> open_windows(windows_);
  for (size_t i = 0; i < open_windows.size(); ++i)
    open_windows[i]->Close();
  base::RunLoop().RunUntilIdle();
  PlatformExit();
}

void Shell::SetShellCreatedCallback(
    base::Callback<void(Shell*)> shell_created_callback) {
  DCHECK(shell_created_callback_.is_null());
  shell_created_callback_ = std::move(shell_created_callback);
}

Shell* Shell::FromRenderViewHost(RenderViewHost* rvh) {
  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i]->web_contents() &&
        windows_[i]->web_contents()->GetRenderViewHost() == rvh) {
      return windows_[i];
    }
  }
  return nullptr;
}

// static
void Shell::Initialize() {
  PlatformInitialize(GetShellDefaultSize());
}

gfx::Size Shell::AdjustWindowSize(const gfx::Size& initial_size) {
  if (!initial_size.IsEmpty())
    return initial_size;
  return GetShellDefaultSize();
}

Shell* Shell::CreateNewWindow(BrowserContext* browser_context,
                              const GURL& url,
                              const scoped_refptr<SiteInstance>& site_instance,
                              const gfx::Size& initial_size) {
  WebContents::CreateParams create_params(browser_context, site_instance);
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForcePresentationReceiverForTesting)) {
    create_params.starting_sandbox_flags =
        blink::kPresentationReceiverSandboxFlags;
  }
  create_params.initial_size = AdjustWindowSize(initial_size);
  std::unique_ptr<WebContents> web_contents =
      WebContents::Create(create_params);
  Shell* shell =
      CreateShell(std::move(web_contents), create_params.initial_size);
  if (!url.is_empty())
    shell->LoadURL(url);
  return shell;
}

void Shell::LoadURL(const GURL& url) {
  LoadURLForFrame(
      url, std::string(),
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
}

void Shell::LoadURLForFrame(const GURL& url,
                            const std::string& frame_name,
                            ui::PageTransition transition_type) {
  NavigationController::LoadURLParams params(url);
  params.frame_name = frame_name;
  params.transition_type = transition_type;
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void Shell::LoadDataWithBaseURL(const GURL& url, const std::string& data,
    const GURL& base_url) {
  bool load_as_string = false;
  LoadDataWithBaseURLInternal(url, data, base_url, load_as_string);
}

#if defined(OS_ANDROID)
void Shell::LoadDataAsStringWithBaseURL(const GURL& url,
                                        const std::string& data,
                                        const GURL& base_url) {
  bool load_as_string = true;
  LoadDataWithBaseURLInternal(url, data, base_url, load_as_string);
}
#endif

void Shell::LoadDataWithBaseURLInternal(const GURL& url,
                                        const std::string& data,
                                        const GURL& base_url,
                                        bool load_as_string) {
#if !defined(OS_ANDROID)
  DCHECK(!load_as_string);  // Only supported on Android.
#endif

  NavigationController::LoadURLParams params(GURL::EmptyGURL());
  const std::string data_url_header = "data:text/html;charset=utf-8,";
  if (load_as_string) {
    params.url = GURL(data_url_header);
    std::string data_url_as_string = data_url_header + data;
#if defined(OS_ANDROID)
    params.data_url_as_string =
        base::RefCountedString::TakeString(&data_url_as_string);
#endif
  } else {
    params.url = GURL(data_url_header + data);
  }

  params.load_type = NavigationController::LOAD_TYPE_DATA;
  params.base_url_for_data_url = base_url;
  params.virtual_url_for_data_url = url;
  params.override_user_agent = NavigationController::UA_OVERRIDE_FALSE;
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void Shell::AddNewContents(WebContents* source,
                           std::unique_ptr<WebContents> new_contents,
                           WindowOpenDisposition disposition,
                           const gfx::Rect& initial_rect,
                           bool user_gesture,
                           bool* was_blocked) {
  WebContents* raw_new_contents = new_contents.get();
  CreateShell(std::move(new_contents), AdjustWindowSize(initial_rect.size()));
  if (switches::IsRunWebTestsSwitchPresent())
    SecondaryTestWindowObserver::CreateForWebContents(raw_new_contents);
}

void Shell::GoBackOrForward(int offset) {
  web_contents_->GetController().GoToOffset(offset);
  web_contents_->Focus();
}

void Shell::Reload() {
  web_contents_->GetController().Reload(ReloadType::NORMAL, false);
  web_contents_->Focus();
}

void Shell::ReloadBypassingCache() {
  web_contents_->GetController().Reload(ReloadType::BYPASSING_CACHE, false);
  web_contents_->Focus();
}

void Shell::Stop() {
  web_contents_->Stop();
  web_contents_->Focus();
}

void Shell::UpdateNavigationControls(bool to_different_document) {
  int current_index = web_contents_->GetController().GetCurrentEntryIndex();
  int max_index = web_contents_->GetController().GetEntryCount() - 1;

  PlatformEnableUIControl(BACK_BUTTON, current_index > 0);
  PlatformEnableUIControl(FORWARD_BUTTON, current_index < max_index);
  PlatformEnableUIControl(STOP_BUTTON,
      to_different_document && web_contents_->IsLoading());
}

void Shell::ShowDevTools() {
  if (!devtools_frontend_) {
    devtools_frontend_ = ShellDevToolsFrontend::Show(web_contents());
    devtools_observer_.reset(new DevToolsWebContentsObserver(
        this, devtools_frontend_->frontend_shell()->web_contents()));
  }

  devtools_frontend_->Activate();
  devtools_frontend_->Focus();
}

void Shell::CloseDevTools() {
  if (!devtools_frontend_)
    return;
  devtools_observer_.reset();
  devtools_frontend_->Close();
  devtools_frontend_ = nullptr;
}

gfx::NativeView Shell::GetContentView() {
  if (!web_contents_)
    return nullptr;
  return web_contents_->GetNativeView();
}

WebContents* Shell::OpenURLFromTab(WebContents* source,
                                   const OpenURLParams& params) {
  WebContents* target = nullptr;
  switch (params.disposition) {
    case WindowOpenDisposition::CURRENT_TAB:
      target = source;
      break;

    // Normally, the difference between NEW_POPUP and NEW_WINDOW is that a popup
    // should have no toolbar, no status bar, no menu bar, no scrollbars and be
    // not resizable.  For simplicity and to enable new testing scenarios in
    // content shell and layout tests, popups don't get special treatment below
    // (i.e. they will have a toolbar and other things described here).
    case WindowOpenDisposition::NEW_POPUP:
    case WindowOpenDisposition::NEW_WINDOW:
    // content_shell doesn't really support tabs, but some layout tests use
    // middle click (which translates into kNavigationPolicyNewBackgroundTab),
    // so we treat the cases below just like a NEW_WINDOW disposition.
    case WindowOpenDisposition::NEW_BACKGROUND_TAB:
    case WindowOpenDisposition::NEW_FOREGROUND_TAB: {
      Shell* new_window =
          Shell::CreateNewWindow(source->GetBrowserContext(),
                                 GURL(),  // Don't load anything just yet.
                                 params.source_site_instance,
                                 gfx::Size());  // Use default size.
      target = new_window->web_contents();
      if (switches::IsRunWebTestsSwitchPresent())
        SecondaryTestWindowObserver::CreateForWebContents(target);
      break;
    }

    // No tabs in content_shell:
    case WindowOpenDisposition::SINGLETON_TAB:
    // No incognito mode in content_shell:
    case WindowOpenDisposition::OFF_THE_RECORD:
    // TODO(lukasza): Investigate if some layout tests might need support for
    // SAVE_TO_DISK disposition.  This would probably require that
    // BlinkTestController always sets up and cleans up a temporary directory
    // as the default downloads destinations for the duration of a test.
    case WindowOpenDisposition::SAVE_TO_DISK:
    // Ignoring requests with disposition == IGNORE_ACTION...
    case WindowOpenDisposition::IGNORE_ACTION:
    default:
      return nullptr;
  }

  NavigationController::LoadURLParams load_url_params(params.url);
  load_url_params.source_site_instance = params.source_site_instance;
  load_url_params.transition_type = params.transition;
  load_url_params.frame_tree_node_id = params.frame_tree_node_id;
  load_url_params.referrer = params.referrer;
  load_url_params.redirect_chain = params.redirect_chain;
  load_url_params.extra_headers = params.extra_headers;
  load_url_params.is_renderer_initiated = params.is_renderer_initiated;
  load_url_params.should_replace_current_entry =
      params.should_replace_current_entry;
  load_url_params.blob_url_loader_factory = params.blob_url_loader_factory;

  if (params.uses_post) {
    load_url_params.load_type = NavigationController::LOAD_TYPE_HTTP_POST;
    load_url_params.post_data = params.post_data;
  }

  target->GetController().LoadURLWithParams(load_url_params);
  return target;
}

void Shell::LoadingStateChanged(WebContents* source,
    bool to_different_document) {
  UpdateNavigationControls(to_different_document);
  PlatformSetIsLoading(source->IsLoading());
}

void Shell::EnterFullscreenModeForTab(
    WebContents* web_contents,
    const GURL& origin,
    const blink::WebFullscreenOptions& options) {
  ToggleFullscreenModeForTab(web_contents, true);
}

void Shell::ExitFullscreenModeForTab(WebContents* web_contents) {
  ToggleFullscreenModeForTab(web_contents, false);
}

void Shell::ToggleFullscreenModeForTab(WebContents* web_contents,
                                       bool enter_fullscreen) {
#if defined(OS_ANDROID)
  PlatformToggleFullscreenModeForTab(web_contents, enter_fullscreen);
#endif
  if (!switches::IsRunWebTestsSwitchPresent())
    return;
  if (is_fullscreen_ != enter_fullscreen) {
    is_fullscreen_ = enter_fullscreen;
    web_contents->GetRenderViewHost()
        ->GetWidget()
        ->SynchronizeVisualProperties();
  }
}

bool Shell::IsFullscreenForTabOrPending(const WebContents* web_contents) const {
#if defined(OS_ANDROID)
  return PlatformIsFullscreenForTabOrPending(web_contents);
#else
  return is_fullscreen_;
#endif
}

blink::WebDisplayMode Shell::GetDisplayMode(
    const WebContents* web_contents) const {
 // TODO : should return blink::WebDisplayModeFullscreen wherever user puts
 // a browser window into fullscreen (not only in case of renderer-initiated
 // fullscreen mode): crbug.com/476874.
 return IsFullscreenForTabOrPending(web_contents)
            ? blink::kWebDisplayModeFullscreen
            : blink::kWebDisplayModeBrowser;
}

void Shell::RequestToLockMouse(WebContents* web_contents,
                               bool user_gesture,
                               bool last_unlocked_by_target) {
  web_contents->GotResponseToLockMouseRequest(true);
}

void Shell::CloseContents(WebContents* source) {
  Close();
}

bool Shell::CanOverscrollContent() const {
#if defined(USE_AURA)
  return true;
#else
  return false;
#endif
}

void Shell::DidNavigateMainFramePostCommit(WebContents* web_contents) {
  PlatformSetAddressBarURL(web_contents->GetLastCommittedURL());
}

JavaScriptDialogManager* Shell::GetJavaScriptDialogManager(
    WebContents* source) {
  if (!dialog_manager_) {
    dialog_manager_.reset(switches::IsRunWebTestsSwitchPresent()
                              ? new LayoutTestJavaScriptDialogManager
                              : new ShellJavaScriptDialogManager);
  }
  return dialog_manager_.get();
}

std::unique_ptr<BluetoothChooser> Shell::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  BlinkTestController* blink_test_controller = BlinkTestController::Get();
  if (blink_test_controller && switches::IsRunWebTestsSwitchPresent())
    return blink_test_controller->RunBluetoothChooser(frame, event_handler);
  return nullptr;
}

bool Shell::DidAddMessageToConsole(WebContents* source,
                                   int32_t level,
                                   const base::string16& message,
                                   int32_t line_no,
                                   const base::string16& source_id) {
  return switches::IsRunWebTestsSwitchPresent();
}

void Shell::RendererUnresponsive(WebContents* source,
                                 RenderWidgetHost* render_widget_host) {
  BlinkTestController* blink_test_controller = BlinkTestController::Get();
  if (blink_test_controller && switches::IsRunWebTestsSwitchPresent())
    blink_test_controller->RendererUnresponsive();
}

void Shell::ActivateContents(WebContents* contents) {
  contents->GetRenderViewHost()->GetWidget()->Focus();
}

bool Shell::ShouldAllowRunningInsecureContent(
    content::WebContents* web_contents,
    bool allowed_per_prefs,
    const url::Origin& origin,
    const GURL& resource_url) {
  bool allowed_by_test = false;
  BlinkTestController* blink_test_controller = BlinkTestController::Get();
  if (blink_test_controller && switches::IsRunWebTestsSwitchPresent()) {
    const base::DictionaryValue& test_flags =
        blink_test_controller->accumulated_layout_test_runtime_flags_changes();
    test_flags.GetBoolean("running_insecure_content_allowed", &allowed_by_test);
  }

  return allowed_per_prefs || allowed_by_test;
}

gfx::Size Shell::EnterPictureInPicture(const viz::SurfaceId& surface_id,
                                       const gfx::Size& natural_size) {
  // During tests, returning a fake window size to pretent the window was
  // created and allow tests to run accordingly.
  return switches::IsRunWebTestsSwitchPresent() ? gfx::Size(42, 42)
                                                : gfx::Size(0, 0);
}

gfx::Size Shell::GetShellDefaultSize() {
  static gfx::Size default_shell_size;
  if (!default_shell_size.IsEmpty())
    return default_shell_size;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kContentShellHostWindowSize)) {
    const std::string size_str = command_line->GetSwitchValueASCII(
                  switches::kContentShellHostWindowSize);
    int width, height;
    CHECK_EQ(2, sscanf(size_str.c_str(), "%dx%d", &width, &height));
    default_shell_size = gfx::Size(width, height);
  } else {
    default_shell_size = gfx::Size(
      kDefaultTestWindowWidthDip, kDefaultTestWindowHeightDip);
  }
  return default_shell_size;
}

void Shell::TitleWasSet(NavigationEntry* entry) {
  if (entry)
    PlatformSetTitle(entry->GetTitle());
}

void Shell::OnDevToolsWebContentsDestroyed() {
  devtools_observer_.reset();
  devtools_frontend_ = nullptr;
}

}  // namespace content
