// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/renderer_preferences.h"
#include "content/shell/browser/notify_done_forwarder.h"
#include "content/shell/browser/shell_browser_main_parts.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "content/shell/browser/shell_devtools_frontend.h"
#include "content/shell/browser/shell_javascript_dialog_manager.h"
#include "content/shell/browser/webkit_test_controller.h"
#include "content/shell/common/shell_messages.h"
#include "content/shell/common/shell_switches.h"

namespace content {

const int Shell::kDefaultTestWindowWidthDip = 800;
const int Shell::kDefaultTestWindowHeightDip = 600;

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
  virtual void WebContentsDestroyed() OVERRIDE {
    shell_->OnDevToolsWebContentsDestroyed();
  }

 private:
  Shell* shell_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsWebContentsObserver);
};

Shell::Shell(WebContents* web_contents)
    : WebContentsObserver(web_contents),
      devtools_frontend_(NULL),
      is_fullscreen_(false),
      window_(NULL),
      url_edit_view_(NULL),
      headless_(false) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kDumpRenderTree))
    headless_ = true;
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
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
  }
}

Shell* Shell::CreateShell(WebContents* web_contents,
                          const gfx::Size& initial_size) {
  Shell* shell = new Shell(web_contents);
  shell->PlatformCreateWindow(initial_size.width(), initial_size.height());

  shell->web_contents_.reset(web_contents);
  web_contents->SetDelegate(shell);

  shell->PlatformSetContents();

  shell->PlatformResizeSubViews();

  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree)) {
    web_contents->GetMutableRendererPrefs()->use_custom_colors = false;
    web_contents->GetRenderViewHost()->SyncRendererPrefs();
  }

  return shell;
}

void Shell::CloseAllWindows() {
  base::AutoReset<bool> auto_reset(&quit_message_loop_, false);
  DevToolsAgentHost::DetachAllClients();
  std::vector<Shell*> open_windows(windows_);
  for (size_t i = 0; i < open_windows.size(); ++i)
    open_windows[i]->Close();
  PlatformExit();
  base::MessageLoop::current()->RunUntilIdle();
}

void Shell::SetShellCreatedCallback(
    base::Callback<void(Shell*)> shell_created_callback) {
  DCHECK(shell_created_callback_.is_null());
  shell_created_callback_ = shell_created_callback;
}

Shell* Shell::FromRenderViewHost(RenderViewHost* rvh) {
  for (size_t i = 0; i < windows_.size(); ++i) {
    if (windows_[i]->web_contents() &&
        windows_[i]->web_contents()->GetRenderViewHost() == rvh) {
      return windows_[i];
    }
  }
  return NULL;
}

// static
void Shell::Initialize() {
  PlatformInitialize(
      gfx::Size(kDefaultTestWindowWidthDip, kDefaultTestWindowHeightDip));
}

gfx::Size Shell::AdjustWindowSize(const gfx::Size& initial_size) {
  if (!initial_size.IsEmpty())
    return initial_size;
  return gfx::Size(kDefaultTestWindowWidthDip, kDefaultTestWindowHeightDip);
}

Shell* Shell::CreateNewWindow(BrowserContext* browser_context,
                              const GURL& url,
                              SiteInstance* site_instance,
                              int routing_id,
                              const gfx::Size& initial_size) {
  WebContents::CreateParams create_params(browser_context, site_instance);
  create_params.routing_id = routing_id;
  create_params.initial_size = AdjustWindowSize(initial_size);
  WebContents* web_contents = WebContents::Create(create_params);
  Shell* shell = CreateShell(web_contents, create_params.initial_size);
  if (!url.is_empty())
    shell->LoadURL(url);
  return shell;
}

void Shell::LoadURL(const GURL& url) {
  LoadURLForFrame(url, std::string());
}

void Shell::LoadURLForFrame(const GURL& url, const std::string& frame_name) {
  NavigationController::LoadURLParams params(url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  params.frame_name = frame_name;
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void Shell::LoadDataWithBaseURL(const GURL& url, const std::string& data,
    const GURL& base_url) {
  const GURL data_url = GURL("data:text/html;charset=utf-8," + data);
  NavigationController::LoadURLParams params(data_url);
  params.load_type = NavigationController::LOAD_TYPE_DATA;
  params.base_url_for_data_url = base_url;
  params.virtual_url_for_data_url = url;
  params.override_user_agent = NavigationController::UA_OVERRIDE_FALSE;
  web_contents_->GetController().LoadURLWithParams(params);
  web_contents_->Focus();
}

void Shell::AddNewContents(WebContents* source,
                           WebContents* new_contents,
                           WindowOpenDisposition disposition,
                           const gfx::Rect& initial_pos,
                           bool user_gesture,
                           bool* was_blocked) {
  CreateShell(new_contents, AdjustWindowSize(initial_pos.size()));
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    NotifyDoneForwarder::CreateForWebContents(new_contents);
}

void Shell::GoBackOrForward(int offset) {
  web_contents_->GetController().GoToOffset(offset);
  web_contents_->Focus();
}

void Shell::Reload() {
  web_contents_->GetController().Reload(false);
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
  InnerShowDevTools("", "");
}

void Shell::ShowDevToolsForElementAt(int x, int y) {
  InnerShowDevTools("", "");
  devtools_frontend_->InspectElementAt(x, y);
}

void Shell::ShowDevToolsForTest(const std::string& settings,
                                const std::string& frontend_url) {
  InnerShowDevTools(settings, frontend_url);
}

void Shell::CloseDevTools() {
  if (!devtools_frontend_)
    return;
  devtools_observer_.reset();
  devtools_frontend_->Close();
  devtools_frontend_ = NULL;
}

gfx::NativeView Shell::GetContentView() {
  if (!web_contents_)
    return NULL;
  return web_contents_->GetNativeView();
}

WebContents* Shell::OpenURLFromTab(WebContents* source,
                                   const OpenURLParams& params) {
  // CURRENT_TAB is the only one we implement for now.
  if (params.disposition != CURRENT_TAB)
      return NULL;
  NavigationController::LoadURLParams load_url_params(params.url);
  load_url_params.referrer = params.referrer;
  load_url_params.frame_tree_node_id = params.frame_tree_node_id;
  load_url_params.transition_type = params.transition;
  load_url_params.extra_headers = params.extra_headers;
  load_url_params.should_replace_current_entry =
      params.should_replace_current_entry;

  if (params.transferred_global_request_id != GlobalRequestID()) {
    load_url_params.is_renderer_initiated = params.is_renderer_initiated;
    load_url_params.transferred_global_request_id =
        params.transferred_global_request_id;
  } else if (params.is_renderer_initiated) {
    load_url_params.is_renderer_initiated = true;
  }

  source->GetController().LoadURLWithParams(load_url_params);
  return source;
}

void Shell::LoadingStateChanged(WebContents* source,
    bool to_different_document) {
  UpdateNavigationControls(to_different_document);
  PlatformSetIsLoading(source->IsLoading());
}

void Shell::ToggleFullscreenModeForTab(WebContents* web_contents,
                                       bool enter_fullscreen) {
#if defined(OS_ANDROID)
  PlatformToggleFullscreenModeForTab(web_contents, enter_fullscreen);
#endif
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  if (is_fullscreen_ != enter_fullscreen) {
    is_fullscreen_ = enter_fullscreen;
    web_contents->GetRenderViewHost()->WasResized();
  }
}

bool Shell::IsFullscreenForTabOrPending(const WebContents* web_contents) const {
#if defined(OS_ANDROID)
  return PlatformIsFullscreenForTabOrPending(web_contents);
#else
  return is_fullscreen_;
#endif
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

JavaScriptDialogManager* Shell::GetJavaScriptDialogManager() {
  if (!dialog_manager_)
    dialog_manager_.reset(new ShellJavaScriptDialogManager());
  return dialog_manager_.get();
}

bool Shell::AddMessageToConsole(WebContents* source,
                                int32 level,
                                const base::string16& message,
                                int32 line_no,
                                const base::string16& source_id) {
  return CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree);
}

void Shell::RendererUnresponsive(WebContents* source) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  WebKitTestController::Get()->RendererUnresponsive();
}

void Shell::ActivateContents(WebContents* contents) {
  contents->GetRenderViewHost()->Focus();
}

void Shell::DeactivateContents(WebContents* contents) {
  contents->GetRenderViewHost()->Blur();
}

void Shell::WorkerCrashed(WebContents* source) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    return;
  WebKitTestController::Get()->WorkerCrashed();
}

bool Shell::HandleContextMenu(const content::ContextMenuParams& params) {
  return PlatformHandleContextMenu(params);
}

void Shell::WebContentsFocused(WebContents* contents) {
#if defined(TOOLKIT_VIEWS)
  PlatformWebContentsFocused(contents);
#endif
}

void Shell::TitleWasSet(NavigationEntry* entry, bool explicit_set) {
  if (entry)
    PlatformSetTitle(entry->GetTitle());
}

void Shell::InnerShowDevTools(const std::string& settings,
                              const std::string& frontend_url) {
  if (!devtools_frontend_) {
    devtools_frontend_ = ShellDevToolsFrontend::Show(
        web_contents(), settings, frontend_url);
    devtools_observer_.reset(new DevToolsWebContentsObserver(
        this, devtools_frontend_->frontend_shell()->web_contents()));
  }

  devtools_frontend_->Activate();
  devtools_frontend_->Focus();
}

void Shell::OnDevToolsWebContentsDestroyed() {
  devtools_observer_.reset();
  devtools_frontend_ = NULL;
}

}  // namespace content
