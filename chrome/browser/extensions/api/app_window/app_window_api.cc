// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/app_window/app_window_api.h"

#include "base/command_line.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/extensions/window_controller.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/app_window.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#endif

namespace app_window = extensions::api::app_window;
namespace Create = app_window::Create;

namespace extensions {

namespace app_window_constants {
const char kInvalidWindowId[] =
    "The window id can not be more than 256 characters long.";
}

const char kNoneFrameOption[] = "none";
const char kHtmlFrameOption[] = "experimental-html";

namespace {

// Opens an inspector window and delays the response to the
// AppWindowCreateFunction until the DevToolsWindow has finished loading, and is
// ready to stop on breakpoints in the callback.
class DevToolsRestorer : public content::NotificationObserver {
 public:
  DevToolsRestorer(AppWindowCreateFunction* delayed_create_function,
                   content::RenderViewHost* created_view)
      : delayed_create_function_(delayed_create_function) {
    DevToolsWindow* devtools_window =
        DevToolsWindow::ToggleDevToolsWindow(
            created_view,
            true /* force_open */,
            DEVTOOLS_TOGGLE_ACTION_SHOW_CONSOLE);

    registrar_.Add(
        this,
        content::NOTIFICATION_LOAD_STOP,
        content::Source<content::NavigationController>(
            &devtools_window->web_contents()->GetController()));
  }

 protected:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK(type == content::NOTIFICATION_LOAD_STOP);
    delayed_create_function_->SendDelayedResponse();
    delete this;
  }

 private:
  scoped_refptr<AppWindowCreateFunction> delayed_create_function_;
  content::NotificationRegistrar registrar_;
};

}  // namespace

void AppWindowCreateFunction::SendDelayedResponse() {
  SendResponse(true);
}

bool AppWindowCreateFunction::RunImpl() {
  scoped_ptr<Create::Params> params(Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url = GetExtension()->GetResourceURL(params->url);
  // Allow absolute URLs for component apps, otherwise prepend the extension
  // path.
  if (GetExtension()->location() == extensions::Manifest::COMPONENT) {
    GURL absolute = GURL(params->url);
    if (absolute.has_scheme())
      url = absolute;
  }

  bool inject_html_titlebar = false;

  // TODO(jeremya): figure out a way to pass the opening WebContents through to
  // ShellWindow::Create so we can set the opener at create time rather than
  // with a hack in AppWindowCustomBindings::GetView().
  ShellWindow::CreateParams create_params;
  app_window::CreateWindowOptions* options = params->options.get();
#if defined(USE_ASH)
  bool force_maximize = ash::Shell::IsForcedMaximizeMode();
#else
  bool force_maximize = false;
#endif
  if (options) {
    if (options->id.get()) {
      // TODO(mek): use URL if no id specified?
      // Limit length of id to 256 characters.
      if (options->id->length() > 256) {
        error_ = app_window_constants::kInvalidWindowId;
        return false;
      }

      create_params.window_key = *options->id;

      if (!options->singleton || *options->singleton) {
        ShellWindow* window = ShellWindowRegistry::Get(profile())->
            GetShellWindowForAppAndKey(extension_id(),
                                       create_params.window_key);
        if (window) {
          content::RenderViewHost* created_view =
              window->web_contents()->GetRenderViewHost();
          int view_id = MSG_ROUTING_NONE;
          if (render_view_host_->GetProcess()->GetID() ==
              created_view->GetProcess()->GetID()) {
            view_id = created_view->GetRoutingID();
          }

          window->GetBaseWindow()->Show();
          base::DictionaryValue* result = new base::DictionaryValue;
          result->Set("viewId", base::Value::CreateIntegerValue(view_id));
          result->SetBoolean("existingWindow", true);
          result->SetBoolean("injectTitlebar", false);
          SetResult(result);
          SendResponse(true);
          return true;
        }
      }
    }

    // TODO(jeremya): remove these, since they do the same thing as
    // left/top/width/height.
    if (options->default_width.get())
      create_params.bounds.set_width(*options->default_width.get());
    if (options->default_height.get())
      create_params.bounds.set_height(*options->default_height.get());
    if (options->default_left.get())
      create_params.bounds.set_x(*options->default_left.get());
    if (options->default_top.get())
      create_params.bounds.set_y(*options->default_top.get());

    if (options->width.get())
      create_params.bounds.set_width(*options->width.get());
    if (options->height.get())
      create_params.bounds.set_height(*options->height.get());
    if (options->left.get())
      create_params.bounds.set_x(*options->left.get());
    if (options->top.get())
      create_params.bounds.set_y(*options->top.get());

    if (options->bounds.get()) {
      app_window::Bounds* bounds = options->bounds.get();
      if (bounds->width.get())
        create_params.bounds.set_width(*bounds->width.get());
      if (bounds->height.get())
        create_params.bounds.set_height(*bounds->height.get());
      if (bounds->left.get())
        create_params.bounds.set_x(*bounds->left.get());
      if (bounds->top.get())
        create_params.bounds.set_y(*bounds->top.get());
    }

    if (CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableExperimentalExtensionApis)) {
      if (options->type == extensions::api::app_window::WINDOW_TYPE_PANEL) {
          create_params.window_type = ShellWindow::WINDOW_TYPE_PANEL;
      }
    }

    if (options->frame.get()) {
      if (*options->frame == kHtmlFrameOption &&
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kEnableExperimentalExtensionApis)) {
        create_params.frame = ShellWindow::FRAME_NONE;
        inject_html_titlebar = true;
      } else if (*options->frame == kNoneFrameOption) {
        create_params.frame = ShellWindow::FRAME_NONE;
      } else {
        create_params.frame = ShellWindow::FRAME_CHROME;
      }
    }

    if (options->transparent_background.get() &&
        CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableExperimentalExtensionApis)) {
      create_params.transparent_background = *options->transparent_background;
    }

    gfx::Size& minimum_size = create_params.minimum_size;
    if (options->min_width.get())
      minimum_size.set_width(*options->min_width);
    if (options->min_height.get())
      minimum_size.set_height(*options->min_height);
    gfx::Size& maximum_size = create_params.maximum_size;
    if (options->max_width.get())
      maximum_size.set_width(*options->max_width);
    if (options->max_height.get())
      maximum_size.set_height(*options->max_height);

    if (options->hidden.get())
      create_params.hidden = *options->hidden.get();

    if (options->resizable.get())
      create_params.resizable = *options->resizable.get();

    if (options->type != extensions::api::app_window::WINDOW_TYPE_PANEL) {
      switch (options->state) {
        case extensions::api::app_window::STATE_NONE:
        case extensions::api::app_window::STATE_NORMAL:
          break;
        case extensions::api::app_window::STATE_FULLSCREEN:
          create_params.state = ShellWindow::CreateParams::STATE_FULLSCREEN;
          break;
        case extensions::api::app_window::STATE_MAXIMIZED:
          create_params.state = ShellWindow::CreateParams::STATE_MAXIMIZED;
          break;
        case extensions::api::app_window::STATE_MINIMIZED:
          create_params.state = ShellWindow::CreateParams::STATE_MINIMIZED;
          break;
      }
    } else {
      force_maximize = false;
    }
  }

  create_params.creator_process_id =
      render_view_host_->GetProcess()->GetID();

  // Rather then maximizing the window after it was created, we maximize it
  // immediately - that way the initial presentation is much smoother (no odd
  // rectangles are shown temporarily in the added space). Note that suppressing
  // animations does not help to remove the shown artifacts.
#if USE_ASH
  if (force_maximize && !create_params.maximum_size.IsEmpty()) {
    // Check that the application is able to fill the monitor - if not don't
    // maximize.
    // TODO(skuhne): In case of multi monitor usage we should find out in
    // advance on which monitor the window will be displayed (or be happy with
    // a temporary bad frame upon creation).
    gfx::Size size = ash::Shell::GetPrimaryRootWindow()->bounds().size();
    if (size.width() > create_params.maximum_size.width() ||
        size.height() > create_params.maximum_size.height())
      force_maximize = false;
  }
 #endif

  if (force_maximize)
    create_params.state = ShellWindow::CreateParams::STATE_MAXIMIZED;

  ShellWindow* shell_window =
      ShellWindow::Create(profile(), GetExtension(), url, create_params);

  if (chrome::ShouldForceFullscreenApp())
    shell_window->Fullscreen();

  content::RenderViewHost* created_view =
      shell_window->web_contents()->GetRenderViewHost();
  int view_id = MSG_ROUTING_NONE;
  if (create_params.creator_process_id == created_view->GetProcess()->GetID())
    view_id = created_view->GetRoutingID();

  base::DictionaryValue* result = new base::DictionaryValue;
  result->Set("viewId", base::Value::CreateIntegerValue(view_id));
  result->Set("injectTitlebar",
      base::Value::CreateBooleanValue(inject_html_titlebar));
  result->Set("id", base::Value::CreateStringValue(shell_window->window_key()));
  DictionaryValue* boundsValue = new DictionaryValue();
  gfx::Rect bounds = shell_window->GetClientBounds();
  boundsValue->SetInteger("left", bounds.x());
  boundsValue->SetInteger("top", bounds.y());
  boundsValue->SetInteger("width", bounds.width());
  boundsValue->SetInteger("height", bounds.height());
  result->Set("bounds", boundsValue);
  SetResult(result);

  if (ShellWindowRegistry::Get(profile())->HadDevToolsAttached(created_view)) {
    new DevToolsRestorer(this, created_view);
    return true;
  }

  SendResponse(true);
  return true;
}

}  // namespace extensions
