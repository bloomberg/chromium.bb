// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/app_window/app_window_api.h"

#include "apps/app_window.h"
#include "apps/app_window_contents.h"
#include "apps/app_window_registry.h"
#include "apps/ui/apps_client.h"
#include "apps/ui/native_app_window.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/app_window.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/image_util.h"
#include "extensions/common/features/simple_feature.h"
#include "extensions/common/permissions/permissions_data.h"
#include "extensions/common/switches.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

using apps::AppWindow;

namespace app_window = extensions::api::app_window;
namespace Create = app_window::Create;

namespace extensions {

namespace app_window_constants {
const char kInvalidWindowId[] =
    "The window id can not be more than 256 characters long.";
const char kInvalidColorSpecification[] =
    "The color specification could not be parsed.";
const char kColorWithFrameNone[] = "Windows with no frame cannot have a color.";
const char kInactiveColorWithoutColor[] =
    "frame.inactiveColor must be used with frame.color.";
const char kConflictingBoundsOptions[] =
    "The $1 property cannot be specified for both inner and outer bounds.";
const char kAlwaysOnTopPermission[] =
    "The \"app.window.alwaysOnTop\" permission is required.";
const char kInvalidUrlParameter[] =
    "The URL used for window creation must be local for security reasons.";
const char kAlphaEnabledWrongChannel[] =
    "The alphaEnabled option requires dev channel or newer.";
const char kAlphaEnabledMissingPermission[] =
    "The alphaEnabled option requires app.window.alpha permission.";
const char kAlphaEnabledNeedsFrameNone[] =
    "The alphaEnabled option can only be used with \"frame: 'none'\".";
}  // namespace app_window_constants

const char kNoneFrameOption[] = "none";
  // TODO(benwells): Remove HTML titlebar injection.
const char kHtmlFrameOption[] = "experimental-html";

namespace {

// Opens an inspector window and delays the response to the
// AppWindowCreateFunction until the DevToolsWindow has finished loading, and is
// ready to stop on breakpoints in the callback.
class DevToolsRestorer : public base::RefCounted<DevToolsRestorer> {
 public:
  DevToolsRestorer(AppWindowCreateFunction* delayed_create_function,
                   content::WebContents* web_contents)
      : delayed_create_function_(delayed_create_function) {
    AddRef();  // Balanced in LoadCompleted.
    DevToolsWindow* devtools_window = DevToolsWindow::OpenDevToolsWindow(
        web_contents,
        DevToolsToggleAction::ShowConsole());
    devtools_window->SetLoadCompletedCallback(
        base::Bind(&DevToolsRestorer::LoadCompleted, this));
  }

 private:
  friend class base::RefCounted<DevToolsRestorer>;
  ~DevToolsRestorer() {}

  void LoadCompleted() {
    delayed_create_function_->SendDelayedResponse();
    Release();
  }

  scoped_refptr<AppWindowCreateFunction> delayed_create_function_;
};

// If the same property is specified for the inner and outer bounds, raise an
// error.
bool CheckBoundsConflict(const scoped_ptr<int>& inner_property,
                         const scoped_ptr<int>& outer_property,
                         const std::string& property_name,
                         std::string* error) {
  if (inner_property.get() && outer_property.get()) {
    std::vector<std::string> subst;
    subst.push_back(property_name);
    *error = ReplaceStringPlaceholders(
        app_window_constants::kConflictingBoundsOptions, subst, NULL);
    return false;
  }

  return true;
}

// Copy over the bounds specification properties from the API to the
// AppWindow::CreateParams.
void CopyBoundsSpec(
    const extensions::api::app_window::BoundsSpecification* input_spec,
    apps::AppWindow::BoundsSpecification* create_spec) {
  if (!input_spec)
    return;

  if (input_spec->left.get())
    create_spec->bounds.set_x(*input_spec->left);
  if (input_spec->top.get())
    create_spec->bounds.set_y(*input_spec->top);
  if (input_spec->width.get())
    create_spec->bounds.set_width(*input_spec->width);
  if (input_spec->height.get())
    create_spec->bounds.set_height(*input_spec->height);
  if (input_spec->min_width.get())
    create_spec->minimum_size.set_width(*input_spec->min_width);
  if (input_spec->min_height.get())
    create_spec->minimum_size.set_height(*input_spec->min_height);
  if (input_spec->max_width.get())
    create_spec->maximum_size.set_width(*input_spec->max_width);
  if (input_spec->max_height.get())
    create_spec->maximum_size.set_height(*input_spec->max_height);
}

}  // namespace

AppWindowCreateFunction::AppWindowCreateFunction()
    : inject_html_titlebar_(false) {}

void AppWindowCreateFunction::SendDelayedResponse() {
  SendResponse(true);
}

bool AppWindowCreateFunction::RunAsync() {
  // Don't create app window if the system is shutting down.
  if (extensions::ExtensionsBrowserClient::Get()->IsShuttingDown())
    return false;

  scoped_ptr<Create::Params> params(Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  GURL url = extension()->GetResourceURL(params->url);
  // Allow absolute URLs for component apps, otherwise prepend the extension
  // path.
  GURL absolute = GURL(params->url);
  if (absolute.has_scheme()) {
    if (extension()->location() == extensions::Manifest::COMPONENT) {
      url = absolute;
    } else {
      // Show error when url passed isn't local.
      error_ = app_window_constants::kInvalidUrlParameter;
      return false;
    }
  }

  // TODO(jeremya): figure out a way to pass the opening WebContents through to
  // AppWindow::Create so we can set the opener at create time rather than
  // with a hack in AppWindowCustomBindings::GetView().
  AppWindow::CreateParams create_params;
  app_window::CreateWindowOptions* options = params->options.get();
  if (options) {
    if (options->id.get()) {
      // TODO(mek): use URL if no id specified?
      // Limit length of id to 256 characters.
      if (options->id->length() > 256) {
        error_ = app_window_constants::kInvalidWindowId;
        return false;
      }

      create_params.window_key = *options->id;

      if (options->singleton && *options->singleton == false) {
        WriteToConsole(
          content::CONSOLE_MESSAGE_LEVEL_WARNING,
          "The 'singleton' option in chrome.apps.window.create() is deprecated!"
          " Change your code to no longer rely on this.");
      }

      if (!options->singleton || *options->singleton) {
        AppWindow* window = apps::AppWindowRegistry::Get(browser_context())
                                ->GetAppWindowForAppAndKey(
                                    extension_id(), create_params.window_key);
        if (window) {
          content::RenderViewHost* created_view =
              window->web_contents()->GetRenderViewHost();
          int view_id = MSG_ROUTING_NONE;
          if (render_view_host_->GetProcess()->GetID() ==
              created_view->GetProcess()->GetID()) {
            view_id = created_view->GetRoutingID();
          }

          if (options->hidden.get() && !*options->hidden.get()) {
            if (options->focused.get() && !*options->focused.get())
              window->Show(AppWindow::SHOW_INACTIVE);
            else
              window->Show(AppWindow::SHOW_ACTIVE);
          }

          base::DictionaryValue* result = new base::DictionaryValue;
          result->Set("viewId", new base::FundamentalValue(view_id));
          window->GetSerializedState(result);
          result->SetBoolean("existingWindow", true);
          // TODO(benwells): Remove HTML titlebar injection.
          result->SetBoolean("injectTitlebar", false);
          SetResult(result);
          SendResponse(true);
          return true;
        }
      }
    }

    if (!GetBoundsSpec(*options, &create_params, &error_))
      return false;

    if (GetCurrentChannel() <= chrome::VersionInfo::CHANNEL_DEV ||
        extension()->location() == extensions::Manifest::COMPONENT) {
      if (options->type == extensions::api::app_window::WINDOW_TYPE_PANEL) {
        create_params.window_type = AppWindow::WINDOW_TYPE_PANEL;
      }
    }

    if (!GetFrameOptions(*options, &create_params))
      return false;

    if (options->alpha_enabled.get()) {
      const char* whitelist[] = {
        "0F42756099D914A026DADFA182871C015735DD95",  // http://crbug.com/323773
        "2D22CDB6583FD0A13758AEBE8B15E45208B4E9A7",
        "E7E2461CE072DF036CF9592740196159E2D7C089",  // http://crbug.com/356200
        "A74A4D44C7CFCD8844830E6140C8D763E12DD8F3",
        "312745D9BF916161191143F6490085EEA0434997",
        "53041A2FA309EECED01FFC751E7399186E860B2C"
      };
      if (GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV &&
          !extensions::SimpleFeature::IsIdInList(
              extension_id(),
              std::set<std::string>(whitelist,
                                    whitelist + arraysize(whitelist)))) {
        error_ = app_window_constants::kAlphaEnabledWrongChannel;
        return false;
      }
      if (!extension()->permissions_data()->HasAPIPermission(
              APIPermission::kAlphaEnabled)) {
        error_ = app_window_constants::kAlphaEnabledMissingPermission;
        return false;
      }
      if (create_params.frame != AppWindow::FRAME_NONE) {
        error_ = app_window_constants::kAlphaEnabledNeedsFrameNone;
        return false;
      }
#if defined(USE_AURA)
      create_params.alpha_enabled = *options->alpha_enabled;
#else
      // Transparency is only supported on Aura.
      // Fallback to creating an opaque window (by ignoring alphaEnabled).
#endif
    }

    if (options->hidden.get())
      create_params.hidden = *options->hidden.get();

    if (options->resizable.get())
      create_params.resizable = *options->resizable.get();

    if (options->always_on_top.get()) {
      create_params.always_on_top = *options->always_on_top.get();

      if (create_params.always_on_top &&
          !extension()->permissions_data()->HasAPIPermission(
              APIPermission::kAlwaysOnTopWindows)) {
        error_ = app_window_constants::kAlwaysOnTopPermission;
        return false;
      }
    }

    if (options->focused.get())
      create_params.focused = *options->focused.get();

    if (options->type != extensions::api::app_window::WINDOW_TYPE_PANEL) {
      switch (options->state) {
        case extensions::api::app_window::STATE_NONE:
        case extensions::api::app_window::STATE_NORMAL:
          break;
        case extensions::api::app_window::STATE_FULLSCREEN:
          create_params.state = ui::SHOW_STATE_FULLSCREEN;
          break;
        case extensions::api::app_window::STATE_MAXIMIZED:
          create_params.state = ui::SHOW_STATE_MAXIMIZED;
          break;
        case extensions::api::app_window::STATE_MINIMIZED:
          create_params.state = ui::SHOW_STATE_MINIMIZED;
          break;
      }
    }
  }

  create_params.creator_process_id =
      render_view_host_->GetProcess()->GetID();

  AppWindow* app_window =
      apps::AppsClient::Get()->CreateAppWindow(browser_context(), extension());
  app_window->Init(
      url, new apps::AppWindowContentsImpl(app_window), create_params);

  if (ExtensionsBrowserClient::Get()->IsRunningInForcedAppMode())
    app_window->ForcedFullscreen();

  content::RenderViewHost* created_view =
      app_window->web_contents()->GetRenderViewHost();
  int view_id = MSG_ROUTING_NONE;
  if (create_params.creator_process_id == created_view->GetProcess()->GetID())
    view_id = created_view->GetRoutingID();

  base::DictionaryValue* result = new base::DictionaryValue;
  result->Set("viewId", new base::FundamentalValue(view_id));
  result->Set("injectTitlebar",
      new base::FundamentalValue(inject_html_titlebar_));
  result->Set("id", new base::StringValue(app_window->window_key()));
  app_window->GetSerializedState(result);
  SetResult(result);

  if (apps::AppWindowRegistry::Get(browser_context())
          ->HadDevToolsAttached(created_view)) {
    new DevToolsRestorer(this, app_window->web_contents());
    return true;
  }

  SendResponse(true);
  app_window->WindowEventsReady();

  return true;
}

bool AppWindowCreateFunction::GetBoundsSpec(
    const extensions::api::app_window::CreateWindowOptions& options,
    apps::AppWindow::CreateParams* params,
    std::string* error) {
  DCHECK(params);
  DCHECK(error);

  if (options.inner_bounds.get() || options.outer_bounds.get()) {
    // Parse the inner and outer bounds specifications. If developers use the
    // new API, the deprecated fields will be ignored - do not attempt to merge
    // them.

    const extensions::api::app_window::BoundsSpecification* inner_bounds =
        options.inner_bounds.get();
    const extensions::api::app_window::BoundsSpecification* outer_bounds =
        options.outer_bounds.get();
    if (inner_bounds && outer_bounds) {
      if (!CheckBoundsConflict(
               inner_bounds->left, outer_bounds->left, "left", error)) {
        return false;
      }
      if (!CheckBoundsConflict(
               inner_bounds->top, outer_bounds->top, "top", error)) {
        return false;
      }
      if (!CheckBoundsConflict(
               inner_bounds->width, outer_bounds->width, "width", error)) {
        return false;
      }
      if (!CheckBoundsConflict(
               inner_bounds->height, outer_bounds->height, "height", error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->min_width,
                               outer_bounds->min_width,
                               "minWidth",
                               error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->min_height,
                               outer_bounds->min_height,
                               "minHeight",
                               error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->max_width,
                               outer_bounds->max_width,
                               "maxWidth",
                               error)) {
        return false;
      }
      if (!CheckBoundsConflict(inner_bounds->max_height,
                               outer_bounds->max_height,
                               "maxHeight",
                               error)) {
        return false;
      }
    }

    CopyBoundsSpec(inner_bounds, &(params->content_spec));
    CopyBoundsSpec(outer_bounds, &(params->window_spec));
  } else {
    // Parse deprecated fields.
    // Due to a bug in NativeAppWindow::GetFrameInsets() on Windows and ChromeOS
    // the bounds set the position of the window and the size of the content.
    // This will be preserved as apps may be relying on this behavior.

    if (options.default_width.get())
      params->content_spec.bounds.set_width(*options.default_width.get());
    if (options.default_height.get())
      params->content_spec.bounds.set_height(*options.default_height.get());
    if (options.default_left.get())
      params->window_spec.bounds.set_x(*options.default_left.get());
    if (options.default_top.get())
      params->window_spec.bounds.set_y(*options.default_top.get());

    if (options.width.get())
      params->content_spec.bounds.set_width(*options.width.get());
    if (options.height.get())
      params->content_spec.bounds.set_height(*options.height.get());
    if (options.left.get())
      params->window_spec.bounds.set_x(*options.left.get());
    if (options.top.get())
      params->window_spec.bounds.set_y(*options.top.get());

    if (options.bounds.get()) {
      app_window::ContentBounds* bounds = options.bounds.get();
      if (bounds->width.get())
        params->content_spec.bounds.set_width(*bounds->width.get());
      if (bounds->height.get())
        params->content_spec.bounds.set_height(*bounds->height.get());
      if (bounds->left.get())
        params->window_spec.bounds.set_x(*bounds->left.get());
      if (bounds->top.get())
        params->window_spec.bounds.set_y(*bounds->top.get());
    }

    gfx::Size& minimum_size = params->content_spec.minimum_size;
    if (options.min_width.get())
      minimum_size.set_width(*options.min_width);
    if (options.min_height.get())
      minimum_size.set_height(*options.min_height);
    gfx::Size& maximum_size = params->content_spec.maximum_size;
    if (options.max_width.get())
      maximum_size.set_width(*options.max_width);
    if (options.max_height.get())
      maximum_size.set_height(*options.max_height);
  }

  return true;
}

AppWindow::Frame AppWindowCreateFunction::GetFrameFromString(
    const std::string& frame_string) {
  if (frame_string == kHtmlFrameOption &&
      (extension()->permissions_data()->HasAPIPermission(
           APIPermission::kExperimental) ||
       CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kEnableExperimentalExtensionApis))) {
     inject_html_titlebar_ = true;
     return AppWindow::FRAME_NONE;
   }

   if (frame_string == kNoneFrameOption)
    return AppWindow::FRAME_NONE;

  return AppWindow::FRAME_CHROME;
}

bool AppWindowCreateFunction::GetFrameOptions(
    const app_window::CreateWindowOptions& options,
    AppWindow::CreateParams* create_params) {
  if (!options.frame)
    return true;

  DCHECK(options.frame->as_string || options.frame->as_frame_options);
  if (options.frame->as_string) {
    create_params->frame = GetFrameFromString(*options.frame->as_string);
    return true;
  }

  if (options.frame->as_frame_options->type)
    create_params->frame =
        GetFrameFromString(*options.frame->as_frame_options->type);

  if (options.frame->as_frame_options->color.get()) {
    if (create_params->frame != AppWindow::FRAME_CHROME) {
      error_ = app_window_constants::kColorWithFrameNone;
      return false;
    }

    if (!image_util::ParseCSSColorString(
            *options.frame->as_frame_options->color,
            &create_params->active_frame_color)) {
      error_ = app_window_constants::kInvalidColorSpecification;
      return false;
    }

    create_params->has_frame_color = true;
    create_params->inactive_frame_color = create_params->active_frame_color;

    if (options.frame->as_frame_options->inactive_color.get()) {
      if (!image_util::ParseCSSColorString(
              *options.frame->as_frame_options->inactive_color,
              &create_params->inactive_frame_color)) {
        error_ = app_window_constants::kInvalidColorSpecification;
        return false;
      }
    }

    return true;
  }

  if (options.frame->as_frame_options->inactive_color.get()) {
    error_ = app_window_constants::kInactiveColorWithoutColor;
    return false;
  }

  return true;
}

}  // namespace extensions
